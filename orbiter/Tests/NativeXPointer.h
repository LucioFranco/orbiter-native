#pragma once
#include "Orbiter.h"
#include "Vessel.h"
#include "Mesh.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

// Verify the actual NASSP needles against the same frozen scene with each
// needle hidden. A bright Moon or white dial alone cannot satisfy this check.
// These original group indices are defined in LM_VC_Resource.h; both pinned
// asset inventories contain the same 686-group cockpit and black material.
inline void verifyNativeXPointer(Orbiter &host, Vessel *vessel,
                                bool (*read)(BYTE *,size_t,DWORD)) {
    auto check=[](bool ok,const char *why) {if(!ok)throw std::runtime_error(why);};
    auto sdk=oapiGetVesselInterface(reinterpret_cast<OBJHANDLE>(vessel));
    auto graphics=host.GetGraphicsClient();
    const bool specular=host.Cfg()->CfgVisualPrm.bSpecular;
    host.Cfg()->CfgVisualPrm.bSpecular=true;
    auto frame=[&]() {
        host.RenderNativeFrame();
        std::vector<BYTE> pixels(size_t(host.ViewW())*host.ViewH()*4);
        check(read(pixels.data(),pixels.size(),host.ViewW()*4),"read X-pointer scene");
        return pixels;
    };
    for(unsigned seat=0;seat<2;++seat) {
        check(host.LoadNativeVirtualCockpit(seat),"select CDR/LMP X-pointer viewpoint");
        // The first draw of a freshly loaded view can populate terrain/atlas
        // caches. Establish an actually frozen scene before changing a needle.
        auto visible=frame();bool stable=false;
        for(unsigned warm=0;warm<4;++warm) {
            auto next=frame();
            if(next==visible){stable=true;break;}
            visible=std::move(next);
        }
        check(stable,"X-pointer comparison scene reaches a stable frozen frame");
        Mesh *source=nullptr,*owned=nullptr;
        for(UINT i=0;i<sdk->GetMeshCount();++i) {
            auto mesh=static_cast<Mesh *>(sdk->GetMeshTemplate(i));
            if(mesh && mesh->nGroup()==686) {
                source=mesh;owned=static_cast<Mesh *>(graphics->clbkGetMesh(vessel->GetVishandle(),i));break;
            }
        }
        check(source && owned && source!=owned,"find independent original LM cockpit mesh");
        const std::array<DWORD,2> groups{677+seat,679+seat};
        for(DWORD group:groups) {
            const auto needle=source->GetGroup(group);
            const auto material=source->GetMaterial(needle->MtrlIdx);
            check(material && material->diffuse.r==0 && material->diffuse.g==0 && material->diffuse.b==0 &&
                  material->specular.r==1 && material->power==0,"original needle has black diffuse and legacy zero-power white specular");
            const auto flags=owned->GetGroupUsrFlag(group),templateFlags=source->GetGroupUsrFlag(group);
            check(!(flags&2),"X-pointer needle is enabled in the visual");
            GROUPEDITSPEC edit{};edit.flags=GRPEDIT_SETUSERFLAG;edit.UsrFlag=flags|2;
            auto handle=reinterpret_cast<DEVMESHHANDLE>(owned);
            check(oapiEditMeshGroup(handle,group,&edit)==0,"hide only the selected X-pointer needle");
            const auto hidden=frame();
            size_t darkPixels=0;
            for(size_t i=0;i<visible.size();i+=4) {
                if(!std::equal(visible.begin()+i,visible.begin()+i+3,hidden.begin()+i) &&
                   visible[i]<32 && visible[i+1]<32 && visible[i+2]<32 &&
                   hidden[i]>100 && hidden[i+1]>100 && hidden[i+2]>100)++darkPixels;
            }
            edit.UsrFlag=flags;
            check(oapiEditMeshGroup(handle,group,&edit)==0,"restore X-pointer needle");
            const auto restored=frame();
            if(restored!=visible) {
                size_t differences=0;
                for(size_t i=0;i<visible.size();i+=4)
                    if(!std::equal(visible.begin()+i,visible.begin()+i+3,restored.begin()+i))++differences;
                std::cout<<"X-pointer restoration changed pixels="<<differences<<'\n';
            }
            check(restored==visible,"restoring the needle reproduces the frozen cockpit frame");
            check(source->GetGroupUsrFlag(group)==templateFlags,"needle check leaves original mesh template intact");
            std::cout<<"X-pointer "<<(seat?"LMP":"CDR")<<" group "<<group<<" black needle pixels="<<darkPixels<<'\n';
            check(darkPixels>30,"actual X-pointer needle remains visibly black with specular lighting enabled");
        }
    }
    host.Cfg()->CfgVisualPrm.bSpecular=specular;
    check(host.LoadNativeVirtualCockpit(0),"restore commander viewpoint after X-pointer validation");
    host.RenderNativeFrame();
}
