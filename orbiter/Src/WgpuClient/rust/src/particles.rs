use super::{boundary,context,OrbGpu};
use wgpu::util::DeviceExt;
#[repr(C)]
#[derive(Clone,Copy,bytemuck::Pod,bytemuck::Zeroable)]
pub struct OrbParticle {center:[f32;4],extent:[f32;2],color:[f32;4],sprite:u32,padding:u32,texture:u64}
const _:()=assert!(std::mem::size_of::<OrbParticle>()==56);
pub(super) struct Particles {pub pipeline:wgpu::RenderPipeline,pub vertices:Option<wgpu::Buffer>,pub count:u32,pub depths:Vec<f32>,pub runs:Vec<(std::ops::Range<u32>,wgpu::BindGroup)>}
impl Particles {
    pub fn new(device:&wgpu::Device)->Self {
        let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {label:Some("Particle billboards"),source:wgpu::ShaderSource::Wgsl(include_str!("particles.wgsl").into())});
        let attrs=wgpu::vertex_attr_array![0=>Float32x4,1=>Float32x2,2=>Float32x4,3=>Uint32];
        let pipeline=device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label:Some("Depth-tested transparent particles"),layout:None,
            vertex:wgpu::VertexState {module:&shader,entry_point:Some("vs"),compilation_options:Default::default(),buffers:&[
                wgpu::VertexBufferLayout {array_stride:56,step_mode:wgpu::VertexStepMode::Instance,attributes:&attrs}]},
            fragment:Some(wgpu::FragmentState {module:&shader,entry_point:Some("fs"),compilation_options:Default::default(),targets:&[
                Some(wgpu::ColorTargetState {format:wgpu::TextureFormat::Rgba8Unorm,blend:Some(wgpu::BlendState::ALPHA_BLENDING),write_mask:wgpu::ColorWrites::ALL})]}),
            primitive:Default::default(),depth_stencil:Some(wgpu::DepthStencilState {format:wgpu::TextureFormat::Depth32Float,
                depth_write_enabled:false,depth_compare:wgpu::CompareFunction::Greater,stencil:Default::default(),bias:Default::default()}),
            multisample:Default::default(),multiview:None,cache:None});
        Self {pipeline,vertices:None,count:0,depths:Vec::new(),runs:Vec::new()}
    }
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_particles_upload(gpu:*mut OrbGpu,records:*const OrbParticle,count:usize)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if count==0 {gpu.particles.vertices=None;gpu.particles.count=0;gpu.particles.depths.clear();gpu.particles.runs.clear();return Ok(1);}
        if records.is_null() || count>1000000 {return Err("Invalid particle batch".into());}
        let records=std::slice::from_raw_parts(records,count);
        for p in records {
            if p.center.iter().chain(p.extent.iter()).chain(p.color.iter()).any(|v|!v.is_finite()) ||
               p.sprite>8 || p.center[3]<=0.0 || p.extent.iter().any(|&v|v<0.0) || p.color.iter().any(|&v|!(0.0..=1.0).contains(&v)) {
                return Err("Invalid particle record".into());
            }
        }
        // The native client already sorts. Also accept unsorted callers without
        // allowing their records to break the cloud/particle depth merge.
        let mut sorted;
        let records=if records.windows(2).any(|pair|pair[0].center[3]<pair[1].center[3]) {
            sorted=records.to_vec();sorted.sort_by(|a,b|b.center[3].total_cmp(&a.center[3]));&sorted[..]
        } else {records};
        // Preserve depth sorting: only adjacent records sharing a texture are grouped.
        let mut runs=Vec::new();let mut first=0;
        while first<count {
            let id=records[first].texture;let mut end=first+1;
            while end<count && records[end].texture==id {end+=1;}
            let texture=if id==0 {&gpu.mesh_white} else {&gpu.surface(id)?.texture};
            let view=texture.create_view(&Default::default());
            let binding=gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {
                label:Some("Particle texture"),layout:&gpu.particles.pipeline.get_bind_group_layout(0),entries:&[
                    wgpu::BindGroupEntry {binding:0,resource:wgpu::BindingResource::TextureView(&view)},
                    wgpu::BindGroupEntry {binding:1,resource:wgpu::BindingResource::Sampler(&gpu.mesh_sampler)}]});
            runs.push((first as u32..end as u32,binding));first=end;
        }
        gpu.particles.runs=runs;
        gpu.particles.depths=records.iter().map(|p|p.center[3]).collect();
        gpu.particles.vertices=Some(gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label:Some("Batched particle instances"),contents:bytemuck::cast_slice(records),usage:wgpu::BufferUsages::VERTEX}));
        gpu.particles.count=count as u32;Ok(1)
    })
}
