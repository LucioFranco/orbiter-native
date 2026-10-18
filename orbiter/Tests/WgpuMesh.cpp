#include "WgpuBridge.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
static void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(std::string(message)+": "+orb_gpu_error());}
int main() {
    try {
        std::unique_ptr<OrbGpu,decltype(&orb_gpu_destroy)> gpu(orb_gpu_create(),orb_gpu_destroy);
        require(bool(gpu),"GPU device");
        const auto target=orb_gpu_surface_create(gpu.get(),32,32);
        require(target!=0,"mesh target");
        OrbMeshVertex vertices[6]{};
        const float xy[3][2]={{-1,-1},{1,-1},{0,1}};
        for(int i=0;i<6;++i) {
            vertices[i].position[0]=xy[i%3][0];vertices[i].position[1]=xy[i%3][1];
            vertices[i].position[2]=i<3?0.8f:0.2f; // reversed Z: blue near triangle first, red far triangle last
            vertices[i].normal[2]=-1;vertices[i].color[i<3?2:0]=1;vertices[i].color[3]=1;
        }
        const uint32_t indices[]={0,1,2,3,4,5};
        const auto mesh=orb_gpu_mesh_create(gpu.get(),vertices,6,indices,6);
        require(mesh!=0,"mesh upload");
        const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"mesh render");
        std::array<uint8_t,32*32*4> pixels{};
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"mesh readback");
        const auto pixel=&pixels[(16*32+16)*4];
        require(pixel[0]==0 && pixel[1]==0 && pixel[2]>128 && pixel[3]==255,"near triangle wins despite far triangle submitted last");
        // A reused depth attachment must clear between frames, and each target
        // must own its dimensions when switching between differently sized views.
        const auto depthFarMesh=orb_gpu_mesh_create(gpu.get(),vertices+3,3,indices,3);
        const auto smallTarget=orb_gpu_surface_create(gpu.get(),17,9);
        require(depthFarMesh && smallTarget,"alternate depth target fixture");
        std::array<uint8_t,17*9*4> smallPixels{};
        for(unsigned frame=0;frame<3;++frame) {
            for(auto destination:{target,smallTarget}) {
                require(orb_gpu_mesh_render(gpu.get(),destination,mesh,identity),"near depth frame");
                require(orb_gpu_mesh_render(gpu.get(),destination,depthFarMesh,identity),"far depth frame");
                auto data=destination==target?pixels.data():smallPixels.data();
                const auto width=destination==target?32u:17u,height=destination==target?32u:9u;
                require(orb_gpu_surface_read(gpu.get(),destination,data,width*height*4,width*4),"reused depth readback");
                const auto center=data+((height/2)*width+width/2)*4;
                require(center[0]>128 && center[1]==0 && center[2]==0,"previous frame depth does not hide farther geometry");
            }
        }
        require(orb_gpu_mesh_release(gpu.get(),depthFarMesh),"far fixture release");
        require(orb_gpu_surface_release(gpu.get(),smallTarget),"alternate depth target release");
        OrbMeshVertex reflective[3]{};
        for(unsigned i=0;i<3;++i) {
            std::copy(vertices[i].position,vertices[i].position+3,reflective[i].position);
            reflective[i].normal[2]=-1;reflective[i].color[3]=1;
            reflective[i].specular[0]=reflective[i].specular[1]=reflective[i].specular[2]=.5f;
            reflective[i].specular[3]=32;
        }
        const auto reflectiveMesh=orb_gpu_mesh_create(gpu.get(),reflective,3,indices,3);
        require(reflectiveMesh,"reflective material fixture");
        OrbMeshDraw reflection{};reflection.mesh=reflectiveMesh;
        std::copy(identity,identity+16,reflection.mvp);reflection.sunlight[2]=-1;
        reflection.eye[2]=-10;reflection.eye[3]=1;
        auto reflectionPixel=[&]() {
            require(orb_gpu_mesh_render_scene(gpu.get(),target,&reflection,1),"render material highlight");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"read material highlight");
        };
        reflectionPixel();require(pixel[0]>=120 && pixel[0]<=130,"aligned Sun produces specified specular intensity on black diffuse material");
        reflection.eye[3]=0;reflectionPixel();require(pixel[0]==0,"specular setting suppresses highlight");
        reflection.eye[3]=1;reflection.eye[0]=20;reflectionPixel();require(pixel[0]<2,"highlight follows viewing direction");
        reflection.eye[0]=0;reflection.sunlight[2]=1;reflectionPixel();require(pixel[0]==0,"back-facing Sun produces no highlight");
        reflection.sunlight[2]=-1;reflection.eye[0]=5;
        for(auto &vertex:reflective)vertex.specular[3]=1;
        require(orb_gpu_mesh_update(gpu.get(),reflectiveMesh,reflective,3),"wide material lobe");
        reflectionPixel();const auto wideHighlight=pixel[0];
        for(auto &vertex:reflective)vertex.specular[3]=64;
        require(orb_gpu_mesh_update(gpu.get(),reflectiveMesh,reflective,3),"narrow material lobe");
        reflectionPixel();require(wideHighlight>100 && pixel[0]<5,"material power controls highlight width");
        reflection.eye[0]=0;reflection.sunlight[2]=0;reflectionPixel();require(pixel[0]==0,"eclipsed Sun produces no highlight");
        auto &specularLight=reflection.lights[0];specularLight.position[2]=-10;specularLight.position[3]=20;
        specularLight.attenuation[0]=1;specularLight.diffuse[1]=1;
        reflectionPixel();require(pixel[0]==0 && pixel[1]>=120 && pixel[1]<=130 && pixel[2]==0,"local light colour contributes a material highlight");
        // Orbiter's D3D9 material import treats powers below 0.1 as matte,
        // even when old meshes retain nonzero specular RGB. NASSP's black
        // X-pointer needles use exactly this combination (white RGB, power 0).
        for(float power:{0.0f,0.05f}) {
            for(auto &vertex:reflective)vertex.specular[3]=power;
            require(orb_gpu_mesh_update(gpu.get(),reflectiveMesh,reflective,3),"matte legacy material upload");
            reflection.sunlight[2]=-1;
            reflectionPixel();
            require(pixel[0]==0 && pixel[1]==0 && pixel[2]==0,"zero/near-zero power legacy material stays black under Sun and local light");
        }
        require(orb_gpu_mesh_release(gpu.get(),reflectiveMesh),"release reflective fixture");
        OrbParticle particle{{0,0,0.5f,1},{0.5f,0.5f},{1,0,0,0.5f}};
        require(orb_gpu_particles_upload(gpu.get(),&particle,1),"upload particle batch");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"occluded particle render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"occluded particle readback");
        require(pixel[0]==0 && pixel[2]>128,"opaque geometry occludes particle behind it");
        particle.center[2]=0.9f;
        require(orb_gpu_particles_upload(gpu.get(),&particle,1),"upload foreground particle");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"foreground particle render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"blended particle readback");
        require(pixel[0]>110 && pixel[0]<135 && pixel[2]>80,"foreground particle alpha blends with opaque scene");
        const auto sprite=orb_gpu_surface_create(gpu.get(),1,1);
        const uint8_t greenSprite[4]={0,255,0,255};
        require(orb_gpu_surface_upload(gpu.get(),sprite,greenSprite,4,4),"particle sprite upload");
        particle.texture=sprite;particle.color[0]=particle.color[1]=particle.color[2]=1;
        require(orb_gpu_particles_upload(gpu.get(),&particle,1),"textured particle batch");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"textured particle render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"textured particle readback");
        require(pixel[0]==0 && pixel[1]>110 && pixel[1]<135 && pixel[2]>80,"particle samples custom sprite color");
        const uint8_t transparentSprite[4]={255,0,0,0};
        require(orb_gpu_surface_upload(gpu.get(),sprite,transparentSprite,4,4),"transparent sprite upload");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"transparent particle render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"transparent particle readback");
        require(pixel[0]==0 && pixel[1]==0 && pixel[2]>128,"particle respects sprite alpha");
        const auto atlas=orb_gpu_surface_create(gpu.get(),8,8);
        std::array<uint8_t,8*8*4> atlasPixels{};
        const uint8_t colors[4][3]={{255,0,0},{0,255,0},{0,0,255},{255,255,255}};
        for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
            auto out=&atlasPixels[(y*8+x)*4];const auto color=colors[(y/4)*2+x/4];
            std::copy(color,color+3,out);out[3]=255;
        }
        require(orb_gpu_surface_upload(gpu.get(),atlas,atlasPixels.data(),atlasPixels.size(),32),"particle atlas upload");
        particle.texture=atlas;
        for(uint32_t variant=1;variant<=8;++variant) {
            particle.sprite=variant;
            require(orb_gpu_particles_upload(gpu.get(),&particle,1),"atlas particle batch");
            require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"atlas particle render");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"atlas readback");
            const auto expected=colors[(variant-1)%4];
            require((expected[0]?pixel[0]>110:pixel[0]==0) && (expected[1]?pixel[1]>110:pixel[1]==0),
                    "particle selects intended atlas quadrant for both orientations");
        }
        require(orb_gpu_particles_upload(gpu.get(),nullptr,0),"clear particle batch");
        require(orb_gpu_surface_release(gpu.get(),atlas),"release particle atlas");
        require(orb_gpu_surface_release(gpu.get(),sprite),"release particle sprite");
        const auto nearMesh=orb_gpu_mesh_create(gpu.get(),vertices,3,indices,3);
        const auto farMesh=orb_gpu_mesh_create(gpu.get(),vertices+3,3,indices,3);
        OrbMeshDraw scene[2]{};scene[0].mesh=nearMesh;scene[1].mesh=farMesh;
        for(auto &draw:scene) {std::copy(identity,identity+16,draw.mvp);draw.sunlight[2]=-1;}
        require(orb_gpu_mesh_render_scene(gpu.get(),target,scene,2),"batched scene render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"scene readback");
        require(pixel[0]==0 && pixel[2]>128,"separate meshes share reversed-Z depth");
        for(auto &draw:scene) {draw.sunlight[2]=0;draw.sunlight[3]=0.03f;}
        require(orb_gpu_mesh_render_scene(gpu.get(),target,scene,2),"eclipsed scene render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"eclipse readback");
        require(pixel[2]>=7 && pixel[2]<=8,"blocked sunlight leaves only configured ambient");
        for(auto &draw:scene)draw.sunlight[3]=0;
        for(auto &draw:scene)draw.sunlight[2]=1;
        require(orb_gpu_mesh_render_scene(gpu.get(),target,scene,2),"scene lit from opposite direction");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"night-side readback");
        require(pixel[0]==0 && pixel[1]==0 && pixel[2]==0,"unlit normals face away from Sun");
        auto &lamp=scene[0].lights[0];
        lamp.position[2]=-0.2f;lamp.position[3]=4;
        lamp.attenuation[0]=1;lamp.diffuse[2]=0.8f;
        auto blue=[&]() {
            require(orb_gpu_mesh_render_scene(gpu.get(),target,scene,2),"local light render");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"local light readback");
            return int(pixel[2]);
        };
        const int full=blue();require(full>190 && full<210,"point light illuminates front-facing surface");
        lamp.diffuse[2]=0.4f;require(std::abs(blue()*2-full)<=2,"local light intensity scales radiance");
        lamp.diffuse[2]=0.8f;lamp.attenuation[0]=0;lamp.attenuation[2]=1;
        lamp.position[2]=-1.2f;
        require(blue()>45 && blue()<55,"quadratic attenuation uses vessel-local distance");
        lamp.position[3]=1;require(blue()==0,"local light range cutoff");
        lamp.position[3]=4;lamp.position[2]=1.2f;require(blue()==0,"local diffuse light respects normal direction");
        lamp.position[2]=-0.2f;lamp.attenuation[0]=1;lamp.attenuation[2]=0;
        lamp.diffuse[3]=1;lamp.direction[2]=1;lamp.direction[3]=0.8f;lamp.attenuation[3]=0.95f;
        require(blue()>190,"spotlight inner cone");
        lamp.direction[2]=-1;require(blue()==0,"spotlight excludes rear hemisphere");
        lamp.direction[2]=0.9f;lamp.direction[0]=std::sqrt(1.0f-0.9f*0.9f);
        const int edge=blue();require(edge>50 && edge<180,"spotlight penumbra attenuates smoothly");
        lamp.direction[0]=0;lamp.direction[2]=1;lamp.attenuation[3]=lamp.direction[3];
        require(blue()>190,"equal cone angles produce finite hard edge");
        lamp={};lamp.position[3]=4;lamp.attenuation[0]=1;lamp.ambient[2]=0.2f;
        require(blue()>=50 && blue()<=52,"local ambient contribution is independent of normals");
        lamp={};require(blue()==0,"removed lights do not remain in GPU state");
        require(orb_gpu_mesh_release(gpu.get(),nearMesh) && orb_gpu_mesh_release(gpu.get(),farMesh),"scene mesh release");
        // Both triangles use white material; distinct texture bindings prove group selection.
        for(auto &vertex:vertices) vertex.color[0]=vertex.color[1]=vertex.color[2]=1;
        const auto textured=orb_gpu_mesh_create(gpu.get(),vertices,6,indices,6);
        const auto green=orb_gpu_surface_create(gpu.get(),1,1);
        const auto red=orb_gpu_surface_create(gpu.get(),1,1);
        const uint8_t greenPixel[4]={0,255,0,255},redPixel[4]={255,0,0,255};
        require(orb_gpu_surface_upload(gpu.get(),green,greenPixel,4,4),"green texture");
        require(orb_gpu_surface_upload(gpu.get(),red,redPixel,4,4),"red texture");
        const OrbMeshGroup groups[]={{0,3,green},{3,3,red}};
        require(orb_gpu_mesh_groups(gpu.get(),textured,groups,2),"group texture binding");
        require(orb_gpu_surface_release(gpu.get(),red),"mesh retains texture allocation");
        require(orb_gpu_mesh_render(gpu.get(),target,textured,identity),"textured groups");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"textured readback");
        require(pixel[0]==0 && pixel[1]>128 && pixel[2]==0,"near group uses green texture");
        require(orb_gpu_surface_upload(gpu.get(),green,redPixel,4,4),"dynamic texture update");
        require(orb_gpu_mesh_render(gpu.get(),target,textured,identity),"updated texture render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"updated readback");
        require(pixel[0]>128 && pixel[1]==0 && pixel[2]==0,"mesh sees dynamic surface update");
        const OrbMeshGroup badGroup{3,6,0};
        require(!orb_gpu_mesh_groups(gpu.get(),textured,&badGroup,1),"invalid group range rejected");
        require(orb_gpu_mesh_release(gpu.get(),textured),"textured mesh release");
        // Back-facing normals leave only the diagnostic ambient term (0.35).
        for(auto &vertex:vertices) {
            vertex.normal[2]=1;
            for(int k=0;k<3;++k)vertex.color[k]=0.5f;
        }
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,6),"baseline lighting vertices");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"baseline lighting");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"baseline lighting pixels");
        const auto baseline=pixel[0];require(baseline>=43 && baseline<=46,"baseline ambient result");
        for(auto &vertex:vertices)vertex.light[0]=0.4f;
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,6),"red added light");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"added light render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"added light pixels");
        require(pixel[0]>=94 && pixel[0]<=97 && pixel[1]==baseline,"added light preserves other channels");
        for(auto &vertex:vertices)vertex.emission[1]=1;
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,6),"green emission");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"emission render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"emission pixels");
        require(pixel[1]>=127 && pixel[1]<=129 && pixel[2]==baseline,"emission sets textured brightness floor");
        for(auto &vertex:vertices)vertex.light[0]=4.0f;
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,6),"overlapping bright lights");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"bounded diffuse illumination");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"bright panel pixels");
        require(pixel[0]>=127 && pixel[0]<=129,"bright light preserves diffuse texture contrast instead of clipping white");
        for(auto &vertex:vertices)vertex.light[0]=0.4f;
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,6),"restore diffuse illumination");
        const auto emissionMap=orb_gpu_surface_create(gpu.get(),1,1);
        const uint8_t blackEmission[]={0,0,0,255},greenEmission[]={0,255,0,255};
        require(orb_gpu_surface_upload(gpu.get(),emissionMap,blackEmission,4,4),"black emission map");
        const OrbMeshGroup masked{0,6,0,emissionMap};
        require(orb_gpu_mesh_groups(gpu.get(),mesh,&masked,1),"separate emission atlas binding");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"masked integral lighting render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"masked emission pixels");
        require(pixel[1]==baseline,"black emission mask leaves panel background unlit");
        require(orb_gpu_surface_upload(gpu.get(),emissionMap,greenEmission,4,4),"illuminated emission texel");
        require(orb_gpu_surface_release(gpu.get(),emissionMap),"group retains emission texture allocation");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"illuminated label render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"illuminated label pixels");
        require(pixel[1]>=127 && pixel[1]<=129 && pixel[2]==baseline,"emission atlas lights only its intended channels");
        const OrbMeshGroup unmasked{0,6,0,0};
        require(orb_gpu_mesh_groups(gpu.get(),mesh,&unmasked,1),"restore untextured mesh binding");
        for(auto &vertex:vertices)vertex.position[0]+=3;
        require(orb_gpu_mesh_update(gpu.get(),mesh,vertices,6),"batched animated vertices");
        require(!orb_gpu_mesh_update(gpu.get(),mesh,vertices,5),"wrong vertex count rejected");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"moved mesh render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"moved mesh readback");
        require(pixel[0]==0 && pixel[1]==0 && pixel[2]==0,"updated vertices move mesh offscreen");
        // Partial updates must leave vertices outside their ranges untouched.
        for(auto &vertex:vertices)vertex.position[0]-=3;
        const OrbMeshRange nearRange{0,3};
        require(orb_gpu_mesh_update_ranges(gpu.get(),mesh,vertices,6,&nearRange,1),"partial mesh upload");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"partial mesh render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"partial mesh readback");
        require(pixel[1]>=127,"partial upload restores near triangle");
        const OrbMeshRange invalidRanges[]={{0,3},{5,2}};
        for(auto &vertex:vertices)vertex.position[0]+=3;
        require(!orb_gpu_mesh_update_ranges(gpu.get(),mesh,vertices,6,invalidRanges,2),"invalid batch rejected before any write");
        require(orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"rejected batch render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"rejected batch readback");
        require(pixel[1]>=127,"rejected batch preserves previous mesh");
        const uint32_t invalid[]={0,1,6};
        require(!orb_gpu_mesh_create(gpu.get(),vertices,6,invalid,3),"invalid mesh indices rejected");
        OrbMeshDraw skyDraw{};skyDraw.mesh=mesh;std::copy(identity,identity+16,skyDraw.mvp);skyDraw.mvp[12]=10;
        float sky[20]={1,0,0,0, 0,0,1,0, 0,1,0,0, 0,6371.001f,0,6371, 0,1,0,6471};
        auto skyPixel=[&]() {
            require(orb_gpu_atmosphere(gpu.get(),sky),"configure atmosphere");
            require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"atmosphere render");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"atmosphere readback");
            return std::array<int,3>{pixel[0],pixel[1],pixel[2]};
        };
        const auto day=skyPixel();require(day[2]>day[0]+10 && day[2]>20,"daytime Rayleigh sky is blue");
        sky[3]=1;const auto noSky=skyPixel();require(noSky==std::array<int,3>{0,0,0},"sky haze can be disabled independently");
        sky[3]=0;sky[7]=1;require(skyPixel()==day,"disabling surface fog preserves sky");sky[7]=0;
        sky[17]=-1;const auto night=skyPixel();require(night[0]<3 && night[1]<3 && night[2]<3,"planet blocks direct scattering on night side");
        const OrbStar skyStar{{0,0,1},{1,1,1},4};
        require(orb_gpu_stars_upload(gpu.get(),&skyStar,1),"sky contrast star");
        float starProjection[16]{};starProjection[0]=starProjection[5]=starProjection[11]=1;
        require(orb_gpu_stars_camera(gpu.get(),starProjection),"sky contrast camera");
        const auto nightStar=skyPixel();require(nightStar[0]>30,"stars remain visible at night");
        sky[17]=1;const auto dayStar=skyPixel();
        require(dayStar==day,"daylight hides catalog stars");
        require(orb_gpu_stars_upload(gpu.get(),nullptr,0),"clear sky contrast star");
        sky[17]=1;sky[13]=6571;sky[9]=-0.245f;sky[10]=0.969523f;
        const auto limb=skyPixel();require(limb[2]>5,"atmosphere visible along orbital limb ray");
        skyDraw.mvp[12]=0;const auto foreground=skyPixel();
        require(orb_gpu_atmosphere(gpu.get(),nullptr),"disable atmosphere for airless world");
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"foreground without atmosphere");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"foreground readback");
        require(foreground==std::array<int,3>{pixel[0],pixel[1],pixel[2]},"opaque spacecraft occludes atmospheric background");
        // Hold screen coverage and material fixed while changing reversed-Z
        // distance: only the air in front of the surface should contribute.
        sky[13]=6371.001f;sky[9]=1;sky[10]=0;
        skyDraw.mvp[10]=0;skyDraw.mvp[14]=0.01f/100.0f;
        const auto nearHaze=skyPixel();
        skyDraw.mvp[14]=0.01f/100000.0f;
        const auto farHaze=skyPixel();
        require(farHaze[2]>nearHaze[2]+10,"distant opaque surfaces receive blue atmospheric scattering");
        sky[3]=1;require(skyPixel()==farHaze,"disabling sky preserves haze over distant surfaces");
        sky[3]=0;sky[7]=1;const auto noFog=skyPixel();

        require(orb_gpu_atmosphere(gpu.get(),nullptr),"disable aerial perspective");
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"surface without atmosphere");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"fog-disabled comparison");
        require(noFog==std::array<int,3>{pixel[0],pixel[1],pixel[2]},"disabled surface fog preserves unmodified opaque surface");

        skyDraw.mvp[12]=10;
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"airless render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"airless readback");
        require(pixel[0]==0 && pixel[1]==0 && pixel[2]==0,"disabled sky leaves black space");
        const auto backgroundTexture=orb_gpu_surface_create(gpu.get(),1,1);
        const uint8_t backgroundPixel[]={80,120,160,255};
        require(orb_gpu_surface_upload(gpu.get(),backgroundTexture,backgroundPixel,4,4),"celestial image upload");
        float backgroundCamera[16]={1,0,0,0,0,1,0,0,0,0,1,0,0.5f,0,0,0};
        require(orb_gpu_background(gpu.get(),backgroundTexture,backgroundCamera),"enable celestial image");
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"celestial background render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"celestial background readback");
        require(pixel[0]==40 && pixel[1]==60 && pixel[2]==80,"celestial background respects configured brightness");
        require(orb_gpu_star_image(gpu.get(),backgroundTexture,backgroundCamera),"enable independent star-image layer");
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"additive star-image render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"star-image readback");
        require(pixel[0]==80 && pixel[1]==120 && pixel[2]==160,"star image adds to the Milky Way layer");
        require(orb_gpu_background(gpu.get(),0,nullptr),"disable only Milky Way");
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"independent star-image render");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"independent star-image readback");
        require(pixel[0]==40 && pixel[1]==60 && pixel[2]==80,"star image remains enabled without background image");
        require(orb_gpu_background(gpu.get(),backgroundTexture,backgroundCamera),"restore Milky Way layer");
        skyDraw.mvp[12]=0;
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"foreground over celestial image");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"foreground background readback");
        const auto withBackground=pixels;
        require(orb_gpu_background(gpu.get(),0,nullptr),"disable celestial image");
        require(orb_gpu_star_image(gpu.get(),0,nullptr),"disable star image");
        require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"foreground without celestial image");
        require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"foreground comparison readback");
        require(std::equal(pixel,pixel+4,withBackground.data()+(16*32+16)*4),"opaque geometry occludes celestial background");
        require(orb_gpu_surface_release(gpu.get(),backgroundTexture),"celestial image release");
        const auto tiledSky=orb_gpu_surface_create(gpu.get(),8192,256);
        std::vector<uint8_t> skyAtlas(8192*256*4,255);
        for(unsigned y=0;y<256;++y)for(unsigned x=0;x<8192;++x) {
            const auto i=(y*8192+x)*4;skyAtlas[i]=x%256;skyAtlas[i+1]=y;skyAtlas[i+2]=(x/256)*30;
        }
        require(orb_gpu_surface_upload(gpu.get(),tiledSky,skyAtlas.data(),skyAtlas.size(),8192*4),"tiled sky upload");
        skyDraw.mvp[12]=10;
        float tiledCamera[16]{};tiledCamera[12]=1;tiledCamera[13]=5;
        for(unsigned south=0;south<2;++south)for(unsigned quadrant=0;quadrant<4;++quadrant) {
            const double longitude=-3.141592653589793+(quadrant+0.5)*3.141592653589793/2;
            tiledCamera[8]=float(std::cos(longitude));tiledCamera[9]=south?-1:1;
            tiledCamera[10]=float(std::sin(longitude))*(south?-1:1);
            require(orb_gpu_background(gpu.get(),tiledSky,tiledCamera),"tiled sky camera");
            require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"tiled sky render");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"tiled sky readback");
            require(std::abs(int(pixel[0])-128)<=1 && std::abs(int(pixel[1])-128)<=1 && pixel[2]==(south*4+quadrant)*30,"sky longitude, latitude and southern hemisphere orientation");
        }
        auto seamPixel=[&](uint64_t texture,float x,float y,float z) {
            tiledCamera[8]=x;tiledCamera[9]=y;tiledCamera[10]=z;
            require(orb_gpu_background(gpu.get(),texture,tiledCamera),"sky seam camera");
            require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"sky seam render");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"sky seam readback");
        };
        seamPixel(tiledSky,0,1,-1);
        require(std::abs(int(pixel[2])-15)<=1,"longitude boundary blends neighbouring sky patches");
        seamPixel(tiledSky,-1,0,-1);
        require(std::abs(int(pixel[2])-105)<=1,"equator blends geographically adjacent rotated hemispheres");
        // Level 8 has unequal longitude counts per latitude band, stored
        // pole-to-equator separately for each hemisphere.
        const auto fullSky=orb_gpu_surface_create(gpu.get(),8192,3072);
        skyAtlas.resize(8192*3072*4);
        for(unsigned y=0;y<3072;++y)for(unsigned x=0;x<8192;++x) {
            const unsigned tile=(y/256)*32+x/256;const auto i=(y*8192+x)*4;
            skyAtlas[i]=tile%256;skyAtlas[i+1]=tile/256*100;skyAtlas[i+2]=0;skyAtlas[i+3]=255;
        }
        require(orb_gpu_surface_upload(gpu.get(),fullSky,skyAtlas.data(),skyAtlas.size(),8192*4),"full sky upload");
        const unsigned counts[]={32,32,30,28,24,18,12,6};tiledCamera[13]=8;
        for(unsigned south=0;south<2;++south)for(unsigned band=0;band<8;++band) {
            const double latitude=(band+0.5)*3.141592653589793/16;
            const double longitude=-3.141592653589793+0.5*2*3.141592653589793/counts[band];
            tiledCamera[8]=float(std::cos(latitude)*std::cos(longitude));
            tiledCamera[9]=float(std::sin(latitude))*(south?-1:1);
            tiledCamera[10]=float(std::cos(latitude)*std::sin(longitude))*(south?-1:1);
            unsigned expected=south*182;for(unsigned b=band+1;b<8;++b)expected+=counts[b];
            require(orb_gpu_background(gpu.get(),fullSky,tiledCamera),"full sky camera");
            require(orb_gpu_mesh_render_scene(gpu.get(),target,&skyDraw,1),"full sky render");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"full sky readback");
            require(pixel[0]==expected%256 && pixel[1]==expected/256*100,"all level-eight latitude bands resolve their hemisphere's tile");
        }
        const double boundaryLatitude=3*3.141592653589793/16, boundaryLongitude=-3.141592653589793+0.01;
        seamPixel(fullSky,float(std::cos(boundaryLatitude)*std::cos(boundaryLongitude)),float(std::sin(boundaryLatitude)),float(std::cos(boundaryLatitude)*std::sin(boundaryLongitude)));
        require(std::abs(int(pixel[0])-74)<=1,"latitude boundary blends bands with different longitude counts");
        require(orb_gpu_background(gpu.get(),0,nullptr),"disable tiled sky");
        require(orb_gpu_surface_release(gpu.get(),fullSky),"full sky release");
        require(orb_gpu_surface_release(gpu.get(),tiledSky),"tiled sky release");
        // Opposite image edges must not bleed into independent terrain tiles.
        const auto edgeTexture=orb_gpu_surface_create(gpu.get(),2,1);
        const uint8_t edgePixels[]={255,0,0,255,0,0,255,255};
        require(orb_gpu_surface_upload(gpu.get(),edgeTexture,edgePixels,8,8),"edge texture upload");
        OrbMeshVertex edgeVertices[3]{};
        for(int i=0;i<3;++i) {
            edgeVertices[i].position[0]=xy[i][0];edgeVertices[i].position[1]=xy[i][1];edgeVertices[i].position[2]=0.5f;
            edgeVertices[i].normal[2]=-1;
            std::fill(edgeVertices[i].color,edgeVertices[i].color+4,1);
        }
        const auto edgeMesh=orb_gpu_mesh_create(gpu.get(),edgeVertices,3,indices,3);
        OrbMeshGroup edgeGroup{0,3,edgeTexture,0,1};
        auto edgePixel=[&]() {
            require(orb_gpu_mesh_groups(gpu.get(),edgeMesh,&edgeGroup,1),"edge sampler binding");
            require(orb_gpu_mesh_render(gpu.get(),target,edgeMesh,identity),"edge sampler render");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"edge sampler readback");
        };
        edgePixel();require(pixel[0]>128 && pixel[2]==0,"terrain edge clamps to its own red texel");
        edgeGroup.clamp_texture=0;edgePixel();
        require(pixel[0]>60 && pixel[2]>60,"spacecraft UV repeat still blends opposite edges");
        const OrbMeshGroup whiteGroup{0,3,0,0,0};
        require(orb_gpu_mesh_groups(gpu.get(),edgeMesh,&whiteGroup,1),"white shadow receiver");
        const auto casterMesh=orb_gpu_mesh_create(gpu.get(),edgeVertices,3,indices,3);
        OrbMeshDraw shadowDraws[2]{};
        for(int i=0;i<2;++i) {
            auto &draw=shadowDraws[i];draw.mesh=i?casterMesh:edgeMesh;
            std::copy(identity,identity+16,draw.mvp);std::copy(identity,identity+16,draw.shadow_mvp);
            draw.sunlight[2]=-1;draw.sunlight[3]=0.2f;draw.shadow[0]=1;draw.shadow[1]=0.001f;
            draw.shadow_mvp[10]=0;draw.shadow_mvp[14]=i?0.8f:0.2f;
        }
        shadowDraws[1].mvp[12]=10; // invisible to camera, still visible to Sun
        auto shadowPixel=[&](size_t count) {
            require(orb_gpu_mesh_render_scene(gpu.get(),target,shadowDraws,count),"shadow scene");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"shadow readback");
        };
        shadowPixel(1);require(pixel[0]>250,"unoccluded sunlight remains bright without self-shadow acne");
        shadowPixel(2);require(pixel[0]>=50 && pixel[0]<=52,"off-camera caster blocks sunlight but preserves ambient");
        const OrbMeshGroup hullGroup{0,3,0,0,0,0,1};
        require(orb_gpu_mesh_groups(gpu.get(),casterMesh,&hullGroup,1),"cockpit exterior hull caster");
        shadowDraws[1].mvp[12]=0;shadowDraws[1].mvp[14]=0.2f;
        shadowPixel(2);require(pixel[0]>=50 && pixel[0]<=52,"in-front hull casts without covering the cockpit colour image");
        shadowDraws[1].shadow[0]=0;shadowPixel(2);
        require(pixel[0]>250,"disabling hidden hull casting restores sunlight");
        shadowDraws[1].shadow[0]=1;shadowDraws[1].mvp[12]=10;shadowDraws[1].mvp[14]=0;
        require(orb_gpu_mesh_groups(gpu.get(),casterMesh,&whiteGroup,1),"restore visible caster material");
        for(auto &draw:shadowDraws)draw.shadow[2]=4;
        for(float offset:{0.9f,1.0f,1.1f,2.0f}) {
            for(auto &draw:shadowDraws)draw.shadow_mvp[12]=offset;
            shadowPixel(2);require(pixel[0]>=50 && pixel[0]<=52,"outer shadow volume preserves occlusion across the inner boundary");
        }
        for(auto &draw:shadowDraws)draw.shadow_mvp[12]=5;
        shadowPixel(2);require(pixel[0]>250,"outside both shadow volumes stays lit");
        for(auto &draw:shadowDraws)draw.shadow_mvp[12]=0;
        const auto cutout=orb_gpu_surface_create(gpu.get(),1,1);
        const uint8_t cutoutPixel[]={255,255,255,0};
        require(orb_gpu_surface_upload(gpu.get(),cutout,cutoutPixel,4,4),"shadow cutout texture");
        const OrbMeshGroup cutoutGroup{0,3,cutout,0,0};
        require(orb_gpu_mesh_groups(gpu.get(),casterMesh,&cutoutGroup,1),"shadow cutout binding");
        shadowPixel(2);require(pixel[0]>250,"transparent mesh cutouts do not cast solid shadows");
        shadowDraws[0].shadow_mvp[2]=0.3f;
        shadowDraws[0].sunlight[0]=0.8f;shadowDraws[0].sunlight[2]=-0.6f;
        shadowPixel(1);const auto slopedShadow=pixels;
        shadowDraws[0].shadow[0]=0;shadowPixel(1);
        for(size_t i=0;i<pixels.size();++i)
            require(std::abs(int(pixels[i])-int(slopedShadow[i]))<=1,"sloped receiver does not shadow itself across the filter footprint");
        const uint8_t translucent[]={255,0,0,128};
        require(orb_gpu_surface_upload(gpu.get(),cutout,translucent,4,4),"translucent cloud texture");
        const OrbMeshGroup cloudGroup{0,3,cutout,0,1,1};
        require(orb_gpu_mesh_groups(gpu.get(),casterMesh,&cloudGroup,1),"transparent cloud group");
        for(auto &draw:shadowDraws) {draw.shadow[0]=0;draw.sunlight[0]=0;draw.sunlight[2]=-1;}
        shadowDraws[1].mvp[12]=0;shadowDraws[1].mvp[14]=0.2f;
        shadowPixel(2);require(pixel[0]>250 && pixel[1]>=126 && pixel[1]<=128,"cloud alpha blends over opaque ground");
        shadowDraws[1].mvp[14]=-0.3f;
        shadowPixel(2);require(pixel[0]>250 && pixel[1]>250,"opaque foreground occludes clouds behind it");
        shadowDraws[1].mvp[14]=0.2f;
        const OrbParticle farExhaust{{0,0,1.3f,2},{2,2},{0,0,1,1}};
        require(orb_gpu_particles_upload(gpu.get(),&farExhaust,1),"exhaust behind cloud");
        shadowPixel(2);require(pixel[0]>125 && pixel[2]>120 && pixel[2]<135,"near cloud blends over distant exhaust");
        const OrbParticle nearExhaust{{0,0,0.45f,0.5f},{0.5f,0.5f},{0,1,0,1}};
        const OrbParticle unorderedExhaust[]={nearExhaust,farExhaust};
        require(orb_gpu_particles_upload(gpu.get(),unorderedExhaust,2),"unsorted exhaust straddles cloud depth");
        shadowPixel(2);require(pixel[1]>250 && pixel[0]<5 && pixel[2]<5,"near exhaust renders after cloud while same-texture distant exhaust renders before it");
        require(orb_gpu_particles_upload(gpu.get(),nullptr,0),"clear cloud exhaust test");
        // Two flames in one mesh straddle an opaque smoke particle. The near
        // group is deliberately submitted first, opposite the blending order.
        OrbMeshVertex flames[6]{};
        for(unsigned layer=0;layer<2;++layer) {
            const float z=layer?4.f:2.f;
            const float xy[3][2]={{-1,-1},{3,-1},{-1,3}};
            for(unsigned v=0;v<3;++v) {
                auto &vertex=flames[layer*3+v];
                vertex.position[0]=xy[v][0]*z;vertex.position[1]=xy[v][1]*z;vertex.position[2]=z;
                vertex.color[layer?2:0]=1;vertex.color[3]=.5f;
                vertex.light[0]=vertex.light[1]=vertex.light[2]=1;
            }
        }
        const uint32_t flameIndices[]={0,1,2,3,4,5};
        const auto flameMesh=orb_gpu_mesh_create(gpu.get(),flames,6,flameIndices,6);
        require(flameMesh,"batched flame geometry");
        const OrbMeshGroup flameGroups[]={{0,3,0,0,1,1,0,{0,0,2}},{3,3,0,0,1,1,0,{0,0,4}}};
        require(orb_gpu_mesh_groups(gpu.get(),flameMesh,flameGroups,2),"separate flame sort centres");
        const OrbParticle smoke{{0,0,.01f,3},{3,3},{0,1,0,1}};
        require(orb_gpu_particles_upload(gpu.get(),&smoke,1),"smoke between batched flames");
        OrbMeshDraw flameDraw{};flameDraw.mesh=flameMesh;
        flameDraw.mvp[0]=flameDraw.mvp[5]=flameDraw.mvp[11]=1;flameDraw.mvp[14]=.01f;
        auto flamePixel=[&]() {
            require(orb_gpu_mesh_render_scene(gpu.get(),target,&flameDraw,1),"render sorted batched flames");
            require(orb_gpu_surface_read(gpu.get(),target,pixels.data(),pixels.size(),128),"read sorted flames");
            require(pixel[0]>=126 && pixel[0]<=129 && pixel[1]>=126 && pixel[1]<=129 && pixel[2]<2,
                    "smoke hides far flame while near flame blends over smoke");
        };
        flamePixel();
        const OrbMeshGroup reversedFlames[]={flameGroups[1],flameGroups[0]};
        require(orb_gpu_mesh_groups(gpu.get(),flameMesh,reversedFlames,2),"reverse batched flame group order");
        flamePixel();
        for(auto &vertex:flames)std::swap(vertex.position[0],vertex.position[2]);
        require(orb_gpu_mesh_update(gpu.get(),flameMesh,flames,6),"rotate flame geometry into local X axis");
        auto rotatedNear=flameGroups[0],rotatedFar=flameGroups[1];
        std::swap(rotatedNear.sort_center[0],rotatedNear.sort_center[2]);
        std::swap(rotatedFar.sort_center[0],rotatedFar.sort_center[2]);
        const OrbMeshGroup rotatedFlames[]={rotatedNear,rotatedFar};
        require(orb_gpu_mesh_groups(gpu.get(),flameMesh,rotatedFlames,2),"updated local flame centres");
        flameDraw.mvp[0]=flameDraw.mvp[11]=0;flameDraw.mvp[3]=flameDraw.mvp[8]=1;
        flamePixel(); // Same screen geometry; sorting must follow the camera transform.
        require(orb_gpu_particles_upload(gpu.get(),nullptr,0),"clear interleaved smoke");
        require(orb_gpu_mesh_release(gpu.get(),flameMesh),"release flame fixture");
        require(orb_gpu_mesh_release(gpu.get(),casterMesh),"shadow caster release");
        require(orb_gpu_surface_release(gpu.get(),cutout),"shadow cutout release");
        require(orb_gpu_mesh_release(gpu.get(),edgeMesh),"edge mesh release");
        require(orb_gpu_surface_release(gpu.get(),edgeTexture),"edge texture release");
        require(orb_gpu_mesh_release(gpu.get(),mesh),"mesh release");
        require(!orb_gpu_mesh_render(gpu.get(),target,mesh,identity),"stale mesh rejected");
        std::cout<<"Indexed mesh rendering and reversed-Z occlusion verified\n";
        return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
