use super::{boundary, context, OrbGpu};
use wgpu::util::DeviceExt;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OrbDraw {
    sequence: u64, source: u64,
    x: i32, y: i32, width: u32, height: u32,
    src_x: i32, src_y: i32, src_width: u32, src_height: u32,
    color: u32, key: u32, flags: u32, reserved: u32,
}
const _: () = assert!(std::mem::size_of::<OrbDraw>() == 64);
#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
struct Vertex { position: [f32;2], uv: [f32;2], color: [f32;4], key: u32, flags: u32 }
pub(super) struct DrawState { pipeline: wgpu::RenderPipeline, dummy: wgpu::Texture }
impl DrawState {
    pub fn new(device: &wgpu::Device) -> Self {
        let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Ordered surface commands"), source: wgpu::ShaderSource::Wgsl(include_str!("draw.wgsl").into()),
        });
        let attributes=wgpu::vertex_attr_array![0=>Float32x2,1=>Float32x2,2=>Float32x4,3=>Uint32,4=>Uint32];
        let pipeline=device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Batched blit/fill"), layout: None,
            vertex: wgpu::VertexState { module: &shader, entry_point: Some("vs"), compilation_options: Default::default(),
                buffers: &[wgpu::VertexBufferLayout { array_stride: std::mem::size_of::<Vertex>() as u64,
                    step_mode: wgpu::VertexStepMode::Vertex, attributes: &attributes }] },
            fragment: Some(wgpu::FragmentState { module: &shader, entry_point: Some("fs"), compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState { format: wgpu::TextureFormat::Rgba8Unorm, blend: None, write_mask: wgpu::ColorWrites::ALL })] }),
            primitive: Default::default(), depth_stencil: None, multisample: Default::default(), multiview: None, cache: None,
        });
        let dummy=device.create_texture(&wgpu::TextureDescriptor {
            label: Some("Fill binding"), size: wgpu::Extent3d {width:1,height:1,depth_or_array_layers:1},
            mip_level_count:1, sample_count:1, dimension:wgpu::TextureDimension::D2, format:wgpu::TextureFormat::Rgba8Unorm,
            usage:wgpu::TextureUsages::TEXTURE_BINDING, view_formats:&[],
        });
        Self {pipeline,dummy}
    }
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_queue_batch(gpu: *mut OrbGpu, target: u64, commands: *const OrbDraw, count: usize) -> i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if !gpu.surface(target)?.texture.usage().contains(wgpu::TextureUsages::RENDER_ATTACHMENT) {return Err("Texture is not a render target".into());}
        if count==0 {return Ok(1);}
        if commands.is_null() || count>u32::MAX as usize/6 {return Err("Invalid command batch".into());}
        let commands=std::slice::from_raw_parts(commands,count);
        for command in commands {
            if command.source!=0 {gpu.surface(command.source)?;}
            if command.flags & !1 != 0 {return Err("Unsupported draw flags".into());}
        }
        gpu.pending.extend(commands.iter().map(|c|(target,*c)));
        Ok(1)
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_flush(gpu: *mut OrbGpu) -> i32 {
    boundary(|| {flush(context(gpu)?)?; Ok(1)})
}
pub(super) fn flush(gpu: &mut OrbGpu) -> Result<(),String> {
    if gpu.pending.is_empty() {return Ok(());}
    let mut commands=std::mem::take(&mut gpu.pending);
    commands.sort_by_key(|(_,c)|c.sequence);
    let mut encoder=gpu.device.create_command_encoder(&Default::default());
    let mut first=0;
    while first<commands.len() {
        let (target_id,command)=commands[first];
        let target=gpu.surface(target_id)?;
        let mut end=first+1;
        // Consecutive operations sharing source and destination become one draw.
        // A self-blit must snapshot the preceding result before each operation.
        if command.source!=target_id {
            while end<commands.len() && commands[end].0==target_id && commands[end].1.source==command.source {end+=1;}
        }
        let snapshot;
        let source=if command.source==0 {&gpu.draw.dummy} else if command.source==target_id {
            snapshot=gpu.device.create_texture(&wgpu::TextureDescriptor {
                label:Some("Self-blit snapshot"), size:target.texture.size(), mip_level_count:1,sample_count:1,
                dimension:wgpu::TextureDimension::D2,format:wgpu::TextureFormat::Rgba8Unorm,
                usage:wgpu::TextureUsages::COPY_DST|wgpu::TextureUsages::TEXTURE_BINDING, view_formats:&[],
            });
            encoder.copy_texture_to_texture(target.texture.as_image_copy(),snapshot.as_image_copy(),target.texture.size());
            &snapshot
        } else {&gpu.surface(command.source)?.texture};
        let mut vertices=Vec::new();
        for (_,c) in &commands[first..end] {
            if c.width==0 || c.height==0 || (c.source!=0 && (c.src_width==0 || c.src_height==0)) {continue;}
            let color=[((c.color>>16)&255) as f32/255.0,((c.color>>8)&255) as f32/255.0,(c.color&255) as f32/255.0,1.0];
            for (u,v) in [(0.0,0.0),(0.0,1.0),(1.0,0.0),(1.0,0.0),(0.0,1.0),(1.0,1.0)] {
                let x=c.x as f64+u*c.width as f64; let y=c.y as f64+v*c.height as f64;
                vertices.push(Vertex {
                    position:[(2.0*x/target.width as f64-1.0) as f32,(1.0-2.0*y/target.height as f64) as f32],
                    uv:[(c.src_x as f64+u*c.src_width as f64) as f32,(c.src_y as f64+v*c.src_height as f64) as f32],
                    color,key:c.key&0xffffff,flags:if c.source==0 {0} else {1|(c.flags<<1)},
                });
            }
        }
        if !vertices.is_empty() {
            let buffer=gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
                label:Some("Surface commands"),contents:bytemuck::cast_slice(&vertices),usage:wgpu::BufferUsages::VERTEX,
            });
            let source_view=source.create_view(&Default::default());
            let target_view=target.texture.create_view(&Default::default());
            let bind=gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {
                label:Some("Blit source"),layout:&gpu.draw.pipeline.get_bind_group_layout(0),entries:&[
                    wgpu::BindGroupEntry {binding:0,resource:wgpu::BindingResource::TextureView(&source_view)}],
            });
            let mut pass=encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label:Some("Surface commands"),color_attachments:&[Some(wgpu::RenderPassColorAttachment {
                    view:&target_view,resolve_target:None,ops:wgpu::Operations {load:wgpu::LoadOp::Load,store:wgpu::StoreOp::Store},
                })],depth_stencil_attachment:None,timestamp_writes:None,occlusion_query_set:None,
            });
            pass.set_pipeline(&gpu.draw.pipeline);pass.set_bind_group(0,&bind,&[]);pass.set_vertex_buffer(0,buffer.slice(..));
            pass.draw(0..vertices.len() as u32,0..1);
        }
        first=end;
    }
    gpu.queue.submit([encoder.finish()]);
    Ok(())
}

