use super::{boundary,context,OrbGpu};
use wgpu::util::DeviceExt;
#[repr(C)]
#[derive(Clone,Copy,bytemuck::Pod,bytemuck::Zeroable)]
pub struct OrbStar {direction:[f32;3],color:[f32;3],radius:f32}
const _: ()=assert!(std::mem::size_of::<OrbStar>()==28);
pub(super) struct Stars {
    pub pipeline:wgpu::RenderPipeline,
    pub vertices:Option<wgpu::Buffer>,pub count:u32,pub camera:Option<[f32;16]>,
}
impl Stars {
    pub fn new(device:&wgpu::Device)->Self {
        let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {label:Some("Catalog stars"),source:wgpu::ShaderSource::Wgsl(include_str!("stars.wgsl").into())});
        let attributes=wgpu::vertex_attr_array![0=>Float32x3,1=>Float32x3,2=>Float32];
        let pipeline=device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label:Some("Stars at infinity"),layout:None,
            vertex:wgpu::VertexState {module:&shader,entry_point:Some("vs"),compilation_options:Default::default(),buffers:&[
                wgpu::VertexBufferLayout {array_stride:28,step_mode:wgpu::VertexStepMode::Instance,attributes:&attributes}]},
            fragment:Some(wgpu::FragmentState {module:&shader,entry_point:Some("fs"),compilation_options:Default::default(),targets:&[
                Some(wgpu::ColorTargetState {format:wgpu::TextureFormat::Rgba8Unorm,blend:Some(wgpu::BlendState::ALPHA_BLENDING),write_mask:wgpu::ColorWrites::ALL})]}),
            primitive:Default::default(),depth_stencil:Some(wgpu::DepthStencilState {format:wgpu::TextureFormat::Depth32Float,
                depth_write_enabled:false,depth_compare:wgpu::CompareFunction::Always,stencil:Default::default(),bias:Default::default()}),
            multisample:Default::default(),multiview:None,cache:None,
        });
        Self {pipeline,vertices:None,count:0,camera:None}
    }
    pub fn binding(&self,device:&wgpu::Device,width:u32,height:u32)->Option<wgpu::BindGroup> {
        if self.count==0 {return None;}
        let camera=self.camera?;
        let mut uniform=[0f32;20];uniform[..16].copy_from_slice(&camera);uniform[16]=2.0/width as f32;uniform[17]=2.0/height as f32;
        let buffer=device.create_buffer_init(&wgpu::util::BufferInitDescriptor {label:Some("Star camera"),contents:bytemuck::cast_slice(&uniform),usage:wgpu::BufferUsages::UNIFORM});
        Some(device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Star camera"),layout:&self.pipeline.get_bind_group_layout(0),entries:&[
            wgpu::BindGroupEntry {binding:0,resource:buffer.as_entire_binding()}]}))
    }
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_stars_upload(gpu:*mut OrbGpu,stars:*const OrbStar,count:usize)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if count==0 {gpu.stars.vertices=None;gpu.stars.count=0;return Ok(1);}
        if stars.is_null() || count>2000000 {return Err("Invalid star batch".into());}
        let stars=std::slice::from_raw_parts(stars,count);
        for star in stars {
            if star.direction.iter().chain(star.color.iter()).chain(std::iter::once(&star.radius)).any(|v|!v.is_finite()) ||
               star.radius<=0.0 || star.radius>32.0 || !(0.99..=1.01).contains(&star.direction.iter().map(|v|v*v).sum::<f32>()) ||
               star.color.iter().any(|&v|!(0.0..=1.0).contains(&v)) {return Err("Invalid star record".into());}
        }
        gpu.stars.vertices=Some(gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label:Some("Star catalog"),contents:bytemuck::cast_slice(stars),usage:wgpu::BufferUsages::VERTEX}));
        gpu.stars.count=count as u32;Ok(1)
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_stars_camera(gpu:*mut OrbGpu,matrix:*const f32)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if matrix.is_null() {return Err("Null star camera".into());}
        let mut camera=[0f32;16];camera.copy_from_slice(std::slice::from_raw_parts(matrix,16));
        if camera.iter().any(|v|!v.is_finite()) {return Err("Invalid star camera".into());}
        gpu.stars.camera=Some(camera);Ok(1)
    })
}
