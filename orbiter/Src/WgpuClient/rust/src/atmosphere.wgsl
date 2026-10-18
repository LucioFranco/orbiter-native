// Single-scattering Rayleigh sky. Distances are planet-relative kilometres.
struct Sky { right:vec4f, up:vec4f, forward:vec4f, eye:vec4f, sun:vec4f };
@group(0) @binding(0) var<uniform> sky:Sky;
@group(0) @binding(1) var sceneDepth:texture_depth_2d;
struct Out { @builtin(position) position:vec4f, @location(0) uv:vec2f };
@vertex fn vs(@builtin(vertex_index) i:u32)->Out {
    var positions=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
    var out:Out;out.uv=positions[i];out.position=vec4f(out.uv,0,1);return out;
}
fn sphere(p:vec3f,d:vec3f,r:f32)->vec2f {
    let b=dot(p,d);let c=(length(p)-r)*(length(p)+r);
    let h=b*b-c;if h<0 {return vec2f(1,-1);}
    let root=sqrt(h);return vec2f(-b-root,-b+root);
}
@fragment fn fs(in:Out)->@location(0) vec4f {
    let ray=normalize(sky.forward.xyz+in.uv.x*sky.right.xyz+in.uv.y*sky.up.xyz);
    let radius=sky.eye.w;let eye=sky.eye.xyz;
    let shell=sphere(eye,ray,sky.sun.w);
    let start=max(shell.x,0.0);var finish=shell.y;
    let ground=sphere(eye,ray,radius);
    if ground.x>0 {finish=min(finish,ground.x);}
    let depth=textureLoad(sceneDepth,vec2i(in.position.xy),0);
    if (depth==0 && sky.right.w>0.5) || (depth>0 && sky.up.w>0.5) {return vec4f(0);}
    if depth>0 {
        // Engine projection is infinite reversed-Z with a 0.01 metre near
        // plane. Convert axial camera depth to ray distance in kilometres.
        let axial=0.00001/depth;
        let rayDistance=axial/max(dot(ray,normalize(sky.forward.xyz)),0.00001);
        finish=min(finish,rayDistance);
    }
    if finish<=start {return vec4f(0);}
    let beta=vec3f(0.0058,0.0135,0.0331);
    let step=(finish-start)/16.0;
    var optical=0.0;var scattered=vec3f(0);
    for(var i=0;i<16;i++) {
        let point=eye+ray*(start+(f32(i)+0.5)*step);
        let density=exp(-max(length(point)-radius,0.0)/8.0);
        let depth=density*step;
        let shadow=sphere(point,sky.sun.xyz,radius);
        if shadow.x<=0 {
            let lightEnd=sphere(point,sky.sun.xyz,sky.sun.w).y;
            let lightStep=max(lightEnd,0.0)/8.0;var lightDepth=0.0;
            for(var j=0;j<8;j++) {
                let sample=point+sky.sun.xyz*((f32(j)+0.5)*lightStep);
                lightDepth+=exp(-max(length(sample)-radius,0.0)/8.0)*lightStep;
            }
            scattered+=exp(-beta*(optical+depth*0.5+lightDepth))*depth;
        }
        optical+=depth;
    }
    let cosine=dot(ray,sky.sun.xyz);
    let phase=3.0/(16.0*3.14159265)*(1.0+cosine*cosine);
    let colour=vec3f(1)-exp(-scattered*beta*phase*20.0);
    let transmission=exp(-beta*optical);
    // Catalog stars use display brightness rather than physical radiance.
    // Approximate their loss of contrast under a bright sky until HDR exposure
    // is shared by the star and atmosphere passes. Preserve dark-sky stars.
    let luminance=dot(colour,vec3f(0.2126,0.7152,0.0722));
    let starContrast=select(1.0,1.0-smoothstep(0.005,0.05,luminance),depth==0);
    return vec4f(colour,1.0-dot(transmission,vec3f(1.0/3.0))*starContrast);
}