// Promote on first write, preserving the public handle and every mesh binding.
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_surface_make_writable(gpu:*mut OrbGpu,id:u64)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if gpu.surface(id)?.texture.usage().contains(wgpu::TextureUsages::RENDER_ATTACHMENT) {return Ok(1);}
        flush(gpu)?;
        let source=gpu.surface(id)?;let width=source.width;let height=source.height;
        let texture=gpu.device.create_texture(&wgpu::TextureDescriptor {
            label:Some("Writable cockpit texture"),size:wgpu::Extent3d {width,height,depth_or_array_layers:1},
            mip_level_count:1,sample_count:1,dimension:wgpu::TextureDimension::D2,format:wgpu::TextureFormat::Rgba8Unorm,
            usage:wgpu::TextureUsages::COPY_SRC|wgpu::TextureUsages::COPY_DST|wgpu::TextureUsages::TEXTURE_BINDING|wgpu::TextureUsages::RENDER_ATTACHMENT,
            view_formats:&[wgpu::TextureFormat::Rgba8UnormSrgb],
        });
        let temporary=gpu.next_id;gpu.next_id=temporary.checked_add(1).ok_or("Surface handle exhaustion")?;
        gpu.surfaces.insert(temporary,super::Surface {texture,width,height,depth:Default::default()});
        gpu.pending.push((temporary,OrbDraw {sequence:0,source:id,x:0,y:0,width,height,src_x:0,src_y:0,src_width:width,src_height:height,color:0,key:0,flags:0,reserved:0}));
        flush(gpu)?;
        let surface=gpu.surfaces.remove(&temporary).unwrap();
        // Texture bindings retain their views. Rebuild them when promotion replaces the allocation.
        for mesh in gpu.meshes.values_mut() {for group in &mut mesh.groups {if group.texture_id==id || group.emission_id==id {
            if group.texture_id==id {group.texture=surface.texture.clone();}
            if group.emission_id==id || (group.emission_id==0 && group.texture_id==id) {group.emission=surface.texture.clone();}
            let sampler=if group.clamp_texture {&gpu.mesh_clamp_sampler} else {&gpu.mesh_sampler};
            group.binding=super::mesh::texture_binding(&gpu.device,&gpu.mesh_pipeline,sampler,&group.texture,&group.emission);
        }}}
        gpu.surfaces.insert(id,surface);Ok(1)
    })
}
