struct Camera {right:vec4f,up:vec4f,forward:vec4f,settings:vec4f};
@group(0) @binding(0) var<uniform> camera:Camera;
@group(0) @binding(1) var sky:texture_2d<f32>;
@group(0) @binding(2) var sky_sampler:sampler;
struct Out {@builtin(position) position:vec4f,@location(0) uv:vec2f};
@vertex fn vs(@builtin(vertex_index) i:u32)->Out {
    var points=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
    var out:Out;out.uv=points[i];out.position=vec4f(out.uv,0,1);return out;
}
struct TilePoint {atlas:vec2f,uv:vec2f,longitude:f32,band:f32,count:f32,bands:f32,south:f32};
fn tilePoint(ray:vec3f,level:u32)->TilePoint {
    var bands=1u;
    var counts=array<u32,8>(4u,0u,0u,0u,0u,0u,0u,0u);
    if(level==6u) {bands=2u;counts=array<u32,8>(8u,4u,0u,0u,0u,0u,0u,0u);}
    if(level==7u) {bands=4u;counts=array<u32,8>(16u,16u,12u,6u,0u,0u,0u,0u);}
    if(level==8u) {bands=8u;counts=array<u32,8>(32u,32u,30u,28u,24u,18u,12u,6u);}
    let south=ray.y<0.0;
    let latitude=asin(clamp(abs(ray.y),0.0,1.0))*f32(bands)/(0.5*3.14159265);
    let band=min(u32(latitude),bands-1u);
    let longitude=fract(atan2(select(ray.z,-ray.z,south),ray.x)/(2.0*3.14159265)+0.5)*f32(counts[band]);
    var index=min(u32(longitude),counts[band]-1u);
    var hemisphere=0u;
    for(var b=0u;b<bands;b++) {
        hemisphere+=counts[b];
        if(b>band) {index+=counts[b];}
    }
    if(south) {index+=hemisphere;}
    let uv=vec2f(fract(longitude),1.0-clamp(latitude-f32(band),0.0,1.0));
    return TilePoint(vec2f(f32(index%32u),f32(index/32u))*256.0,uv,floor(longitude),f32(band),f32(counts[band]),f32(bands),select(1.0,-1.0,south));
}
fn tileSample(point:TilePoint)->vec3f {
    let pixel=point.atlas+clamp(point.uv*256.0,vec2f(0.5),vec2f(255.5));
    return textureSampleLevel(sky,sky_sampler,pixel/vec2f(textureDimensions(sky)),0.0).rgb;
}
// A footprint can cross an atlas edge into a different longitude count or
// hemisphere. Reproject its taps on the sphere instead of sampling adjacent
// atlas storage (which contains unrelated patches).
fn footprintTap(point:TilePoint,pixel:vec2f,level:u32)->vec3f {
    let uv=pixel/256.0;
    let longitude=(point.longitude+uv.x)/point.count*(2.0*3.14159265)-3.14159265;
    let latitude=(point.band+1.0-uv.y)/point.bands*(0.5*3.14159265);
    let ray=vec3f(cos(latitude)*cos(longitude),sin(latitude)*point.south,cos(latitude)*sin(longitude)*point.south);
    return tileSample(tilePoint(ray,level));
}
@fragment fn fs(in:Out)->@location(0) vec4f {
    let ray=normalize(camera.forward.xyz+camera.right.xyz*in.uv.x+camera.up.xyz*in.uv.y);
    let level=u32(camera.settings.y);
    var colour:vec3f;
    if(level>=5u) {
        let point=tilePoint(ray,level);
        let position=point.uv*256.0-0.5;
        let base=floor(position)+0.5;
        let weight=fract(position);
        // Interior footprints use native bilinear filtering. Only boundary
        // pixels need the four sphere-reprojected taps.
        if(all(base>=vec2f(0.5)) && all(base+1.0<=vec2f(255.5))) {
            colour=tileSample(point);
        } else {
            colour=mix(mix(footprintTap(point,base,level),footprintTap(point,base+vec2f(1,0),level),weight.x),
                       mix(footprintTap(point,base+vec2f(0,1),level),footprintTap(point,base+vec2f(1,1),level),weight.x),weight.y);
        }
    } else {
        let uv=vec2f(atan2(ray.z,ray.x)/(2.0*3.14159265)+0.5,acos(clamp(ray.y,-1.0,1.0))/3.14159265);
        let halfTexel=vec2f(0.5)/vec2f(textureDimensions(sky));
        let sampleUV=vec2f(halfTexel.x+uv.x*(1.0-2.0*halfTexel.x),clamp(uv.y,halfTexel.y,1.0-halfTexel.y));
        colour=textureSampleLevel(sky,sky_sampler,sampleUV,0.0).rgb;
    }
    return vec4f(colour*camera.settings.x,1);
}
