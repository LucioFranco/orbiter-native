@group(0) @binding(0) var sprite:texture_2d<f32>;
@group(0) @binding(1) var sprite_sampler:sampler;
struct Out {
    @builtin(position) position:vec4<f32>,
    @location(0) uv:vec2<f32>,
    @location(1) color:vec4<f32>,
    @location(2) texture_uv:vec2<f32>,
};
@vertex fn vs(@builtin(vertex_index) index:u32,
    @location(0) center:vec4<f32>,@location(1) extent:vec2<f32>,
    @location(2) color:vec4<f32>,@location(3) sprite_index:u32) -> Out {
    let corners=array<vec2<f32>,6>(vec2(-1.,-1.),vec2(1.,-1.),vec2(-1.,1.),vec2(-1.,1.),vec2(1.,-1.),vec2(1.,1.));
    var out:Out;
    out.position=center+vec4(corners[index]*extent,0.,0.);
    out.uv=corners[index];out.color=color;
    var uv=corners[index]*vec2(0.5,-0.5)+vec2(0.5);
    if(sprite_index>0u) {
        let variant=sprite_index-1u;
        if(variant>=4u) {uv=vec2(1.-uv.y,uv.x);}
        let cell=variant%4u;
        uv=(uv+vec2(f32(cell%2u),f32(cell/2u)))*0.5;
    }
    out.texture_uv=uv;return out;
}
@fragment fn fs(in:Out)->@location(0) vec4<f32> {
    let radius=dot(in.uv,in.uv);
    if(radius>=1.) {discard;}
    let fade=(1.-radius)*(1.-radius);
    let texel=textureSample(sprite,sprite_sampler,in.texture_uv);
    return vec4(in.color.rgb*texel.rgb,in.color.a*texel.a*fade);
}
