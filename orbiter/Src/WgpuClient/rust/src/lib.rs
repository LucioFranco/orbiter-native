mod particles;
use std::{cell::RefCell, collections::HashMap, ffi::{c_char, CString}, panic::{catch_unwind, AssertUnwindSafe}, ptr};
use wgpu::util::DeviceExt;
mod present;
mod draw;
mod mesh;
mod dds;
mod stars;
mod atmosphere;
mod background;
mod ui;
use present::Presentation;

thread_local! { static ERROR: RefCell<CString> = RefCell::new(CString::new("").unwrap()); }
fn fail(message: impl ToString) {
    ERROR.with(|e| *e.borrow_mut() = CString::new(message.to_string().replace('\0', " ")).unwrap());
}
// Panics must never unwind into C++. Recoverable errors use explicit return values.
fn boundary<T: Default>(f: impl FnOnce() -> Result<T, String>) -> T {
    match catch_unwind(AssertUnwindSafe(f)) {
        Ok(Ok(value)) => value,
        Ok(Err(error)) => { fail(error); T::default() },
        Err(_) => { fail("Rust renderer panic"); T::default() },
    }
}
#[no_mangle]
pub extern "C" fn orb_gpu_error() -> *const c_char { ERROR.with(|e| e.borrow().as_ptr()) }

struct Surface {
    texture: wgpu::Texture,
    width: u32,
    height: u32,
    // Allocate only for surfaces used as 3D render targets, and release with them.
    depth: std::sync::OnceLock<wgpu::TextureView>,
}
impl Surface {
    fn depth_view(&self, device: &wgpu::Device) -> &wgpu::TextureView {
        self.depth.get_or_init(|| {
            device.create_texture(&wgpu::TextureDescriptor {
                label:Some("Reversed Z depth"), size:self.texture.size(),
                mip_level_count:1, sample_count:1, dimension:wgpu::TextureDimension::D2,
                format:wgpu::TextureFormat::Depth32Float,
                usage:wgpu::TextureUsages::RENDER_ATTACHMENT|wgpu::TextureUsages::TEXTURE_BINDING,
                view_formats:&[],
            }).create_view(&Default::default())
        })
    }
}
pub struct OrbGpu {
    background:background::Background,
    star_image:background::Background,
    instance: wgpu::Instance,
    adapter: wgpu::Adapter,
    presentation: Option<Presentation>,
    device: wgpu::Device,
    queue: wgpu::Queue,
    fill: wgpu::RenderPipeline,
    draw: draw::DrawState,
    pending: Vec<(u64, draw::OrbDraw)>,
    surfaces: HashMap<u64, Surface>,
    next_id: u64,
    next_mesh: u64,
    meshes: HashMap<u64,mesh::Mesh>,
    stars: stars::Stars,
    atmosphere: atmosphere::Atmosphere,
    ui: ui::Ui,
    particles: particles::Particles,
    mesh_pipeline: wgpu::RenderPipeline,
    mesh_sampler: wgpu::Sampler,
    shadow_pipeline: wgpu::RenderPipeline,
    shadow_targets: [Option<mesh::ShadowTarget>;2],
    mesh_alpha_pipeline: wgpu::RenderPipeline,
    mesh_clamp_sampler: wgpu::Sampler,
    mesh_white: wgpu::Texture,
}
#[repr(C)]
pub struct OrbFill { x: i32, y: i32, width: u32, height: u32, rgba: [u8; 4] }
#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct Vertex { position: [f32; 2], color: [f32; 4] }

