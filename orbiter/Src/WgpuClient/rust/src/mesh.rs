use super::{boundary,context,OrbGpu};
use wgpu::util::DeviceExt;
#[repr(C)]
#[derive(Clone,Copy,bytemuck::Pod,bytemuck::Zeroable)]
pub struct OrbMeshVertex { position:[f32;3],normal:[f32;3],uv:[f32;2],color:[f32;4],light:[f32;4],emission:[f32;4],specular:[f32;4] }
impl OrbMeshVertex {
    fn finite(&self)->bool {self.position.iter().chain(self.normal.iter()).chain(self.uv.iter()).chain(self.color.iter()).chain(self.light.iter()).chain(self.emission.iter()).chain(self.specular.iter()).all(|v|v.is_finite())}
}
const _: ()=assert!(std::mem::size_of::<OrbMeshVertex>()==96);
pub(super) struct Mesh { vertices:wgpu::Buffer,indices:wgpu::Buffer,count:u32,nvertices:usize,pub(super) groups:Vec<MeshGroup> }
pub(super) struct MeshGroup {
    sort_center:[f32;3],
    shadow_only:bool,
    alpha_blend:bool,
    pub(super) clamp_texture:bool,
    first:u32, count:u32, pub(super) texture_id:u64, pub(super) emission_id:u64, pub(super) texture:wgpu::Texture, pub(super) emission:wgpu::Texture, pub(super) binding:wgpu::BindGroup,
}
pub(super) fn texture_binding(device:&wgpu::Device,pipeline:&wgpu::RenderPipeline,sampler:&wgpu::Sampler,texture:&wgpu::Texture,emission:&wgpu::Texture)->wgpu::BindGroup {
    let view=texture.create_view(&Default::default());
    let emission_view=emission.create_view(&Default::default());
    device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Mesh texture"),layout:&pipeline.get_bind_group_layout(1),entries:&[
        wgpu::BindGroupEntry {binding:0,resource:wgpu::BindingResource::TextureView(&view)},
        wgpu::BindGroupEntry {binding:1,resource:wgpu::BindingResource::Sampler(sampler)},
        wgpu::BindGroupEntry {binding:2,resource:wgpu::BindingResource::TextureView(&emission_view)}]})
}
fn group(gpu:&OrbGpu,first:u32,count:u32,texture_id:u64,texture:&wgpu::Texture,emission_id:u64,emission:&wgpu::Texture,clamp_texture:bool,alpha_blend:bool,shadow_only:bool)->MeshGroup {
    let sampler=if clamp_texture {&gpu.mesh_clamp_sampler} else {&gpu.mesh_sampler};
    MeshGroup {sort_center:[0.0;3],first,count,texture_id,emission_id,clamp_texture,alpha_blend,shadow_only,texture:texture.clone(),emission:emission.clone(),binding:texture_binding(&gpu.device,&gpu.mesh_pipeline,sampler,texture,emission)}
}
pub(super) fn pipeline(device:&wgpu::Device)->(wgpu::RenderPipeline,wgpu::RenderPipeline) {
    let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {label:Some("Native mesh"),source:wgpu::ShaderSource::Wgsl(include_str!("mesh.wgsl").into())});
    let attributes=[
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x4,offset:80,shader_location:6},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x4,offset:48,shader_location:4},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x4,offset:64,shader_location:5},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x2,offset:24,shader_location:3},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x3,offset:0,shader_location:0},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x3,offset:12,shader_location:1},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x4,offset:32,shader_location:2}];
    let camera=device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {label:Some("Shared mesh camera"),entries:&[
        wgpu::BindGroupLayoutEntry {binding:0,visibility:wgpu::ShaderStages::VERTEX_FRAGMENT,ty:wgpu::BindingType::Buffer {
            ty:wgpu::BufferBindingType::Uniform,has_dynamic_offset:false,min_binding_size:None},count:None}]});
    let texture_entry=|binding,sample_type|wgpu::BindGroupLayoutEntry {binding,visibility:wgpu::ShaderStages::FRAGMENT,
        ty:wgpu::BindingType::Texture {sample_type,view_dimension:wgpu::TextureViewDimension::D2,multisampled:false},count:None};
    let sampler_entry=|binding,kind|wgpu::BindGroupLayoutEntry {binding,visibility:wgpu::ShaderStages::FRAGMENT,ty:wgpu::BindingType::Sampler(kind),count:None};
    let textures=device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {label:Some("Shared mesh textures"),entries:&[
        texture_entry(0,wgpu::TextureSampleType::Float {filterable:true}),sampler_entry(1,wgpu::SamplerBindingType::Filtering),texture_entry(2,wgpu::TextureSampleType::Float {filterable:true})]});
    let shadows=device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {label:Some("Shared mesh shadows"),entries:&[
        texture_entry(0,wgpu::TextureSampleType::Depth),sampler_entry(1,wgpu::SamplerBindingType::Comparison)]});
    let layout=device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {label:Some("Opaque and transparent meshes"),bind_group_layouts:&[&camera,&textures,&shadows],push_constant_ranges:&[]});
    let create=|alpha|device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label:Some("Native mesh reversed Z"),layout:Some(&layout),
        vertex:wgpu::VertexState {module:&shader,entry_point:Some("vs"),compilation_options:Default::default(),buffers:&[
            wgpu::VertexBufferLayout {array_stride:96,step_mode:wgpu::VertexStepMode::Vertex,attributes:&attributes}]},
        fragment:Some(wgpu::FragmentState {module:&shader,entry_point:Some(if alpha {"fs_alpha"} else {"fs"}),compilation_options:Default::default(),targets:&[
            Some(wgpu::ColorTargetState {format:wgpu::TextureFormat::Rgba8Unorm,blend:if alpha {Some(wgpu::BlendState::ALPHA_BLENDING)} else {None},write_mask:wgpu::ColorWrites::ALL})]}),
        primitive:wgpu::PrimitiveState {cull_mode:None,..Default::default()},
        depth_stencil:Some(wgpu::DepthStencilState {format:wgpu::TextureFormat::Depth32Float,depth_write_enabled:!alpha,
            depth_compare:wgpu::CompareFunction::Greater,stencil:Default::default(),bias:Default::default()}),
        multisample:Default::default(),multiview:None,cache:None,
    });(create(false),create(true))
}
pub(super) fn shadow_pipeline(device:&wgpu::Device)->wgpu::RenderPipeline {
    let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {label:Some("Sunlight shadow"),source:wgpu::ShaderSource::Wgsl(include_str!("shadow.wgsl").into())});
    let attributes=[
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x3,offset:0,shader_location:0},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x2,offset:24,shader_location:1},
        wgpu::VertexAttribute {format:wgpu::VertexFormat::Float32x4,offset:32,shader_location:2}];
    device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label:Some("Sunlight shadow depth"),layout:None,
        vertex:wgpu::VertexState {module:&shader,entry_point:Some("vs"),compilation_options:Default::default(),buffers:&[
            wgpu::VertexBufferLayout {array_stride:96,step_mode:wgpu::VertexStepMode::Vertex,attributes:&attributes}]},
        fragment:Some(wgpu::FragmentState {module:&shader,entry_point:Some("fs"),compilation_options:Default::default(),targets:&[]}),
        primitive:wgpu::PrimitiveState {cull_mode:None,..Default::default()},
        depth_stencil:Some(wgpu::DepthStencilState {format:wgpu::TextureFormat::Depth32Float,depth_write_enabled:true,
            depth_compare:wgpu::CompareFunction::Greater,stencil:Default::default(),bias:Default::default()}),
        multisample:Default::default(),multiview:None,cache:None,
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_mesh_create(gpu:*mut OrbGpu,vertices:*const OrbMeshVertex,nvertices:usize,indices:*const u32,nindices:usize)->u64 {
    boundary(|| {
        let gpu=context(gpu)?;
        if vertices.is_null() || indices.is_null() || nvertices==0 || nindices==0 || nindices%3!=0 || nvertices>u32::MAX as usize || nindices>u32::MAX as usize {
            return Err("Invalid mesh arrays".into());
        }
        if nvertices as u64*96>gpu.device.limits().max_buffer_size || nindices as u64*4>gpu.device.limits().max_buffer_size {return Err("Mesh exceeds GPU buffer limits".into());}
        let vertices=std::slice::from_raw_parts(vertices,nvertices);let indices=std::slice::from_raw_parts(indices,nindices);
        if vertices.iter().any(|v|!v.finite()) {return Err("Nonfinite mesh vertex".into());}
        if indices.iter().any(|&i|i as usize>=nvertices) {return Err("Mesh index out of range".into());}
        let vertex_buffer=gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {label:Some("Mesh vertices"),contents:bytemuck::cast_slice(vertices),usage:wgpu::BufferUsages::VERTEX|wgpu::BufferUsages::COPY_DST});
        let index_buffer=gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {label:Some("Mesh indices"),contents:bytemuck::cast_slice(indices),usage:wgpu::BufferUsages::INDEX});
        let id=gpu.next_mesh;gpu.next_mesh=id.checked_add(1).ok_or("Mesh handle exhaustion")?;
        let groups=vec![group(gpu,0,nindices as u32,0,&gpu.mesh_white,0,&gpu.mesh_white,false,false,false)];
        gpu.meshes.insert(id,Mesh {vertices:vertex_buffer,indices:index_buffer,count:nindices as u32,nvertices,groups});Ok(id)
    })
}
#[repr(C)]
#[derive(Clone,Copy)]
pub struct OrbMeshGroup { first:u32,count:u32,texture:u64,emission:u64,clamp_texture:u64,alpha_blend:u64,shadow_only:u64,sort_center:[f32;3],reserved:f32 }
const _: ()=assert!(std::mem::size_of::<OrbMeshGroup>()==64);
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_mesh_groups(gpu:*mut OrbGpu,id:u64,groups:*const OrbMeshGroup,count:usize)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if groups.is_null() || count==0 || count>isize::MAX as usize/std::mem::size_of::<OrbMeshGroup>() {return Err("Invalid mesh groups".into());}
        let mesh=gpu.meshes.get(&id).ok_or("Unknown mesh")?;
        let mut result=Vec::new();
        for group in std::slice::from_raw_parts(groups,count) {
            if group.first%3!=0 || group.count%3!=0 || group.first.checked_add(group.count).is_none_or(|end|end>mesh.count) {return Err("Invalid mesh group range".into());}
            let texture=if group.texture==0 {&gpu.mesh_white} else {&gpu.surface(group.texture)?.texture};
            let emission=if group.emission==0 {texture} else {&gpu.surface(group.emission)?.texture};
            if group.clamp_texture>1 || group.alpha_blend>1 || group.shadow_only>1 {return Err("Invalid mesh material mode".into());}
            if group.sort_center.iter().any(|v|!v.is_finite()) {return Err("Nonfinite transparent sort centre".into());}
            result.push(self::group(gpu,group.first,group.count,group.texture,texture,group.emission,emission,group.clamp_texture!=0,group.alpha_blend!=0,group.shadow_only!=0));
            result.last_mut().unwrap().sort_center=group.sort_center;
        }
        gpu.meshes.get_mut(&id).unwrap().groups=result;Ok(1)
    })
}
pub(super) fn white(device:&wgpu::Device,queue:&wgpu::Queue)->wgpu::Texture {
    device.create_texture_with_data(queue,&wgpu::TextureDescriptor {
        label:Some("Untextured mesh white"),size:wgpu::Extent3d {width:1,height:1,depth_or_array_layers:1},mip_level_count:1,sample_count:1,
        dimension:wgpu::TextureDimension::D2,format:wgpu::TextureFormat::Rgba8Unorm,usage:wgpu::TextureUsages::TEXTURE_BINDING,view_formats:&[]
    },wgpu::util::TextureDataOrder::LayerMajor,&[255,255,255,255])
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_mesh_release(gpu:*mut OrbGpu,id:u64)->i32 {
    boundary(|| {context(gpu)?.meshes.remove(&id).ok_or("Unknown mesh")?;Ok(1)})
}
#[repr(C)]
#[derive(Clone,Copy,bytemuck::Pod,bytemuck::Zeroable)]
pub struct OrbLocalLight {position:[f32;4],direction:[f32;4],attenuation:[f32;4],diffuse:[f32;4],ambient:[f32;4]}
#[repr(C)]
pub struct OrbMeshDraw {mesh:u64,mvp:[f32;16],sunlight:[f32;4],lights:[OrbLocalLight;8],shadow_mvp:[f32;16],shadow:[f32;4],eye:[f32;4]}
const _: ()=assert!(std::mem::size_of::<OrbLocalLight>()==80 && std::mem::size_of::<OrbMeshDraw>()==824);
// Keep both atlas sizes resident after first use so toggling shadows does not
// allocate textures every frame. Each render pass still clears the depth data.
pub(super) struct ShadowTarget {
    view: wgpu::TextureView,
    binding: wgpu::BindGroup,
}
impl ShadowTarget {
    fn new(device: &wgpu::Device, pipeline: &wgpu::RenderPipeline, shadow_size: u32) -> Self {
        let shadow_depth=device.create_texture(&wgpu::TextureDescriptor {label:Some("Focused sunlight shadow"),
            size:wgpu::Extent3d {width:shadow_size*2,height:shadow_size,depth_or_array_layers:1},mip_level_count:1,sample_count:1,
            dimension:wgpu::TextureDimension::D2,format:wgpu::TextureFormat::Depth32Float,
            usage:wgpu::TextureUsages::RENDER_ATTACHMENT|wgpu::TextureUsages::TEXTURE_BINDING,view_formats:&[]});
        let shadow_view=shadow_depth.create_view(&Default::default());
        let shadow_sampler=device.create_sampler(&wgpu::SamplerDescriptor {label:Some("Shadow PCF"),
            mag_filter:wgpu::FilterMode::Linear,min_filter:wgpu::FilterMode::Linear,compare:Some(wgpu::CompareFunction::GreaterEqual),..Default::default()});
        let shadow_binding=device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Sunlight shadow receiver"),layout:&pipeline.get_bind_group_layout(2),entries:&[
            wgpu::BindGroupEntry {binding:0,resource:wgpu::BindingResource::TextureView(&shadow_view)},
            wgpu::BindGroupEntry {binding:1,resource:wgpu::BindingResource::Sampler(&shadow_sampler)}]});
        Self { view: shadow_view, binding: shadow_binding }
    }
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_mesh_render(gpu:*mut OrbGpu,target:u64,id:u64,mvp:*const f32)->i32 {
    if mvp.is_null() {return boundary(||Err("Null camera matrix".into()));}
    let mut draw=OrbMeshDraw {mesh:id,mvp:[0.0;16],sunlight:[0.2,0.5,-1.0,0.35],lights:[bytemuck::Zeroable::zeroed();8],shadow_mvp:[0.0;16],shadow:[0.0;4],eye:[0.0;4]};draw.mvp.copy_from_slice(std::slice::from_raw_parts(mvp,16));
    orb_gpu_mesh_render_scene(gpu,target,&draw,1)
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_mesh_render_scene(gpu:*mut OrbGpu,target:u64,draws:*const OrbMeshDraw,count:usize)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;super::draw::flush(gpu)?;
        if draws.is_null() || count==0 || count>4096 {return Err("Invalid scene draws".into());}
        let draws=std::slice::from_raw_parts(draws,count);
        let shadow_enabled=draws.iter().any(|d|d.shadow[0]!=0.0);
        let shadow_size=if shadow_enabled {2048} else {1};
        let shadow_slot=usize::from(shadow_enabled);
        if gpu.shadow_targets[shadow_slot].is_none() {
            gpu.shadow_targets[shadow_slot]=Some(ShadowTarget::new(&gpu.device,&gpu.mesh_pipeline,shadow_size));
        }
        let target=gpu.surface(target)?;
        if target.texture.format()!=wgpu::TextureFormat::Rgba8Unorm || !target.texture.usage().contains(wgpu::TextureUsages::RENDER_ATTACHMENT) {return Err("Invalid mesh render target".into());}
        let mut prepared=Vec::new();
        for draw in draws {
            if draw.mvp.iter().chain(draw.sunlight.iter()).chain(draw.shadow_mvp.iter()).chain(draw.shadow.iter()).chain(draw.eye.iter()).any(|v|!v.is_finite()) {return Err("Invalid camera matrix".into());}
            let mesh=gpu.meshes.get(&draw.mesh).ok_or("Unknown mesh")?;
            let lights:&[f32]=bytemuck::cast_slice(&draw.lights);
            if lights.iter().any(|v|!v.is_finite()) {return Err("Nonfinite local light".into());}
            let mut camera=[0.0f32;204];camera[..16].copy_from_slice(&draw.mvp);camera[16..20].copy_from_slice(&draw.sunlight);camera[20..180].copy_from_slice(lights);
            camera[180..196].copy_from_slice(&draw.shadow_mvp);camera[196..200].copy_from_slice(&draw.shadow);camera[200..].copy_from_slice(&draw.eye);
            let uniform=gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {label:Some("Camera-relative matrix"),contents:bytemuck::cast_slice(&camera),usage:wgpu::BufferUsages::UNIFORM});
            let bind=gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Mesh camera"),layout:&gpu.mesh_pipeline.get_bind_group_layout(0),entries:&[
                wgpu::BindGroupEntry {binding:0,resource:uniform.as_entire_binding()}]});
            prepared.push((mesh,bind));
        }
        let depth_view=target.depth_view(&gpu.device);
        let stars=gpu.stars.binding(&gpu.device,target.width,target.height);
        let background=gpu.background.binding(&gpu.device,&gpu.mesh_clamp_sampler);
        let star_image=gpu.star_image.binding(&gpu.device,&gpu.mesh_clamp_sampler);
        let color_view=target.texture.create_view(&Default::default());
        let atmosphere=gpu.atmosphere.binding(&gpu.device,depth_view);
        let mut encoder=gpu.device.create_command_encoder(&Default::default());
        let shadow_target=gpu.shadow_targets[shadow_slot].as_ref().unwrap();
        let shadow_view=&shadow_target.view;
        let shadow_binding=&shadow_target.binding;
        let mut casters=Vec::new();
        for (draw,(mesh,_)) in draws.iter().zip(&prepared) {
            if draw.shadow[0]==0.0 {continue;}
            let mut cameras=Vec::new();
            for cascade in 0..2 {
                let factor=if cascade==0 {1.0} else {draw.shadow[2].max(1.0)};
                let mut matrix=draw.shadow_mvp;
                for col in 0..4 {
                    matrix[col*4]/=factor;matrix[col*4+1]/=factor;
                    matrix[col*4+2]=(matrix[col*4+2]-0.5*matrix[col*4+3])/factor+0.5*matrix[col*4+3];
                }
                let uniform=gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {label:Some("Shadow transform"),contents:bytemuck::cast_slice(&matrix),usage:wgpu::BufferUsages::UNIFORM});
                cameras.push(gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Shadow camera"),layout:&gpu.shadow_pipeline.get_bind_group_layout(0),entries:&[
                    wgpu::BindGroupEntry {binding:0,resource:uniform.as_entire_binding()}]}));
            }
            let mut groups=Vec::new();
            for group in &mesh.groups {
                if group.alpha_blend {continue;}
                let view=group.texture.create_view(&Default::default());
                let sampler=if group.clamp_texture {&gpu.mesh_clamp_sampler} else {&gpu.mesh_sampler};
                let binding=gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {label:Some("Shadow alpha mask"),layout:&gpu.shadow_pipeline.get_bind_group_layout(1),entries:&[
                    wgpu::BindGroupEntry {binding:0,resource:wgpu::BindingResource::TextureView(&view)},
                    wgpu::BindGroupEntry {binding:1,resource:wgpu::BindingResource::Sampler(sampler)}]});
                groups.push((group,binding));
            }
            casters.push((mesh,cameras,groups));
        }
        {
            let mut pass=encoder.begin_render_pass(&wgpu::RenderPassDescriptor {label:Some("Sunlight shadow map"),color_attachments:&[],
                depth_stencil_attachment:Some(wgpu::RenderPassDepthStencilAttachment {view:shadow_view,depth_ops:Some(wgpu::Operations {load:wgpu::LoadOp::Clear(0.0),store:wgpu::StoreOp::Store}),stencil_ops:None}),
                timestamp_writes:None,occlusion_query_set:None});
            pass.set_pipeline(&gpu.shadow_pipeline);
            for cascade in 0..2 {
            pass.set_viewport((cascade as u32*shadow_size) as f32,0.0,shadow_size as f32,shadow_size as f32,0.0,1.0);
            pass.set_scissor_rect(cascade as u32*shadow_size,0,shadow_size,shadow_size);
            for (mesh,cameras,groups) in &casters {
                let camera=&cameras[cascade];
                pass.set_bind_group(0,camera,&[]);pass.set_vertex_buffer(0,mesh.vertices.slice(..));pass.set_index_buffer(mesh.indices.slice(..),wgpu::IndexFormat::Uint32);
                for (group,binding) in groups {pass.set_bind_group(1,binding,&[]);pass.draw_indexed(group.first..group.first+group.count,0,0..1);}
            }
            }
        }
        {
            let mut pass=encoder.begin_render_pass(&wgpu::RenderPassDescriptor {label:Some("3D cockpit geometry"),color_attachments:&[Some(wgpu::RenderPassColorAttachment {
                view:&color_view,resolve_target:None,ops:wgpu::Operations {load:wgpu::LoadOp::Clear(wgpu::Color::BLACK),store:wgpu::StoreOp::Store}})],
                depth_stencil_attachment:Some(wgpu::RenderPassDepthStencilAttachment {view:depth_view,depth_ops:Some(wgpu::Operations {load:wgpu::LoadOp::Clear(0.0),store:wgpu::StoreOp::Store}),stencil_ops:None}),
                timestamp_writes:None,occlusion_query_set:None});
            if let Some(binding)=&background {
                pass.set_pipeline(&gpu.background.pipeline);pass.set_bind_group(0,binding,&[]);pass.draw(0..3,0..1);
            }
            if let Some(binding)=&star_image {
                pass.set_pipeline(&gpu.star_image.pipeline);pass.set_bind_group(0,binding,&[]);pass.draw(0..3,0..1);
            }
            if let (Some(binding),Some(vertices))=(&stars,&gpu.stars.vertices) {
                pass.set_pipeline(&gpu.stars.pipeline);pass.set_bind_group(0,binding,&[]);
                pass.set_vertex_buffer(0,vertices.slice(..));pass.draw(0..6,0..gpu.stars.count);
            }
            pass.set_pipeline(&gpu.mesh_pipeline);
            pass.set_bind_group(2,shadow_binding,&[]);
            for (mesh,camera) in &prepared {
                pass.set_bind_group(0,camera,&[]);pass.set_vertex_buffer(0,mesh.vertices.slice(..));pass.set_index_buffer(mesh.indices.slice(..),wgpu::IndexFormat::Uint32);
                for group in &mesh.groups {if group.alpha_blend || group.shadow_only {continue;}pass.set_bind_group(1,&group.binding,&[]);pass.draw_indexed(group.first..group.first+group.count,0,0..1);}
            }
        }
        if let Some(binding)=&atmosphere {
            // Depth is sampled here, so it must not also be an attachment.
            let mut pass=encoder.begin_render_pass(&wgpu::RenderPassDescriptor {label:Some("Atmospheric aerial perspective"),color_attachments:&[Some(wgpu::RenderPassColorAttachment {
                view:&color_view,resolve_target:None,ops:wgpu::Operations {load:wgpu::LoadOp::Load,store:wgpu::StoreOp::Store}})],
                depth_stencil_attachment:None,timestamp_writes:None,occlusion_query_set:None});
            pass.set_pipeline(&gpu.atmosphere.pipeline);pass.set_bind_group(0,binding,&[]);pass.draw(0..3,0..1);
        }
        let mut transparent=Vec::new();
        for (mesh_index,((mesh,_),draw)) in prepared.iter().zip(draws).enumerate() {
            for (group_index,group) in mesh.groups.iter().enumerate() {
                if !group.alpha_blend || group.shadow_only {continue;}
                let p=group.sort_center;
                let depth=draw.mvp[3]*p[0]+draw.mvp[7]*p[1]+draw.mvp[11]*p[2]+draw.mvp[15];
                transparent.push((mesh_index,group_index,depth));
            }
        }
        transparent.sort_by(|a,b|b.2.total_cmp(&a.2));
        enum TransparentDraw {Mesh(usize),Particles(usize,std::ops::Range<u32>)}
        let mut commands=Vec::new();let mut cursor=0u32;let mut run=0usize;
        // Merge sorted group centres with particle depths, splitting texture
        // runs only where a transparent group must be inserted between particles.
        for index in 0..=transparent.len() {
            let end=if index==transparent.len() {gpu.particles.count} else {
                let depth=transparent[index].2;
                gpu.particles.depths.partition_point(|&z|z>=depth) as u32
            };
            while cursor<end {
                let finish=end.min(gpu.particles.runs[run].0.end);
                commands.push(TransparentDraw::Particles(run,cursor..finish));cursor=finish;
                if cursor==gpu.particles.runs[run].0.end {run+=1;}
            }
            if index<transparent.len() {commands.push(TransparentDraw::Mesh(index));}
        }
        if !commands.is_empty() {
            let mut pass=encoder.begin_render_pass(&wgpu::RenderPassDescriptor {label:Some("Sorted transparent scene"),color_attachments:&[Some(wgpu::RenderPassColorAttachment {
                view:&color_view,resolve_target:None,ops:wgpu::Operations {load:wgpu::LoadOp::Load,store:wgpu::StoreOp::Store}})],
                depth_stencil_attachment:Some(wgpu::RenderPassDepthStencilAttachment {view:depth_view,depth_ops:Some(wgpu::Operations {load:wgpu::LoadOp::Load,store:wgpu::StoreOp::Store}),stencil_ops:None}),
                timestamp_writes:None,occlusion_query_set:None});
            for command in commands {match command {
            TransparentDraw::Mesh(index)=> {
                let (mesh_index,group_index,_)=transparent[index];
                let (mesh,camera)=&prepared[mesh_index];
                let group=&mesh.groups[group_index];
                pass.set_pipeline(&gpu.mesh_alpha_pipeline);pass.set_bind_group(2,shadow_binding,&[]);
                pass.set_bind_group(0,camera,&[]);pass.set_vertex_buffer(0,mesh.vertices.slice(..));pass.set_index_buffer(mesh.indices.slice(..),wgpu::IndexFormat::Uint32);
                pass.set_bind_group(1,&group.binding,&[]);pass.draw_indexed(group.first..group.first+group.count,0,0..1);
            }
            TransparentDraw::Particles(run,range)=> {
                pass.set_pipeline(&gpu.particles.pipeline);pass.set_vertex_buffer(0,gpu.particles.vertices.as_ref().unwrap().slice(..));
                pass.set_bind_group(0,&gpu.particles.runs[run].1,&[]);pass.draw(0..6,range);
            }
            }}
        }
        gpu.queue.submit([encoder.finish()]);Ok(1)
    })
}

