#include "LocalLights.h"
#include "SkyLighting.h"
#include "FocusedShadows.h"
#include "SkyTexture.h"
#include <sstream>
#include <iostream>
#include <limits>
#include <stdexcept>
static void require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
int main() {
    try {
        std::string archive;
        for(unsigned level=1;level<=3;++level) {
            std::string dds(136,'\0');dds.replace(0,4,"DDS ");dds[4]=124;
            dds[12]=dds[16]=4;dds.replace(84,4,"DXT1");dds[128]=char(level);archive+=dds;
        }
        std::istringstream skyInput(archive);const auto overview=ReadSkyOverview(skyInput);
        require(overview.size()==136 && overview[128]==3,"legacy sky reader selects third complete-sphere DDS");
        bool truncated=false;
        try {std::istringstream shortInput(archive.substr(0,archive.size()-1));ReadSkyOverview(shortInput);}
        catch(const std::runtime_error &){truncated=true;}
        require(truncated,"legacy sky reader rejects truncated pixels");
        std::string tiles;
        for(unsigned i=0;i<501;++i) {
            std::string dds(128+32768,char(i%251));
            std::fill_n(dds.begin(),128,0);dds.replace(0,4,"DDS ");dds[4]=124;
            dds[13]=dds[17]=1;dds.replace(84,4,"DXT1");tiles+=dds;
        }
        std::istringstream tiledInput(tiles);const auto atlas=ReadSkyTexture(tiledInput);
        require(atlas.level==8 && atlas.dds.size()==128+8192*3072/2,"highest complete sky level packed in compressed atlas");
        for(unsigned i=0;i<364;++i) {
            const size_t offset=128+(size_t(i/32*64)*2048+(i%32)*64)*8;
            require(atlas.dds[offset]==uint8_t((137+i)%251),"sky atlas preserves every tile index and compressed block");
        }
        std::istringstream overviewInput(archive);require(ReadSkyTexture(overviewInput).level==3,"overview-only sky archive remains supported");
        const auto snapSun=_V(0,0,1);
        const double shadowTexel=8*50.0/2048;
        const auto focusA=_V(100,200,300),focusB=focusA+_V(shadowTexel*0.1,shadowTexel*0.1,0);
        const auto snappedA=focusA+ShadowFocusCorrection(focusA,snapSun,50);
        const auto snappedB=focusB+ShadowFocusCorrection(focusB,snapSun,50);
        require(length(snappedA-snappedB)<1e-10,"sub-texel focus motion preserves sunlight shadow grid");
        const auto focusC=focusA+_V(shadowTexel,0,0);
        const auto snappedC=focusC+ShadowFocusCorrection(focusC,snapSun,50);
        require(std::abs(length(snappedC-snappedA)-shadowTexel)<1e-10,"shadow focus advances in whole outer texels");
        require(std::abs(dotp(ShadowFocusCorrection(focusA,snapSun,50),snapSun))<1e-10,"shadow snapping preserves depth range");
        OrbMeshDraw projected{};
        projected.mvp[0]=2;projected.mvp[5]=3;projected.mvp[11]=1;
        projected.mvp[12]=200;projected.mvp[13]=600;projected.mvp[15]=300;
        FocusedShadow(projected,_V(100,200,300),_V(0,0,1),50,2,3);
        require(projected.shadow_mvp[12]==0 && projected.shadow_mvp[13]==0 && projected.shadow_mvp[14]==0.5f,"shadow volume follows focus independently of camera translation");
        require(std::abs(projected.shadow_mvp[0]-0.02f)<1e-7 && std::abs(projected.shadow_mvp[5]-0.02f)<1e-7 && std::abs(projected.shadow_mvp[10]-0.01f)<1e-7,"shadow projection removes camera aspect and aperture scaling");
        OrbMeshDraw rolled{};
        rolled.mvp[1]=-3;rolled.mvp[4]=2;rolled.mvp[11]=1;
        rolled.mvp[12]=400;rolled.mvp[13]=-300;rolled.mvp[15]=300;
        FocusedShadow(rolled,_V(200,-100,300),_V(0,0,1),50,2,3,_V(1,0,0));
        for(int i=0;i<16;++i)require(std::abs(rolled.shadow_mvp[i]-projected.shadow_mvp[i])<1e-7,"camera roll preserves the sunlight shadow projection");
        FocusedShadow(projected,_V(100,200,300),_V(0,1,0),50,2,3);
        for(float value:projected.shadow_mvp)require(std::isfinite(value),"overhead Sun uses a nondegenerate shadow basis");
        const auto center=_V(1.5e11,0,0);const double earthRadius=6371000;
        const auto surface=center+_V(0,earthRadius,0);
        using orbiter::graphics::EarthSkyAmbient;
        require(std::abs(EarthSkyAmbient(surface,center,earthRadius,_V(0,1,0))-0.25f)<1e-6,"daytime ground receives diffuse skylight");
        require(EarthSkyAmbient(surface,center,earthRadius,_V(0,-1,0))==0.03f,"night keeps dark ambient");
        require(EarthSkyAmbient(surface+_V(0,200000,0),center,earthRadius,_V(0,1,0))<0.03001f,"spacecraft in orbit do not receive ground sky fill");
        const auto twilight=EarthSkyAmbient(surface,center,earthRadius,_V(1,0,0));
        require(twilight>0.03f && twilight<0.25f,"twilight transitions smoothly");
        PointLight point(nullptr,_V(1,2,3),4,1,2,3);
        OrbLocalLight packed{};
        point.SetVisibility(LightEmitter::VIS_EXTERNAL);
        require(!CockpitLight(point,packed),"external emitters excluded from cockpit");
        point.SetVisibility(LightEmitter::VIS_COCKPIT);
        double intensity=0.25;VECTOR3 position=_V(2,3,4);
        point.SetIntensityRef(&intensity);point.SetPositionRef(&position);
        require(CockpitLight(point,packed) && packed.diffuse[0]==0.25f && packed.position[0]==2,"referenced light values sampled");
        intensity=0.5;position.x=5;
        require(CockpitLight(point,packed) && packed.diffuse[0]==0.5f && packed.position[0]==5,"referenced values refreshed");
        point.Activate(false);require(!CockpitLight(point,packed),"inactive lights excluded");
        point.Activate(true);intensity=0;require(!CockpitLight(point,packed),"zero intensity excluded");
        intensity=std::numeric_limits<double>::infinity();require(!CockpitLight(point,packed),"nonfinite intensity excluded");
        intensity=1;position.y=std::numeric_limits<double>::quiet_NaN();require(!CockpitLight(point,packed),"nonfinite position excluded");
        SpotLight spot(nullptr,_V(0,0,0),_V(0,0,2),3,1,0,0,0,PI/2);
        spot.SetVisibility(LightEmitter::VIS_ALWAYS);
        require(CockpitLight(spot,packed) && packed.diffuse[3]==1 && packed.direction[2]==1,"spot direction normalized");
        require(std::abs(packed.direction[3]-std::sqrt(0.5))<1e-6 && packed.attenuation[3]==1,"SDK apertures converted as full cone angles");
        spot.SetCameraRelative(true);spot.SetPosition(_V(0,0,0.25));
        const auto yaw=_M(0,0,1,0,1,0,-1,0,0);
        require(CockpitLight(spot,packed,_V(2,3,4),yaw) && packed.position[0]==2.25f && packed.position[1]==3 && packed.position[2]==4,"camera light offset rotated and translated into vessel frame");
        require(packed.direction[0]==1 && packed.direction[1]==0 && packed.direction[2]==0,"camera light direction rotates with view");
        require(CockpitLight(spot,packed,_V(5,6,7),yaw) && packed.position[0]==5.25f && packed.position[1]==6,"camera light follows new seat without changing emitter");
        spot.SetDirection(_V(0,0,0));require(!CockpitLight(spot,packed),"degenerate spot direction excluded");
        std::cout<<"Native cockpit light filtering and referenced properties verified\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
