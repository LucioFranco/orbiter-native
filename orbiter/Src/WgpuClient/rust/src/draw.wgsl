@group(0) @binding(0) var source: texture_2d<f32>;
struct Out {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
    @location(2) @interpolate(flat) key: u32,
    @location(3) @interpolate(flat) flags: u32,
};
@vertex fn vs(@location(0) position: vec2f, @location(1) uv: vec2f,
    @location(2) color: vec4f, @location(3) key: u32, @location(4) flags: u32) -> Out {
    var out: Out;
    out.position=vec4f(position,0,1); out.uv=uv; out.color=color; out.key=key; out.flags=flags;
    return out;
}
@fragment fn fs(in: Out) -> @location(0) vec4f {
    if ((in.flags & 1u)==0u) { return in.color; }
    let at=vec2i(floor(in.uv));
    let size=vec2i(textureDimensions(source));
    if(any(at<vec2i(0)) || any(at>=size)) { discard; }
    let color=textureLoad(source,at,0);
    let rgb=vec3u(round(color.rgb*255.0));
    let key=(rgb.r<<16u)|(rgb.g<<8u)|rgb.b;
    if ((in.flags & 2u)!=0u && key==in.key) { discard; }
    return color;
}
