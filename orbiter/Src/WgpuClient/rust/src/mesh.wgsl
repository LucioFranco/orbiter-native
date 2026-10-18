struct LocalLight { position: vec4f, direction: vec4f, attenuation: vec4f, diffuse: vec4f, ambient: vec4f };
struct Camera { mvp: mat4x4f, sunlight: vec4f, lights: array<LocalLight,8>, shadow_mvp:mat4x4f, shadow:vec4f, eye:vec4f };
@group(0) @binding(0) var<uniform> camera: Camera;
@group(1) @binding(0) var image: texture_2d<f32>;
@group(1) @binding(1) var image_sampler: sampler;
@group(1) @binding(2) var emission_image: texture_2d<f32>;
@group(2) @binding(0) var shadow_image:texture_depth_2d;
@group(2) @binding(1) var shadow_sampler:sampler_comparison;
fn shadow_sample(ndc:vec3f,cascade:f32,bias:f32)->f32 {
    if any(abs(ndc.xy)>vec2f(1)) || ndc.z<0 || ndc.z>1 {return 1.0;}
    let halfTexel=0.5/f32(textureDimensions(shadow_image).y);
    let uv=clamp(vec2f(ndc.x*0.5+0.5,0.5-ndc.y*0.5),vec2f(halfTexel),vec2f(1.0-halfTexel));
    return textureSampleCompareLevel(shadow_image,shadow_sampler,vec2f((uv.x+cascade)*0.5,uv.y),ndc.z+bias);
}
fn shadow_edge(ndc:vec3f)->f32 {return max(max(abs(ndc.x),abs(ndc.y)),abs(ndc.z*2.0-1.0));}
fn sunlight_visibility(position:vec3f,normal:vec3f,sun:vec3f)->f32 {
    let p=camera.shadow_mvp*vec4f(position,1);
    if camera.shadow.x==0 || p.w<=0 {return 1.0;}
    let ndc=p.xyz/p.w;
    // Allow the receiver's depth variation over the bilinear filter footprint.
    // A constant bias alone produces bands on ground viewed at grazing light.
    let cosine=clamp(abs(dot(normal,sun)),0.05,1.0);
    let slope=sqrt(1.0-cosine*cosine)/cosine;
    let bias=camera.shadow.y+2.0*slope/f32(textureDimensions(shadow_image).y);
    let near=shadow_sample(ndc,0.0,bias);
    if camera.shadow.z<=1 {return near;}
    let farNdc=vec3f(ndc.xy/camera.shadow.z,0.5+(ndc.z-0.5)/camera.shadow.z);
    let far=shadow_sample(farNdc,1.0,bias);
    let outer=mix(far,1.0,smoothstep(0.9,1.0,shadow_edge(farNdc)));
    return mix(near,outer,smoothstep(0.8,1.0,shadow_edge(ndc)));
}
struct Out { @location(6) specular:vec4f, @location(5) local_position: vec3f, @location(3) light: vec4f, @location(4) emission: vec4f, @location(2) uv: vec2f, @builtin(position) position: vec4f, @location(0) normal: vec3f, @location(1) color: vec4f };
@vertex fn vs(@location(0) position: vec3f, @location(1) normal: vec3f, @location(2) color: vec4f, @location(3) uv: vec2f, @location(4) light: vec4f, @location(5) emission: vec4f, @location(6) specular:vec4f) -> Out {
    var out: Out;
    out.position=camera.mvp*vec4f(position,1); out.local_position=position;
    out.light=light; out.emission=emission; out.specular=specular; out.uv=uv; out.normal=normal; out.color=color;
    return out;
}
fn highlight(normal:vec3f,light:vec3f,view:vec3f,power:f32)->f32 {
    if dot(normal,light)<=0.0 || dot(normal,view)<=0.0 {return 0.0;}
    return pow(max(dot(reflect(-light,normal),view),0.0),max(power,0.0001));
}
fn shade(in: Out,cutoff:f32) -> vec4f {
    let n=in.normal/max(length(in.normal),0.0001);
    let sun=camera.sunlight.xyz/max(length(camera.sunlight.xyz),0.0001);
    let ambient=clamp(camera.sunlight.w,0.0,1.0);
    let visibility=sunlight_visibility(in.local_position,n,sun);
    let light=ambient+(1.0-ambient)*max(dot(n,sun),0.0)*visibility;
    let viewDelta=camera.eye.xyz-in.local_position;
    let view=viewDelta/max(length(viewDelta),0.0001);
    // Match Orbiter D3D9's matte legacy materials: low power disables
    // specular RGB even when an old mesh leaves it nonzero.
    let reflective=camera.eye.w>0.0 && in.specular.w>=0.1 && any(in.specular.rgb>vec3f(0));
    var specular=vec3f(0);
    if reflective {specular=vec3f((1.0-ambient)*visibility*highlight(n,sun,view,in.specular.w));}
    var local=vec3f(0.0);
    for(var i=0u;i<8u;i++) {
        let source=camera.lights[i];
        let delta=source.position.xyz-in.local_position;
        let distance=length(delta);
        if source.position.w<=0.0 || distance>source.position.w {continue;}
        let toward=delta/max(distance,0.0001);
        var cone=1.0;
        if source.diffuse.w>0.5 {
            let cosine=dot(-toward,source.direction.xyz);
            let inner=source.attenuation.w; let outer=source.direction.w;
            if inner-outer<0.00001 {cone=select(0.0,1.0,cosine>=outer);}
            else {cone=clamp((cosine-outer)/(inner-outer),0.0,1.0);}
        }
        let attenuation=1.0/max(dot(source.attenuation.xyz,vec3f(1.0,distance,distance*distance)),0.0001);
        local+=cone*attenuation*(source.ambient.rgb+source.diffuse.rgb*max(dot(n,toward),0.0));
        if reflective {specular+=cone*attenuation*source.diffuse.rgb*highlight(n,toward,view,in.specular.w);}
    }
    let texel=textureSample(image,image_sampler,in.uv)*in.color;
    if texel.a<cutoff {discard;}
    return vec4f(max(texel.rgb*clamp(vec3f(light)+in.light.rgb+local,vec3f(0),vec3f(1)),textureSample(emission_image,image_sampler,in.uv).rgb*in.color.rgb*in.emission.rgb)+specular*max(in.specular.rgb,vec3f(0))*camera.eye.w,texel.a);
}
@fragment fn fs(in:Out)->@location(0) vec4f {return vec4f(shade(in,0.5).rgb,1);}
@fragment fn fs_alpha(in:Out)->@location(0) vec4f {return shade(in,0.001);}
