use super::{boundary, context, OrbGpu};
use raw_window_handle::{RawDisplayHandle, RawWindowHandle};
use std::{ffi::c_void, num::NonZeroIsize, ptr::NonNull};
#[repr(C)]
pub struct OrbNativeWindow {
    kind: u32, width: u32, height: u32, screen: i32,
    window: *mut c_void, display: *mut c_void, id: u64,
}
pub(super) struct Presentation {
    surface: wgpu::Surface<'static>, config: wgpu::SurfaceConfiguration,
    pipeline: wgpu::RenderPipeline, sampler: wgpu::Sampler, suspended: bool,
}
unsafe fn target(window: &OrbNativeWindow) -> Result<wgpu::SurfaceTargetUnsafe, String> {
    let handles = match window.kind {
        #[cfg(target_os = "macos")]
        1 => {
            if window.window.is_null() { return Err("Null Metal layer".into()); }
            return Ok(wgpu::SurfaceTargetUnsafe::CoreAnimationLayer(window.window));
        }
        2 => {
            let mut handle = raw_window_handle::Win32WindowHandle::new(
                NonZeroIsize::new(window.window as isize).ok_or("Null HWND")?);
            handle.hinstance = NonZeroIsize::new(window.display as isize);
            (RawWindowHandle::Win32(handle), RawDisplayHandle::Windows(raw_window_handle::WindowsDisplayHandle::new()))
        }
        3 => {
            if window.id == 0 { return Err("Null Xlib window".into()); }
            (RawWindowHandle::Xlib(raw_window_handle::XlibWindowHandle::new(window.id as _)),
             RawDisplayHandle::Xlib(raw_window_handle::XlibDisplayHandle::new(NonNull::new(window.display), window.screen)))
        }
        4 => (
            RawWindowHandle::Wayland(raw_window_handle::WaylandWindowHandle::new(NonNull::new(window.window).ok_or("Null Wayland surface")?)),
            RawDisplayHandle::Wayland(raw_window_handle::WaylandDisplayHandle::new(NonNull::new(window.display).ok_or("Null Wayland display")?))),
        _ => return Err("Unsupported native window kind".into()),
    };
    Ok(wgpu::SurfaceTargetUnsafe::RawHandle { raw_window_handle: handles.0, raw_display_handle: handles.1 })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_window_attach(gpu: *mut OrbGpu, window: *const OrbNativeWindow) -> i32 {
    boundary(|| {
        let gpu = context(gpu)?;
        let window = window.as_ref().ok_or("Null window descriptor")?;
        let surface = gpu.instance.create_surface_unsafe(target(window)?).map_err(|e| e.to_string())?;
        let caps = surface.get_capabilities(&gpu.adapter);
        // Legacy surface bytes carry display RGB values, so prefer a linear
        // swapchain to avoid applying another sRGB encoding during presentation.
        let format = caps.formats.iter().copied().find(|f| !f.is_srgb()).or(caps.formats.first().copied())
            .ok_or("Adapter cannot present to this window")?;
        let config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT, format,
            width: window.width.max(1), height: window.height.max(1),
            present_mode: wgpu::PresentMode::Fifo, desired_maximum_frame_latency: 2,
            alpha_mode: *caps.alpha_modes.first().ok_or("No surface alpha mode")?, view_formats: vec![],
        };
        if config.width > gpu.device.limits().max_texture_dimension_2d || config.height > gpu.device.limits().max_texture_dimension_2d {
            return Err("Window exceeds GPU limits".into());
        }
        surface.configure(&gpu.device, &config);
        let shader = gpu.device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("Surface presentation"), source: wgpu::ShaderSource::Wgsl(include_str!("present.wgsl").into()),
        });
        let pipeline = gpu.device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("Surface presentation"), layout: None,
            vertex: wgpu::VertexState { module: &shader, entry_point: Some("vs"), compilation_options: Default::default(), buffers: &[] },
            fragment: Some(wgpu::FragmentState { module: &shader, entry_point: Some("fs"), compilation_options: Default::default(),
                targets: &[Some(wgpu::ColorTargetState { format, blend: None, write_mask: wgpu::ColorWrites::ALL })] }),
            primitive: Default::default(), depth_stencil: None, multisample: Default::default(), multiview: None, cache: None,
        });
        let sampler = gpu.device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("Presentation sampler"), min_filter: wgpu::FilterMode::Linear, mag_filter: wgpu::FilterMode::Linear,
            ..Default::default()
        });
        gpu.presentation = Some(Presentation { surface, config, pipeline, sampler,
            suspended: window.width == 0 || window.height == 0 });
        Ok(1)
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_window_detach(gpu: *mut OrbGpu) {
    boundary(|| { context(gpu)?.presentation = None; Ok(()) });
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_window_resize(gpu: *mut OrbGpu, width: u32, height: u32) -> i32 {
    boundary(|| {
        let gpu = context(gpu)?;
        let p = gpu.presentation.as_mut().ok_or("No presentation window")?;
        p.suspended = width == 0 || height == 0;
        if p.suspended { return Ok(1); }
        if width > gpu.device.limits().max_texture_dimension_2d || height > gpu.device.limits().max_texture_dimension_2d {
            return Err("Window exceeds GPU limits".into());
        }
        if p.config.width != width || p.config.height != height {
            p.config.width = width; p.config.height = height;
            p.surface.configure(&gpu.device, &p.config);
        }
        Ok(1)
    })
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_present(gpu: *mut OrbGpu, image: u64) -> i32 {
    boundary(|| {
        let gpu = context(gpu)?;
        super::draw::flush(gpu)?;
        let p = gpu.presentation.as_ref().ok_or("No presentation window")?;
        if p.suspended { return Ok(1); }
        let frame = match p.surface.get_current_texture() {
            Ok(frame) => frame,
            Err(wgpu::SurfaceError::Timeout) => return Ok(1),
            Err(wgpu::SurfaceError::Lost | wgpu::SurfaceError::Outdated) => {
                p.surface.configure(&gpu.device, &p.config);
                return Ok(1);
            }
            Err(error) => return Err(error.to_string()),
        };
        let view = frame.texture.create_view(&Default::default());
        let source = if image == 0 { None } else { Some(gpu.surface(image)?) };
        let source_view = source.map(|s| s.texture.create_view(&wgpu::TextureViewDescriptor {
            // Decode source bytes if the platform only offers an sRGB swapchain.
            format: Some(if p.config.format.is_srgb() { wgpu::TextureFormat::Rgba8UnormSrgb } else { wgpu::TextureFormat::Rgba8Unorm }),
            ..Default::default()
        }));
        let group = source_view.as_ref().map(|v| gpu.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("Presentation source"), layout: &p.pipeline.get_bind_group_layout(0), entries: &[
                wgpu::BindGroupEntry { binding: 0, resource: wgpu::BindingResource::TextureView(v) },
                wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::Sampler(&p.sampler) },
            ],
        }));
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("Present"), color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view, resolve_target: None,
                    ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::BLACK), store: wgpu::StoreOp::Store },
                })], depth_stencil_attachment: None, timestamp_writes: None, occlusion_query_set: None,
            });
            if let (Some(source), Some(group)) = (source, group.as_ref()) {
                // Preserve panel aspect ratio, letterboxing as needed.
                let scale = (p.config.width as f32 / source.width as f32).min(p.config.height as f32 / source.height as f32);
                let width = source.width as f32 * scale; let height = source.height as f32 * scale;
                pass.set_viewport((p.config.width as f32-width)*0.5, (p.config.height as f32-height)*0.5, width, height, 0.0, 1.0);
                pass.set_pipeline(&p.pipeline); pass.set_bind_group(0, group, &[]); pass.draw(0..3, 0..1);
            }
        }
        gpu.queue.submit([encoder.finish()]); frame.present();
        Ok(1)
    })
}
