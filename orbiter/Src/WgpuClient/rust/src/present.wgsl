@group(0) @binding(0) var image: texture_2d<f32>;
@group(0) @binding(1) var image_sampler: sampler;
struct Out { @builtin(position) position: vec4f, @location(0) uv: vec2f };
@vertex fn vs(@builtin(vertex_index) vertex: u32) -> Out {
    let positions = array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
    var out: Out;
    let p=positions[vertex];
    out.position=vec4f(p,0,1);
    out.uv=vec2f((p.x+1)*0.5,(1-p.y)*0.5);
    return out;
}
@fragment fn fs(in: Out) -> @location(0) vec4f {
    return textureSample(image,image_sampler,in.uv);
}
