use super::{bc_decode,Image};
// Generate only missing lower levels. Original compressed blocks are untouched.
// The small BC1 encoder uses bounding-box endpoints and nearest-palette colours;
// this is a loading-time fallback, not an offline production texture compressor.
fn encode_bc1(pixels:&[u8],width:u32,height:u32)->Vec<u8> {
    let mut output=Vec::new();
    let pack=|p:[u8;3]|->u16 {((p[0] as u16*31+127)/255)<<11 | ((p[1] as u16*63+127)/255)<<5 | (p[2] as u16*31+127)/255};
    let unpack=|v:u16|->[i32;3] {let r=((v>>11)&31) as i32;let g=((v>>5)&63) as i32;let b=(v&31) as i32;[(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)]};
    for by in 0..height.div_ceil(4) {for bx in 0..width.div_ceil(4) {
        let mut block=[[0u8;4];16];let mut low=[255u8;3];let mut high=[0u8;3];let mut alpha=false;
        for y in 0..4 {for x in 0..4 {
            let src=(((by*4+y).min(height-1)*width+(bx*4+x).min(width-1))*4) as usize;
            let p=&pixels[src..src+4];block[(y*4+x) as usize].copy_from_slice(p);
            if p[3]<128 {alpha=true;} else {for c in 0..3 {low[c]=low[c].min(p[c]);high[c]=high[c].max(p[c]);}}
        }}
        let mut a=pack(high);let mut b=pack(low);
        if alpha {if a>b {std::mem::swap(&mut a,&mut b);}}
        else if a<=b {if b==65535 {b-=1;a=65535;} else {a=b+1;}}
        let mut palette=[unpack(a),unpack(b),[0;3],[0;3]];
        for c in 0..3 {
            palette[2][c]=if alpha {(palette[0][c]+palette[1][c])/2}else{(2*palette[0][c]+palette[1][c])/3};
            palette[3][c]=(palette[0][c]+2*palette[1][c])/3;
        }
        let mut selectors=0u32;
        for (i,p) in block.iter().enumerate() {
            let index=if alpha && p[3]<128 {3} else {
                (0..if alpha {3}else{4}).min_by_key(|&k| (0..3).map(|c|(p[c] as i32-palette[k][c]).pow(2)).sum::<i32>()).unwrap()
            };
            selectors|=(index as u32)<<(2*i);
        }
        output.extend(a.to_le_bytes());output.extend(b.to_le_bytes());output.extend(selectors.to_le_bytes());
    }}
    output
}
pub(super) fn complete(image:&mut Image) {
    let bc1=image.format==wgpu::TextureFormat::Bc1RgbaUnorm;
    if !bc1 && image.format!=wgpu::TextureFormat::Rgba8Unorm {return;}
    // Standard planet tiles are power-of-two. Preserve other asset conventions.
    if !image.width.is_power_of_two() || !image.height.is_power_of_two() {return;}
    let last=image.levels.len()-1;
    let mut width=(image.width>>last).max(1);let mut height=(image.height>>last).max(1);
    let mut pixels=if bc1 {bc_decode(&image.levels[last],width,height,image.format)}else{image.levels[last].clone()};
    while width>1 || height>1 {
        let w=(width/2).max(1);let h=(height/2).max(1);let mut next=vec![0u8;(w*h*4) as usize];
        for y in 0..h {for x in 0..w {
            let mut sum=[0u32;4];
            for dy in 0..2 {for dx in 0..2 {
                let at=(((y*2+dy).min(height-1)*width+(x*2+dx).min(width-1))*4) as usize;
                for c in 0..3 {sum[c]+=pixels[at+c] as u32*pixels[at+3] as u32;}
                sum[3]+=pixels[at+3] as u32;
            }}
            let at=((y*w+x)*4) as usize;
            for c in 0..3 {next[at+c]=if sum[3]>0 {((sum[c]+sum[3]/2)/sum[3]) as u8}else{0};}
            next[at+3]=((sum[3]+2)/4) as u8;
        }}
        image.levels.push(if bc1 {encode_bc1(&next,w,h)}else{next.clone()});
        pixels=next;width=w;height=h;
    }
}