#[no_mangle]
pub unsafe extern "C" fn orb_gpu_mesh_update(gpu:*mut OrbGpu,id:u64,vertices:*const OrbMeshVertex,count:usize)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;let mesh=gpu.meshes.get(&id).ok_or("Unknown mesh")?;
        if vertices.is_null() || count!=mesh.nvertices {return Err("Mesh update vertex count mismatch".into());}
        let vertices=std::slice::from_raw_parts(vertices,count);
        if vertices.iter().any(|v|!v.finite()) {return Err("Nonfinite animated vertex".into());}
        gpu.queue.write_buffer(&mesh.vertices,0,bytemuck::cast_slice(vertices));Ok(1)
    })
}

#[repr(C)]
pub struct OrbMeshRange {first:u32,count:u32}
const _: ()=assert!(std::mem::size_of::<OrbMeshRange>()==8);
// One FFI batch per mesh/frame. Validate every changed range before any upload.
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_mesh_update_ranges(gpu:*mut OrbGpu,id:u64,vertices:*const OrbMeshVertex,count:usize,ranges:*const OrbMeshRange,range_count:usize)->i32 {
    boundary(|| {
        let gpu=context(gpu)?;let mesh=gpu.meshes.get(&id).ok_or("Unknown mesh")?;
        if vertices.is_null() || count!=mesh.nvertices || ranges.is_null() || range_count==0 || range_count>count {return Err("Invalid mesh update ranges".into());}
        let vertices=std::slice::from_raw_parts(vertices,count);
        let ranges=std::slice::from_raw_parts(ranges,range_count);
        for range in ranges {
            let end=range.first.checked_add(range.count).ok_or("Mesh range overflow")? as usize;
            if range.count==0 || end>count {return Err("Mesh range out of bounds".into());}
            if vertices[range.first as usize..end].iter().any(|v|!v.finite()) {return Err("Nonfinite animated vertex".into());}
        }
        for range in ranges {
            gpu.queue.write_buffer(&mesh.vertices,range.first as u64*96,bytemuck::cast_slice(&vertices[range.first as usize..(range.first+range.count) as usize]));
        }
        Ok(1)
    })
}
