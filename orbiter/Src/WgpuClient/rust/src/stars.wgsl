struct Camera { projection:mat4x4f, viewport:vec4f };
@group(0) @binding(0) var<uniform> camera:Camera;
struct Out { @builtin(position) position:vec4f, @location(0) disk:vec2f, @location(1) color:vec3f };
@vertex fn vs(@builtin(vertex_index) vertex:u32,@location(0) direction:vec3f,@location(1) color:vec3f,@location(2) radius:f32)->Out {
    let corners=array<vec2f,6>(vec2f(-1,-1),vec2f(1,-1),vec2f(-1,1),vec2f(-1,1),vec2f(1,-1),vec2f(1,1));
    let corner=corners[vertex];
    // A direction has no translation; depth zero is the reversed-Z sky limit.
    let center=camera.projection*vec4f(direction,0);
    var out:Out;
    out.position=vec4f(center.xy+corner*radius*camera.viewport.xy*center.w,0,center.w);
    out.disk=corner;out.color=color;return out;
}
@fragment fn fs(in:Out)->@location(0) vec4f {
    let alpha=clamp(1.0-length(in.disk),0.0,1.0);
    return vec4f(in.color,alpha);
}
