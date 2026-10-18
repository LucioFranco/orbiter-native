@group(0) @binding(0) var<uniform> matrix:mat4x4f;
@group(1) @binding(0) var image:texture_2d<f32>;
@group(1) @binding(1) var image_sampler:sampler;
struct Out { @builtin(position) position:vec4f, @location(0) uv:vec2f, @location(1) alpha:f32 };
@vertex fn vs(@location(0) position:vec3f,@location(1) uv:vec2f,@location(2) color:vec4f)->Out {
    var out:Out;out.position=matrix*vec4f(position,1);out.uv=uv;out.alpha=color.a;return out;
}
@fragment fn fs(in:Out) {
    if textureSample(image,image_sampler,in.uv).a*in.alpha<0.5 {discard;}
}
