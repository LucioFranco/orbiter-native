// Legacy DDS layouts: https://learn.microsoft.com/windows/win32/direct3ddds/dds-header
use super::{boundary,context,OrbGpu,Surface};
#[path="dds_mips.rs"] mod mips;
fn word(data:&[u8],at:usize)->Result<u32,String> {
    Ok(u32::from_le_bytes(data.get(at..at+4).ok_or("Truncated DDS header")?.try_into().unwrap()))
}
struct Image { width:u32,height:u32,format:wgpu::TextureFormat,levels:Vec<Vec<u8>> }
fn decode(data:&[u8])->Result<Image,String> {
    if data.len()<128 || &data[..4]!=b"DDS " || word(data,4)?!=124 || word(data,76)?!=32 {return Err("Invalid DDS header".into());}
    let width=word(data,16)?;let height=word(data,12)?;
    if width==0 || height==0 || width>16384 || height>16384 {return Err("Invalid DDS dimensions".into());}
    if word(data,24)?>1 || word(data,112)? & (0x200000|0xfe00)!=0 {return Err("DDS volume/cubemap is not a 2D cockpit texture".into());}
    let count=word(data,28)?.max(1);
    if count>32-width.max(height).leading_zeros() {return Err("Invalid DDS mip count".into());}
    let flags=word(data,80)?;
    let format=if flags&4!=0 {
        match &data[84..88] {
            b"DXT1"=>wgpu::TextureFormat::Bc1RgbaUnorm,
            b"DXT3"=>wgpu::TextureFormat::Bc2RgbaUnorm,
            b"DXT5"=>wgpu::TextureFormat::Bc3RgbaUnorm,
            _=>return Err("Unsupported DDS FourCC".into()),
        }
    } else if flags&0x40!=0 && word(data,88)?==32 {wgpu::TextureFormat::Rgba8Unorm}
    else {return Err("Unsupported DDS pixel format".into());};
    let compressed=format!=wgpu::TextureFormat::Rgba8Unorm;
    let masks=[word(data,92)?,word(data,96)?,word(data,100)?,if flags&1!=0 {word(data,104)?} else {0}];
    if !compressed {
        let mut used=0;
        for (i,&mask) in masks.iter().enumerate() {
            if mask==0 {if i<3{return Err("Missing DDS colour mask".into());}continue;}
            if mask.count_ones()!=8 || (mask>>mask.trailing_zeros())!=255 || used&mask!=0 {return Err("Unsupported DDS channel masks".into());}
            used|=mask;
        }
    }
    let mut levels=Vec::new();let mut offset=128usize;
    for level in 0..count {
        let w=(width>>level).max(1);let h=(height>>level).max(1);
        let bytes=if compressed {
            w.div_ceil(4) as usize*h.div_ceil(4) as usize*if format==wgpu::TextureFormat::Bc1RgbaUnorm {8}else{16}
        } else {
            let pitch=if level==0 && word(data,8)?&8!=0 {word(data,20)?} else {w*4};
            if pitch<w*4 {return Err("Invalid DDS pitch".into());}
            pitch as usize*h as usize
        };
        let input=data.get(offset..offset.checked_add(bytes).ok_or("DDS size overflow")?).ok_or("Truncated DDS mip data")?;
        offset+=bytes;
        if compressed {levels.push(input.to_vec());} else {
            let pitch=bytes/h as usize;let mut output=Vec::with_capacity(w as usize*h as usize*4);
            for y in 0..h as usize {for x in 0..w as usize {
                let pixel=word(input,y*pitch+x*4)?;
                for &mask in &masks {output.push(if mask==0 {255}else{((pixel&mask)>>mask.trailing_zeros()) as u8});}
            }}
            levels.push(output);
        }
    }
    // wgpu/Metal require BC base dimensions aligned to a block. NASSP has
    // several legacy DXT1 images with partial edge blocks; crop decoded pixels.
    if compressed && (width%4!=0 || height%4!=0) {
        levels=levels.iter().enumerate().map(|(level,bytes)|bc_decode(bytes,(width>>level).max(1),(height>>level).max(1),format)).collect();
        return Ok(Image {width,height,format:wgpu::TextureFormat::Rgba8Unorm,levels});
    }
    Ok(Image {width,height,format,levels})
}
fn bc_decode(bytes:&[u8],width:u32,height:u32,format:wgpu::TextureFormat)->Vec<u8> {
    let bc1=format==wgpu::TextureFormat::Bc1RgbaUnorm;
    let bc2=format==wgpu::TextureFormat::Bc2RgbaUnorm;
    let mut output=vec![0;width as usize*height as usize*4];
    let rgb=|v:u16|-> [u8;4] {
        let r=((v>>11)&31) as u8;let g=((v>>5)&63) as u8;let b=(v&31) as u8;
        [(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2),255]
    };
    for by in 0..height.div_ceil(4) {for bx in 0..width.div_ceil(4) {
        let at=((by*width.div_ceil(4)+bx)*if bc1 {8}else{16}) as usize;
        let block=&bytes[at+if bc1 {0}else{8}..at+if bc1 {8}else{16}];
        let alpha=if bc1 {0}else{u64::from_le_bytes(bytes[at..at+8].try_into().unwrap())};
        let mut alphas=[0u8;8];
        if !bc1 && !bc2 {
            alphas[0]=bytes[at];alphas[1]=bytes[at+1];
            if alphas[0]>alphas[1] {
                for i in 1..7 {alphas[i+1]=(((7-i) as u16*alphas[0] as u16+i as u16*alphas[1] as u16)/7) as u8;}
            } else {
                for i in 1..5 {alphas[i+1]=(((5-i) as u16*alphas[0] as u16+i as u16*alphas[1] as u16)/5) as u8;}
                alphas[6]=0;alphas[7]=255;
            }
        }
        let a=u16::from_le_bytes([block[0],block[1]]);let b=u16::from_le_bytes([block[2],block[3]]);
        let mut colors=[rgb(a),rgb(b),[0,0,0,255],[0,0,0,0]];
        let four_colors=!bc1 || a>b;
        for c in 0..3 {
            colors[2][c]=if four_colors {((2*colors[0][c] as u16+colors[1][c] as u16)/3) as u8} else {((colors[0][c] as u16+colors[1][c] as u16)/2) as u8};
            if four_colors {colors[3][c]=((colors[0][c] as u16+2*colors[1][c] as u16)/3) as u8;}
        }
        if four_colors {colors[3][3]=255;}
        let selectors=u32::from_le_bytes(block[4..8].try_into().unwrap());
        for y in 0..4 {for x in 0..4 {
            if bx*4+x>=width || by*4+y>=height {continue;}
            let dest=(((by*4+y)*width+bx*4+x)*4) as usize;
            let index=((selectors>>(2*(y*4+x)))&3) as usize;
            output[dest..dest+4].copy_from_slice(&colors[index]);
            if !bc1 {output[dest+3]=if bc2 {(((alpha>>(4*(y*4+x)))&15)*17) as u8}
                else {alphas[((alpha>>(16+3*(y*4+x)))&7) as usize]};}
        }}
    }}
    output
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_dds_create(gpu:*mut OrbGpu,data:*const u8,size:usize)->u64 {
    create(gpu,data,size,false)
}
#[no_mangle]
pub unsafe extern "C" fn orb_gpu_dds_create_mipped(gpu:*mut OrbGpu,data:*const u8,size:usize)->u64 {
    create(gpu,data,size,true)
}
unsafe fn create(gpu:*mut OrbGpu,data:*const u8,size:usize,complete_mips:bool)->u64 {
    boundary(|| {
        let gpu=context(gpu)?;
        if data.is_null() || size>isize::MAX as usize {return Err("Invalid DDS buffer".into());}
        let mut image=decode(std::slice::from_raw_parts(data,size))?;
        if complete_mips {mips::complete(&mut image);}
        if image.width>gpu.device.limits().max_texture_dimension_2d || image.height>gpu.device.limits().max_texture_dimension_2d {return Err("DDS exceeds GPU dimensions".into());}
        let compressed=image.format!=wgpu::TextureFormat::Rgba8Unorm;
        if compressed && !gpu.device.features().contains(wgpu::Features::TEXTURE_COMPRESSION_BC) {return Err("GPU lacks BC texture support".into());}
        let texture=gpu.device.create_texture(&wgpu::TextureDescriptor {
            label:Some("DDS cockpit texture"),size:wgpu::Extent3d {width:image.width,height:image.height,depth_or_array_layers:1},
            mip_level_count:image.levels.len() as u32,sample_count:1,dimension:wgpu::TextureDimension::D2,format:image.format,
            usage:wgpu::TextureUsages::COPY_DST|wgpu::TextureUsages::TEXTURE_BINDING,view_formats:&[],
        });
        for (level,bytes) in image.levels.iter().enumerate() {
            let w=(image.width>>level).max(1);let h=(image.height>>level).max(1);
            let block=if image.format==wgpu::TextureFormat::Bc1RgbaUnorm {8}else{16};
            let pitch=if compressed {w.div_ceil(4)*block}else{w*4};
            gpu.queue.write_texture(wgpu::TexelCopyTextureInfo {texture:&texture,mip_level:level as u32,origin:wgpu::Origin3d::ZERO,aspect:wgpu::TextureAspect::All},bytes,
                wgpu::TexelCopyBufferLayout {offset:0,bytes_per_row:Some(pitch),rows_per_image:None},
                wgpu::Extent3d {width:if compressed {w.div_ceil(4)*4}else{w},height:if compressed {h.div_ceil(4)*4}else{h},depth_or_array_layers:1});
        }
        let id=gpu.next_id;gpu.next_id=id.checked_add(1).ok_or("Surface handle exhaustion")?;
        gpu.surfaces.insert(id,Surface {texture,width:image.width,height:image.height,depth:Default::default()});Ok(id)
    })
}