impl OrbGpu {
    async fn new() -> Result<Self, String> {
        let instance = wgpu::Instance::new(&wgpu::InstanceDescriptor::default());
        let adapter = instance.request_adapter(&wgpu::RequestAdapterOptions::default()).await
            .ok_or("No wgpu adapter available")?;
        let (device, queue) = adapter.request_device(&wgpu::DeviceDescriptor {
            label: Some("Orbiter device"), required_features: adapter.features() & wgpu::Features::TEXTURE_COMPRESSION_BC,
            required_limits: wgpu::Limits::default(), memory_hints: wgpu::MemoryHints::default(),
        }, None).await.map_err(|e| e.to_string())?;
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Batched surface fill"), source: wgpu::ShaderSource::Wgsl(include_str!("fill.wgsl").into()),
        });
        let attributes = wgpu::vertex_attr_array![0 => Float32x2, 1 => Float32x4];
        let fill = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Surface fill"), layout: None,
            vertex: wgpu::VertexState { module: &shader, entry_point: Some("vs"),
                compilation_options: Default::default(), buffers: &[wgpu::VertexBufferLayout {
                    array_stride: std::mem::size_of::<Vertex>() as u64,
                    step_mode: wgpu::VertexStepMode::Vertex, attributes: &attributes,
                }] },
            fragment: Some(wgpu::FragmentState { module: &shader, entry_point: Some("fs"),
                compilation_options: Default::default(), targets: &[Some(wgpu::ColorTargetState {
                    format: wgpu::TextureFormat::Rgba8Unorm, blend: None, write_mask: wgpu::ColorWrites::ALL,
                })] }),
            primitive: Default::default(), depth_stencil: None, multisample: Default::default(),
            multiview: None, cache: None,
        });
        let stars=stars::Stars::new(&device);
        let atmosphere=atmosphere::Atmosphere::new(&device);
        let background=background::Background::new(&device,false);
        let star_image=background::Background::new(&device,true);
        let ui=ui::Ui::new(&device);
        let particles=particles::Particles::new(&device);
        let (mesh_pipeline,mesh_alpha_pipeline)=mesh::pipeline(&device);
        let shadow_pipeline=mesh::shadow_pipeline(&device);
        let mesh_sampler=device.create_sampler(&wgpu::SamplerDescriptor {
            mag_filter:wgpu::FilterMode::Linear,min_filter:wgpu::FilterMode::Linear,mipmap_filter:wgpu::FilterMode::Linear,
            address_mode_u:wgpu::AddressMode::Repeat,address_mode_v:wgpu::AddressMode::Repeat,..Default::default()});
        let mesh_clamp_sampler=device.create_sampler(&wgpu::SamplerDescriptor {
            // Ground tiles are commonly viewed at grazing angles; preserve
            // detail along the short axis of their projected pixel footprint.
            anisotropy_clamp:if std::env::var_os("ORBITER_DIAGNOSTIC_NO_ANISOTROPY").is_some() {1} else {16},
            mag_filter:wgpu::FilterMode::Linear,min_filter:wgpu::FilterMode::Linear,mipmap_filter:wgpu::FilterMode::Linear,
            address_mode_u:wgpu::AddressMode::ClampToEdge,address_mode_v:wgpu::AddressMode::ClampToEdge,..Default::default()});
        let mesh_white=mesh::white(&device,&queue);
        let draw = draw::DrawState::new(&device);
        Ok(Self { shadow_targets: [None,None], star_image, background, atmosphere, particles, ui, stars, shadow_pipeline, mesh_alpha_pipeline, mesh_sampler, mesh_clamp_sampler, mesh_white, mesh_pipeline, meshes: HashMap::new(), next_mesh:1, draw, pending: Vec::new(), instance, adapter, presentation: None, device, queue, fill, surfaces: HashMap::new(), next_id: 1 })
    }
    fn surface(&self, id: u64) -> Result<&Surface, String> {
        self.surfaces.get(&id).ok_or_else(|| "Unknown surface".into())
    }
}
unsafe fn context<'a>(gpu: *mut OrbGpu) -> Result<&'a mut OrbGpu, String> {
    gpu.as_mut().ok_or_else(|| "Null renderer".into())
}
#[no_mangle]
pub extern "C" fn orb_gpu_create() -> *mut OrbGpu {
    boundary(|| Ok(Box::into_raw(Box::new(pollster::block_on(OrbGpu::new())?))))
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_destroy(gpu: *mut OrbGpu) {
    boundary(|| { if !gpu.is_null() { drop(Box::from_raw(gpu)); } Ok(()) });
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_surface_create(gpu: *mut OrbGpu, width: u32, height: u32) -> u64 {
    boundary(|| {
        let gpu = context(gpu)?;
        let max = gpu.device.limits().max_texture_dimension_2d;
        if width == 0 || height == 0 || width > max || height > max { return Err("Invalid surface dimensions".into()); }
        let texture = gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("Orbiter surface"), size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::COPY_SRC | wgpu::TextureUsages::COPY_DST |
                wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[wgpu::TextureFormat::Rgba8UnormSrgb],
        });
        let id = gpu.next_id;
        gpu.next_id = id.checked_add(1).ok_or("Surface handle exhaustion")?;
        gpu.surfaces.insert(id, Surface { texture, width, height, depth:Default::default() });
        Ok(id)
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_surface_release(gpu: *mut OrbGpu, surface: u64) -> i32 {
    boundary(|| { let gpu = context(gpu)?; draw::flush(gpu)?; gpu.surfaces.remove(&surface).ok_or("Unknown surface")?; Ok(1) })
}
fn buffer_size(surface: &Surface, pitch: u32, size: usize) -> Result<usize, String> {
    if pitch < surface.width * 4 { return Err("Pixel pitch too small".into()); }
    let needed = (pitch as usize).checked_mul((surface.height - 1) as usize)
        .and_then(|n| n.checked_add(surface.width as usize * 4)).ok_or("Pixel size overflow")?;
    if size < needed { return Err("Pixel buffer too small".into()); }
    Ok(needed)
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_surface_upload(gpu: *mut OrbGpu, id: u64, rgba: *const u8, size: usize, pitch: u32) -> i32 {
    boundary(|| {
        let gpu = context(gpu)?; draw::flush(gpu)?; let surface = gpu.surface(id)?;
        if surface.texture.format()!=wgpu::TextureFormat::Rgba8Unorm {return Err("RGBA upload requires an uncompressed surface".into());}
        let needed = buffer_size(surface, pitch, size)?;
        if rgba.is_null() { return Err("Null pixel buffer".into()); }
        gpu.queue.write_texture(surface.texture.as_image_copy(), std::slice::from_raw_parts(rgba, needed),
            wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(pitch), rows_per_image: None },
            surface.texture.size());
        Ok(1)
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_surface_fill_batch(gpu: *mut OrbGpu, id: u64, fills: *const OrbFill, count: usize) -> i32 {
    boundary(|| {
        let gpu = context(gpu)?; draw::flush(gpu)?; let surface = gpu.surface(id)?;
        if !surface.texture.usage().contains(wgpu::TextureUsages::RENDER_ATTACHMENT) {return Err("Surface is sample-only".into());}
        if count == 0 { return Ok(1); }
        if fills.is_null() || count > (u32::MAX as usize / 6) { return Err("Invalid fill batch".into()); }
        let mut vertices = Vec::new();
        for fill in std::slice::from_raw_parts(fills, count) {
            let left = (fill.x as i64).clamp(0, surface.width as i64) as f32;
            let right = (fill.x as i64 + fill.width as i64).clamp(0, surface.width as i64) as f32;
            let top = (fill.y as i64).clamp(0, surface.height as i64) as f32;
            let bottom = (fill.y as i64 + fill.height as i64).clamp(0, surface.height as i64) as f32;
            if left >= right || top >= bottom { continue; }
            let color = fill.rgba.map(|c| c as f32 / 255.0);
            for (x,y) in [(left,top), (left,bottom), (right,top), (right,top), (left,bottom), (right,bottom)] {
                vertices.push(Vertex { position: [2.0*x/surface.width as f32-1.0, 1.0-2.0*y/surface.height as f32], color });
            }
        }
        if vertices.is_empty() { return Ok(1); }
        let buffer = gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("Surface fill batch"), contents: bytemuck::cast_slice(&vertices), usage: wgpu::BufferUsages::VERTEX,
        });
        let view = surface.texture.create_view(&Default::default());
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Surface fill batch"), color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view, resolve_target: None, ops: wgpu::Operations { load: wgpu::LoadOp::Load, store: wgpu::StoreOp::Store },
                })], depth_stencil_attachment: None, timestamp_writes: None, occlusion_query_set: None,
            });
            pass.set_pipeline(&gpu.fill); pass.set_vertex_buffer(0, buffer.slice(..));
            pass.draw(0..vertices.len() as u32, 0..1);
        }
        gpu.queue.submit([encoder.finish()]);
        Ok(1)
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_surface_read(gpu: *mut OrbGpu, id: u64, rgba: *mut u8, size: usize, pitch: u32) -> i32 {
    boundary(|| {
        let gpu = context(gpu)?; draw::flush(gpu)?; let surface = gpu.surface(id)?;
        if surface.texture.format()!=wgpu::TextureFormat::Rgba8Unorm || !surface.texture.usage().contains(wgpu::TextureUsages::COPY_SRC) {return Err("Readback requires a readable RGBA surface".into());}
        buffer_size(surface, pitch, size)?;
        if rgba.is_null() { return Err("Null pixel buffer".into()); }
        let row_bytes = surface.width * 4;
        let aligned = row_bytes.div_ceil(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT) * wgpu::COPY_BYTES_PER_ROW_ALIGNMENT;
        let buffer = gpu.device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("Surface readback"), size: aligned as u64 * surface.height as u64,
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ, mapped_at_creation: false,
        });
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        encoder.copy_texture_to_buffer(surface.texture.as_image_copy(), wgpu::TexelCopyBufferInfo {
            buffer: &buffer, layout: wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(aligned), rows_per_image: None },
        }, surface.texture.size());
        gpu.queue.submit([encoder.finish()]);
        let (tx, rx) = std::sync::mpsc::channel();
        buffer.slice(..).map_async(wgpu::MapMode::Read, move |result| { let _ = tx.send(result); });
        gpu.device.poll(wgpu::Maintain::Wait);
        rx.recv().map_err(|e| e.to_string())?.map_err(|e| e.to_string())?;
        {
            let data = buffer.slice(..).get_mapped_range();
            for y in 0..surface.height as usize {
                ptr::copy_nonoverlapping(data.as_ptr().add(y * aligned as usize), rgba.add(y * pitch as usize), row_bytes as usize);
            }
        }
        buffer.unmap();
        Ok(1)
    })
}
