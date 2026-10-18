struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
};
@vertex fn vs(@location(0) position: vec2f, @location(1) uv: vec2f,
             @location(2) color: vec4f) -> VertexOutput {
    return VertexOutput(vec4f(position, 0.0, 1.0), uv, color);
}
@group(0) @binding(0) var image: texture_2d<f32>;
@group(0) @binding(1) var image_sampler: sampler;
@fragment fn fs(in: VertexOutput) -> @location(0) vec4f {
    return in.color * textureSample(image, image_sampler, in.uv);
}
