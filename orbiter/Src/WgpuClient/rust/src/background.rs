use super::{boundary,context,OrbGpu};
use wgpu::util::DeviceExt;
pub(super) struct Background {pub pipeline:wgpu::RenderPipeline,data:Option<([f32;16],wgpu::Texture)>}
impl Background {
    pub fn new(device:&wgpu::Device,additive:bool)->Self {
        let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {label:Some("Celestial background"),source:wgpu::ShaderSource::Wgsl(include_str!("background.wgsl").into())});
        let pipeline=device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {label:Some("Celestial background"),layout:None,
            vertex:wgpu::VertexState {module:&shader,entry_point:Some("vs"),compilation_options:Default::default(),buffers:&[]},
            fragment:Some(wgpu::FragmentState {module:&shader,entry_point:Some("fs"),compilation_options:Default::default(),targets:&[
                Some(wgpu::ColorTargetState {format:wgpu::TextureFormat::Rgba8Unorm,blend:if additive {Some(wgpu::BlendState {color:wgpu::BlendComponent {src_factor:wgpu::BlendFactor::One,dst_factor:wgpu::BlendFactor::One,operation:wgpu::BlendOperation::Add},alpha:wgpu::BlendComponent::REPLACE})}else{None},write_mask:wgpu::ColorWrites::ALL})]}),
            primitive:Default::default(),depth_stencil:Some(wgpu::DepthStencilState {format:wgpu::TextureFormat::Depth32Float,
                depth_write_enabled:false,depth_compare:wgpu::CompareFunction::Always,stencil:Default::default(),bias:Default::default()}),
            multisample:Default::default(),multiview:None,cache:None});
        Self {pipeline,data:None}
    }
    pub fn binding(&self,device:&wgpu::Device,sampler:&wgpu::Sampler)->Option<wgpu::BindGroup> {
        let (values,texture)=self.data.as_ref()?;
        let buffer=device.create_buffer_init(&wgpu::util::BufferInitDescriptor {label:Some("Celestial camera"),contents:bytemuck::cast_slice(values),usage:wgpu::BufferUsages::UNIFORM});
        let view=texture.create_view(&Default::default());
        Some(device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Celestial background"),layout:&self.pipeline.get_bind_group_layout(0),entries:&[
            wgpu::BindGroupEntry {binding:0,resource:buffer.as_entire_binding()},
            wgpu::BindGroupEntry {binding:1,resource:wgpu::BindingResource::TextureView(&view)},
            wgpu::BindGroupEntry {binding:2,resource:wgpu::BindingResource::Sampler(sampler)}]}))
    }
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_background(gpu:*mut OrbGpu,texture:u64,values:*const f32)->i32 {
    configure(gpu,texture,values,false)
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_star_image(gpu:*mut OrbGpu,texture:u64,values:*const f32)->i32 {
    configure(gpu,texture,values,true)
}
unsafe fn configure(gpu:*mut OrbGpu,texture:u64,values:*const f32,stars:bool)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if texture==0 {if stars {gpu.star_image.data=None;}else{gpu.background.data=None;}return Ok(1);}
        if values.is_null() {return Err("Missing celestial camera".into());}
        let mut data=[0f32;16];data.copy_from_slice(std::slice::from_raw_parts(values,16));
        if data.iter().any(|v|!v.is_finite()) || !(0.0..=1.0).contains(&data[12]) {return Err("Invalid celestial camera".into());}
        let texture=gpu.surface(texture)?.texture.clone();
        let layer=if stars {&mut gpu.star_image}else{&mut gpu.background};layer.data=Some((data,texture));Ok(1)
    })
}
