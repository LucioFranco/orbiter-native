use crate::{boundary, context, draw, OrbGpu};
use wgpu::util::DeviceExt;
use std::collections::HashMap;

#[repr(C)]
#[derive(Clone, Copy, bytemuck::Pod, bytemuck::Zeroable)]
pub struct OrbUiVertex { position: [f32;2], uv: [f32;2], color: [u8;4] }
#[repr(C)]
pub struct OrbUiCommand { texture: u64, first: u32, count: u32, base_vertex: i32, clip: [u32;4], reserved: u32 }
const _: () = assert!(std::mem::size_of::<OrbUiVertex>()==20 && std::mem::size_of::<OrbUiCommand>()==40);

pub struct Ui { pipeline: wgpu::RenderPipeline, sampler: wgpu::Sampler }
impl Ui {
    pub fn new(device: &wgpu::Device) -> Self {
        let shader=device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("ImGui shader"), source: wgpu::ShaderSource::Wgsl(include_str!("ui.wgsl").into()),
        });
        let attributes=wgpu::vertex_attr_array![0 => Float32x2, 1 => Float32x2, 2 => Unorm8x4];
        let pipeline=device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("ImGui overlay"), layout: None,
            vertex: wgpu::VertexState {module: &shader, entry_point: Some("vs"), compilation_options: Default::default(),
                buffers: &[wgpu::VertexBufferLayout {array_stride: 20, step_mode: wgpu::VertexStepMode::Vertex, attributes: &attributes}]},
            fragment: Some(wgpu::FragmentState {module: &shader, entry_point: Some("fs"), compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState {format: wgpu::TextureFormat::Rgba8Unorm,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING), write_mask: wgpu::ColorWrites::ALL})]}),
            primitive: Default::default(), depth_stencil: None, multisample: Default::default(), multiview: None, cache: None,
        });
        let sampler=device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("ImGui sampler"), mag_filter: wgpu::FilterMode::Linear, min_filter: wgpu::FilterMode::Linear,
            ..Default::default()
        });
        Self {pipeline,sampler}
    }
}

// One call for all ImDrawData lists. C++ converts positions to NDC and scissor
// rectangles to framebuffer pixels; all indices are widened to u32.
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_ui_render(gpu: *mut OrbGpu, target: u64,
    vertices: *const OrbUiVertex, vertex_count: usize, indices: *const u32, index_count: usize,
    commands: *const OrbUiCommand, command_count: usize) -> i32 {
    boundary(|| {
        let gpu=context(gpu)?;
        if command_count==0 {return Ok(1);}
        let limit=gpu.device.limits().max_buffer_size as usize;
        if vertices.is_null() || indices.is_null() || commands.is_null() || vertex_count==0 || index_count==0 ||
            vertex_count>limit/20 || index_count>limit/4 || command_count>limit/40 {
            return Err("Invalid ImGui batch buffers".into());
        }
        let vertices=std::slice::from_raw_parts(vertices,vertex_count);
        let indices=std::slice::from_raw_parts(indices,index_count);
        let commands=std::slice::from_raw_parts(commands,command_count);
        if vertices.iter().any(|v| v.position.iter().chain(v.uv.iter()).any(|n| !n.is_finite())) {
            return Err("Nonfinite ImGui vertex".into());
        }
        let surface=gpu.surface(target)?;
        if surface.texture.format()!=wgpu::TextureFormat::Rgba8Unorm {return Err("ImGui target must be RGBA8".into());}
        let (width,height)=(surface.width,surface.height);
        let mut bindings=HashMap::new();
        for command in commands {
            let end=command.first as usize+command.count as usize;
            if end>indices.len() || command.count%3!=0 || command.base_vertex<0 || command.reserved!=0 {
                return Err("Invalid ImGui index range".into());
            }
            if indices[command.first as usize..end].iter().any(|&i| i as u64+command.base_vertex as u64>=vertex_count as u64) {
                return Err("ImGui index exceeds vertex buffer".into());
            }
            if command.texture==target {return Err("ImGui cannot sample its target".into());}
            if let std::collections::hash_map::Entry::Vacant(entry)=bindings.entry(command.texture) {
                let texture=gpu.surface(command.texture)?;
                entry.insert(gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {
                    label: Some("ImGui texture"), layout: &gpu.ui.pipeline.get_bind_group_layout(0),
                    entries: &[
                        wgpu::BindGroupEntry {binding:0,resource:wgpu::BindingResource::TextureView(&texture.texture.create_view(&Default::default()))},
                        wgpu::BindGroupEntry {binding:1,resource:wgpu::BindingResource::Sampler(&gpu.ui.sampler)},
                    ],
                }));
            }
        }
        // Validate the whole batch before touching the output.
        draw::flush(gpu)?;
        let view=gpu.surface(target)?.texture.create_view(&Default::default());
        let vb=gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label:Some("ImGui vertices"),contents:bytemuck::cast_slice(vertices),usage:wgpu::BufferUsages::VERTEX});
        let ib=gpu.device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label:Some("ImGui indices"),contents:bytemuck::cast_slice(indices),usage:wgpu::BufferUsages::INDEX});
        let mut encoder=gpu.device.create_command_encoder(&Default::default());
        {
            let mut pass=encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label:Some("ImGui overlay"),color_attachments:&[Some(wgpu::RenderPassColorAttachment {
                    view:&view,resolve_target:None,ops:wgpu::Operations {load:wgpu::LoadOp::Load,store:wgpu::StoreOp::Store}})],
                depth_stencil_attachment:None,timestamp_writes:None,occlusion_query_set:None,
            });
            pass.set_pipeline(&gpu.ui.pipeline);pass.set_vertex_buffer(0,vb.slice(..));pass.set_index_buffer(ib.slice(..),wgpu::IndexFormat::Uint32);
            for command in commands {
                let [x,y,w,h]=command.clip;
                let x=x.min(width);let y=y.min(height);let w=w.min(width-x);let h=h.min(height-y);
                if w==0 || h==0 || command.count==0 {continue;}
                pass.set_scissor_rect(x,y,w,h);pass.set_bind_group(0,&bindings[&command.texture],&[]);
                pass.draw_indexed(command.first..command.first+command.count,command.base_vertex,0..1);
            }
        }
        gpu.queue.submit(Some(encoder.finish()));Ok(1)
    })
}
