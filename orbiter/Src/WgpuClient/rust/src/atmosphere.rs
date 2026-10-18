use super::{boundary,context,OrbGpu};
use wgpu::util::DeviceExt;
pub(super) struct Atmosphere {pub pipeline:wgpu::RenderPipeline, uniform:Option<[f32;20]>}
impl Atmosphere {
    pub fn new(device:&wgpu::Device)->Self {
        let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {label:Some("Rayleigh sky"),source:wgpu::ShaderSource::Wgsl(include_str!("atmosphere.wgsl").into())});
        let pipeline=device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label:Some("Atmospheric sky"),layout:None,
            vertex:wgpu::VertexState {module:&shader,entry_point:Some("vs"),compilation_options:Default::default(),buffers:&[]},
            fragment:Some(wgpu::FragmentState {module:&shader,entry_point:Some("fs"),compilation_options:Default::default(),targets:&[Some(wgpu::ColorTargetState {
                format:wgpu::TextureFormat::Rgba8Unorm,blend:Some(wgpu::BlendState::PREMULTIPLIED_ALPHA_BLENDING),write_mask:wgpu::ColorWrites::ALL})]}),
            primitive:Default::default(),depth_stencil:None,
            multisample:Default::default(),multiview:None,cache:None,
        });Self {pipeline,uniform:None}
    }
    pub fn binding(&self,device:&wgpu::Device,depth:&wgpu::TextureView)->Option<wgpu::BindGroup> {
        let values=self.uniform?;
        let buffer=device.create_buffer_init(&wgpu::util::BufferInitDescriptor {label:Some("Atmosphere camera"),contents:bytemuck::cast_slice(&values),usage:wgpu::BufferUsages::UNIFORM});
        Some(device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Atmosphere camera"),layout:&self.pipeline.get_bind_group_layout(0),entries:&[
            wgpu::BindGroupEntry {binding:0,resource:buffer.as_entire_binding()},
            wgpu::BindGroupEntry {binding:1,resource:wgpu::BindingResource::TextureView(depth)}]}))
    }
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_atmosphere(gpu:*mut OrbGpu,values:*const f32)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if values.is_null() {gpu.atmosphere.uniform=None;return Ok(1);}
        let mut data=[0f32;20];data.copy_from_slice(std::slice::from_raw_parts(values,20));
        let sun_norm=data[16..19].iter().map(|v|v*v).sum::<f32>();
        if data.iter().any(|v|!v.is_finite()) || data[15]<=0.0 || data[19]<=data[15] || !(0.99..=1.01).contains(&sun_norm) {
            return Err("Invalid atmospheric sky parameters".into());
        }
        gpu.atmosphere.uniform=Some(data);Ok(1)
    })
}
