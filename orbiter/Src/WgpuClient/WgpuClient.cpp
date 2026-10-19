#include "Orbiter.h"
#include "GraphicsAPI.h"
#include "Camera.h"
#include "Mesh.h"
#include "NativeParticleStream.h"
#include "ParticleLighting.h"
#include "SkyLighting.h"
#include "FocusedShadows.h"
#include "ExhaustGeometry.h"
#include "SkyTexture.h"
#include "TerrainTextureBorder.h"
#include "Planet.h"
#include "elevmgr.h"
#include "Terrain/TileGeometry.h"
#include "Terrain/TileSelection.h"
#include <cstring>
#include <string>
#include "AnimationPose.h"
#include "StarCatalog.h"
#include "LocalLights.h"
#include "NativeSketchpad.h"
#include "NativeImGuiDraw.h"
#include <cmath>
#include "Platform/Vfs.h"
#include "WgpuBridge.h"
#include <algorithm>
#include <climits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// OrbiterCore exports these global data symbols. MSVC consumers need an
// explicit import declaration even with WINDOWS_EXPORT_ALL_SYMBOLS enabled.
extern DLLIMPORT Orbiter *g_pOrbiter;
extern DLLIMPORT Camera *g_camera;
namespace {
class WgpuClient final : public oapi::GraphicsClient {
    struct Surface {
        uint64_t id;
        DWORD width, height;
        unsigned references = 1;
        std::vector<OrbDraw> commands;
        DWORD key = 0;
        bool hasKey = false;
        bool writable = true;
        SURFHANDLE emission = nullptr;
    };
    std::vector<std::unique_ptr<orbiter::graphics::NativeParticleStream>> streams_;
    struct ExhaustMesh {uint64_t mesh=0;size_t vertices=0;};
    std::unordered_map<OBJHANDLE,ExhaustMesh> exhaustMeshes_;
    SURFHANDLE defaultExhaust_=nullptr;
    bool exhaustVisible_=true;
    void ClearExhausts() {
        for(const auto &[object,batch]:exhaustMeshes_)if(batch.mesh)orb_gpu_mesh_release(gpu_,batch.mesh);
        exhaustMeshes_.clear();
        if(defaultExhaust_)clbkReleaseSurface(defaultExhaust_);
        defaultExhaust_=nullptr;
    }
    oapi::ParticleStream *NewStream(PARTICLESTREAMSPEC *spec,bool reverse=false) {
        if(!spec || !(spec->srcrate>0) || !(spec->lifetime>0) || streams_.size()>=1024)return nullptr;
        auto stream=std::make_unique<orbiter::graphics::NativeParticleStream>(this,spec,reverse);
        auto result=stream.get();streams_.push_back(std::move(stream));return result;
    }
    OrbGpu *gpu_ = nullptr;
    mutable uint64_t sequence_ = 0;
    bool attached_ = false;
    bool imguiInitialized_=false;
    bool starsLoaded_=false;size_t starCount_=0;
    uint64_t skyTexture_=0;
    unsigned skyLevel_=3;std::string skyPath_;
    uint64_t starImage_=0;unsigned starImageLevel_=3;std::string starImagePath_;
    struct TerrainTexture {
        OrbGpu *gpu;uint64_t id;
        orbiter::terrain::TileAddress source;
        bool bordered=false;
        ~TerrainTexture(){if(id)orb_gpu_surface_release(gpu,id);}
    };
    struct TerrainMesh {uint64_t mesh=0;std::shared_ptr<TerrainTexture> texture;std::array<double,3> origin;};
    std::unordered_map<OBJHANDLE,std::unordered_map<uint64_t,TerrainMesh>> planetMeshes_;
    std::unordered_map<OBJHANDLE,std::unique_ptr<ZTreeMgr>> surfaceTrees_;
    std::unordered_map<OBJHANDLE,std::unordered_map<uint64_t,std::weak_ptr<TerrainTexture>>> terrainTextures_;
    decltype(planetMeshes_) cloudMeshes_;
    decltype(surfaceTrees_) cloudTrees_;
    decltype(terrainTextures_) cloudTextures_;
    void ReleaseTerrainMesh(TerrainMesh &tile) {
        if(tile.mesh)orb_gpu_mesh_release(gpu_,tile.mesh);
        tile.texture.reset();
    }
    std::shared_ptr<TerrainTexture> LoadTerrainTexture(OBJHANDLE body,const char *name,ZTreeMgr *tree,
        orbiter::terrain::TileAddress requested,std::array<float,3> &uv,bool cloud=false) {
        const auto create=std::getenv("ORBITER_DIAGNOSTIC_NO_TERRAIN_MIPS")?orb_gpu_dds_create:orb_gpu_dds_create_mipped;
        const auto flags=g_pOrbiter->Cfg()->CfgPRenderPrm.TileLoadFlags;
        auto read=[&](orbiter::terrain::TileAddress source) {
            std::vector<uint8_t> bytes;
            if(flags&1) {
                char file[256],path[256];
                std::snprintf(file,sizeof(file),"%s/%s/%02u/%06u/%06u.dds",name,cloud?"Cloud":"Surf",source.level,source.latitude,source.longitude);
                g_pOrbiter->Cfg()->PTexPath(path,file);
                std::ifstream input(orbiter::platform::ResolvePath(path),std::ios::binary|std::ios::ate);
                if(input) {
                    const auto size=input.tellg();
                    if(size>=128 && size<=256*1024*1024) {
                        bytes.resize(static_cast<size_t>(size));input.seekg(0);
                        if(!input.read(reinterpret_cast<char *>(bytes.data()),size))bytes.clear();
                    }
                }
            }
            if(bytes.empty() && tree) {
                BYTE *data=nullptr;const auto size=tree->ReadData(source.level,source.latitude,source.longitude,&data);
                std::unique_ptr<BYTE[]> owner(data);
                if(size)bytes.assign(data,data+size);
            }
            return bytes;
        };
        auto borderedUV=[&]() {uv[0]=.25f+.5f*uv[0];uv[1]=.25f+.5f*uv[1];uv[2]*=.5f;};
        for(auto source=requested;;--source.level,source.latitude/=2,source.longitude/=2) {
            const uint64_t key=(uint64_t(source.level)<<56)|(uint64_t(source.latitude)<<28)|source.longitude;
            auto &resident=(cloud?cloudTextures_:terrainTextures_)[body][key];
            auto shared=resident.lock();
            const unsigned factor=1u<<(requested.level-source.level);
            uv={float(requested.longitude%factor)/factor,float(requested.latitude%factor)/factor,1.0f/factor};
            if(shared) {if(shared->bordered)borderedUV();return shared;}
            uint64_t texture=0;
            auto bytes=read(source);bool bordered=false;
            if(!bytes.empty() && !cloud && !std::getenv("ORBITER_DIAGNOSTIC_NO_TERRAIN_BORDER")) {
                const unsigned bands=1u<<(source.level-4);
                if(source.latitude>0 && source.latitude+1<bands) {
                    std::array<std::vector<uint8_t>,9> neighbors;
                    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                        const unsigned index=(dy+1)*3+dx+1;
                        neighbors[index]=index==4?bytes:read({source.level,unsigned(int(source.latitude)+dy),
                            unsigned((int(source.longitude)+dx+int(bands*2))%int(bands*2))});
                    }
                    auto padded=BorderTerrainTexture(neighbors);
                    if(!padded.empty()){bytes=std::move(padded);bordered=true;}
                }
            }
            if(!bytes.empty())texture=create(gpu_,bytes.data(),bytes.size());
            // Preserve archive fallback when an overriding loose DDS is invalid.
            if(!texture && tree) {
                BYTE *data=nullptr;const auto size=tree->ReadData(source.level,source.latitude,source.longitude,&data);
                std::unique_ptr<BYTE[]> owner(data);
                if(size)texture=create(gpu_,data,size);
                bordered=false;
            }
            if(texture) {
                shared=std::shared_ptr<TerrainTexture>(new TerrainTexture{gpu_,texture,source,bordered});
                if(bordered)borderedUV();
                resident=shared;return shared;
            }
            if(source.level==4)break;
        }
        uv={0,0,1};return {};
    }
    void ClearTerrain() {
        orb_gpu_background(gpu_,0,nullptr);
        orb_gpu_star_image(gpu_,0,nullptr);
        if(starImage_)orb_gpu_surface_release(gpu_,starImage_);
        starImage_=0;starImagePath_.clear();
        if(skyTexture_)orb_gpu_surface_release(gpu_,skyTexture_);
        skyTexture_=0;skyPath_.clear();
        for(auto &[body,tiles]:planetMeshes_)for(auto &[key,tile]:tiles)ReleaseTerrainMesh(tile);
        for(auto &[body,tiles]:cloudMeshes_)for(auto &[key,tile]:tiles)ReleaseTerrainMesh(tile);
        planetMeshes_.clear();surfaceTrees_.clear();terrainTextures_.clear();
        cloudMeshes_.clear();cloudTrees_.clear();cloudTextures_.clear();
    }
    SURFHANDLE output_ = nullptr;
    OBJHANDLE liveVessel_ = nullptr;
    struct Lighting {oapi::FVECTOR4 light{},emission{};};
    struct VertexRange {size_t start,count;VECTOR3 offset;Mesh *mesh;DWORD material;bool edited=false;};
    struct RenderData {
        size_t shadowOnlyGroups=0;
        uint64_t mesh=0;float mvp[16]{};
        std::vector<OrbMeshVertex> baseVertices,animatedVertices;
        std::vector<OrbMeshGroup> groups;
        std::vector<orbiter::graphics::Pose> previousPoses;
        std::vector<VertexRange> animatedRanges;
        std::vector<orbiter::graphics::AnimatedGroup> animatedGroups;
    } preview_;
    // Selected only while preparing a visual; device callbacks resolve their owner independently.
    RenderData *render_=&preview_;
    struct Visual {
        OBJHANDLE object=nullptr;std::vector<std::unique_ptr<Mesh>> meshes;
        std::vector<WORD> visibility;
        std::unordered_map<Mesh *,std::unordered_map<DWORD,Lighting>> lighting;
        std::unordered_map<Mesh *,std::unordered_set<DWORD>> dirtyMaterials;
        bool dirty=true,reset=false,cockpit=true;RenderData render;
        std::array<uint64_t,MAXMFD> mfdLayout{};
    };
    Lighting MeshLighting(Mesh *mesh,DWORD material) const {
        if(auto owner=MeshOwner(reinterpret_cast<DEVMESHHANDLE>(mesh))) {
            auto found=owner->lighting.find(mesh);
            if(found!=owner->lighting.end()) {auto value=found->second.find(material);if(value!=found->second.end())return value->second;}
        }
        Lighting result{};
        if(auto mat=mesh->GetMaterial(material))result.light={mat->emissive.r,mat->emissive.g,mat->emissive.b,mat->emissive.a};
        return result;
    }
    std::unordered_map<OBJHANDLE,std::unique_ptr<Visual>> visuals_;
    Visual *visual_=nullptr;
    Visual *MeshOwner(DEVMESHHANDLE handle) const {
        for(const auto &[object,visual]:visuals_)
            for(const auto &mesh:visual->meshes)if(mesh.get()==reinterpret_cast<Mesh *>(handle))return visual.get();
        return nullptr;
    }
    Mesh *DeviceMesh(DEVMESHHANDLE handle) const {
        return MeshOwner(handle)?reinterpret_cast<Mesh *>(handle):nullptr;
    }
    void DestroyVisual(OBJHANDLE object) {
        auto it=visuals_.find(object);if(it==visuals_.end())return;
        UnregisterVisObject(object);
        if(it->second->render.mesh)orb_gpu_mesh_release(gpu_,it->second->render.mesh);
        if(visual_==it->second.get()){visual_=nullptr;render_=&preview_;}
        visuals_.erase(it);
    }
    void DestroyVisuals() {
        ClearExhausts();
        while(!visuals_.empty())DestroyVisual(visuals_.begin()->first);
        liveVessel_=nullptr;
    }
    void EnsureVisual(OBJHANDLE object,VESSEL *vessel,bool cockpit) {
        auto it=visuals_.find(object);
        if(it!=visuals_.end() && it->second->reset)DestroyVisual(object);
        auto &entry=visuals_[object];
        const bool created=!entry;
        if(created) {
            entry=std::make_unique<Visual>();entry->object=object;
            for(UINT i=0;i<vessel->GetMeshCount();++i) {
                entry->visibility.push_back(vessel->GetMeshVisibilityMode(i));
                auto source=static_cast<Mesh *>(vessel->GetMeshTemplate(i));
                entry->meshes.push_back(source?std::make_unique<Mesh>(*source):nullptr);
            }
        }
        visual_=entry.get();render_=&visual_->render;
        if(visual_->cockpit!=cockpit){visual_->cockpit=cockpit;visual_->dirty=true;}
        if(created)RegisterVisObject(object,reinterpret_cast<VISHANDLE>(visual_));
    }
    struct MeshInstance { MESHHANDLE mesh; VECTOR3 offset; UINT index=0; bool shadowOnly=false; };
    mutable std::unordered_map<SURFHANDLE, Surface> surfaces_;
    Surface *Find(SURFHANDLE handle) const {
        auto it = surfaces_.find(handle);
        return it == surfaces_.end() ? nullptr : &it->second;
    }
    bool Writable(Surface *surface) const {
        if (!surface) return false;
        if (surface->writable) return true;
        if (!FlushAll() || !orb_gpu_surface_make_writable(gpu_,surface->id)) return false;
        surface->writable=true;return true;
    }
    bool CockpitMFD(UINT mesh,DWORD group,Surface *&surface) const {
        surface=nullptr;
        if(!visual_ || !visual_->cockpit || visual_->object!=oapiGetFocusObject())return false;
        for(int id=0;id<MAXMFD;++id) {
            const VCMFDSPEC *spec=nullptr;auto handle=GetVCMFDSurface(id,&spec);
            if(spec && spec->nmesh==mesh && spec->ngroup==group) {surface=Find(handle);return true;}
        }
        return false;
    }
    bool FlushAll() const {
        for (auto &entry : surfaces_) {
            auto &surface=entry.second;
            if (surface.commands.empty()) continue;
            if (!orb_gpu_queue_batch(gpu_,surface.id,surface.commands.data(),surface.commands.size())) return false;
            surface.commands.clear();
        }
        return orb_gpu_flush(gpu_) != 0;
    }
public:
    oapi::ParticleStream *clbkCreateParticleStream(PARTICLESTREAMSPEC *spec) override {return NewStream(spec);}
    oapi::ParticleStream *clbkCreateExhaustStream(PARTICLESTREAMSPEC *spec,OBJHANDLE vessel,const double *level,const VECTOR3 &pos,const VECTOR3 &dir) override {
        auto stream=NewStream(spec);if(stream)stream->Attach(vessel,pos,dir,level);return stream;
    }
    oapi::ParticleStream *clbkCreateExhaustStream(PARTICLESTREAMSPEC *spec,OBJHANDLE vessel,const double *level,const VECTOR3 *pos,const VECTOR3 *dir) override {
        auto stream=NewStream(spec,true);if(stream)stream->Attach(vessel,pos,dir,level);return stream;
    }
    void clbkUpdate(bool running) override {
        if(!running)return;
        for(auto &stream:streams_)stream->Advance(oapiGetSimStep());
        streams_.erase(std::remove_if(streams_.begin(),streams_.end(),[](const auto &stream){return stream->Finished();}),streams_.end());
    }
    explicit WgpuClient(HINSTANCE instance) : GraphicsClient(instance) {}
    ~WgpuClient() override { clbkImGuiShutdown();streams_.clear();ClearTerrain();DestroyVisuals();if(preview_.mesh)orb_gpu_mesh_release(gpu_,preview_.mesh);orb_gpu_destroy(gpu_); }
    bool clbkInitialise() override {
        if (!gpu_) gpu_ = orb_gpu_create();
        if(gpu_)clbkImGuiInit();
        return gpu_ != nullptr;
    }
    bool clbkAttachNativeWindow(const OrbNativeWindow *window) override {
        if (!orb_gpu_window_attach(gpu_,window)) throw std::runtime_error(orb_gpu_error());
        attached_ = true;
        return true;
    }
    void clbkDetachNativeWindow() override {
        orb_gpu_window_detach(gpu_); attached_ = false;
    }
    void clbkResizeNativeWindow(DWORD w, DWORD h) override {
        if (attached_ && !orb_gpu_window_resize(gpu_,w,h)) throw std::runtime_error(orb_gpu_error());
    }
    bool clbkDisplayFrame() override {
        if (!attached_) return false;
        auto source = Find(output_);
        if (!orb_gpu_present(gpu_,source ? source->id : 0)) throw std::runtime_error(orb_gpu_error());
        return true;
    }
    bool SetOutput(SURFHANDLE handle) {
        if (handle && !Find(handle)) return false;
        if (handle) clbkIncrSurfaceRef(handle);
        if (output_) clbkReleaseSurface(output_);
        output_ = handle;
        return true;
    }
    bool PreviewMesh(MESHHANDLE mesh, const float *mvp, DWORD width, DWORD height) {
        return PreviewMeshes({{mesh,{0,0,0}}},mvp,width,height);
    }
    bool PreviewMeshes(const std::vector<MeshInstance> &instances, const float *mvp, DWORD width, DWORD height) {
        if (instances.empty() || !mvp) return false;
        render_->animatedRanges.clear();render_->animatedGroups.clear();
        render_->shadowOnlyGroups=0;
        std::vector<OrbMeshVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<OrbMeshGroup> groups;
        for (const auto &instance:instances) {
        auto mesh=instance.mesh;
        if (!mesh) continue;
        DWORD materialIndex=SPEC_DEFAULT, textureIndex=SPEC_DEFAULT;
        for (DWORD i=0;i<oapiMeshGroupCount(mesh);++i) {
            auto group=oapiMeshGroup(mesh,i);
            if (!group || !group->nVtx || !group->nIdx) continue;
            if (group->MtrlIdx!=SPEC_INHERIT) materialIndex=group->MtrlIdx;
            if (group->TexIdx!=SPEC_INHERIT) textureIndex=group->TexIdx;
            if(group->UsrFlag&2)continue;
            auto material=oapiMeshMaterial(mesh,materialIndex);
            COLOUR4 color=material ? material->diffuse : COLOUR4{0.8f,0.8f,0.8f,1};
            if (color.a<=0) continue;
            auto texture=textureIndex==SPEC_DEFAULT ? nullptr : Find(oapiGetTextureHandle(mesh,textureIndex+1));
            auto emission=texture?Find(texture->emission):nullptr;
            Surface *mfd=nullptr;const bool screen=!instance.shadowOnly && CockpitMFD(instance.index,i,mfd);
            if(screen) {texture=emission=mfd;color=mfd?COLOUR4{1,1,1,1}:COLOUR4{0,0,0,1};}
            groups.push_back({uint32_t(indices.size()),group->nIdx,texture ? texture->id : 0,emission ? emission->id : 0,screen?1u:0u,color.a<1?1u:0u,instance.shadowOnly?1u:0u});
            if(instance.shadowOnly)++render_->shadowOnlyGroups;
            const auto base=uint32_t(vertices.size());
            render_->animatedGroups.push_back({instance.index,i});
            render_->animatedRanges.push_back({base,group->nVtx,instance.offset,static_cast<Mesh *>(mesh),materialIndex});
            const auto lighting=MeshLighting(static_cast<Mesh *>(mesh),materialIndex);
            for (DWORD v=0;v<group->nVtx;++v) {
                const auto &p=group->Vtx[v];
                vertices.push_back({{float(p.x+instance.offset.x),float(p.y+instance.offset.y),float(p.z+instance.offset.z)},{p.nx,p.ny,p.nz},{p.tu,p.tv},{color.r,color.g,color.b,color.a},{lighting.light.r,lighting.light.g,lighting.light.b,lighting.light.a},{lighting.emission.r,lighting.emission.g,lighting.emission.b,lighting.emission.a}});
                if(screen) {
                    auto &vertex=vertices.back();
                    for(unsigned k=0;k<3;++k)vertex.emission[k]=1;
                }
                if(material && !screen) {
                    auto &vertex=vertices.back();
                    vertex.specular[0]=material->specular.r;vertex.specular[1]=material->specular.g;
                    vertex.specular[2]=material->specular.b;vertex.specular[3]=material->power;
                }
            }
            for (DWORD n=0;n<group->nIdx;++n) {
                if (group->Idx[n]>=group->nVtx) return false;
                indices.push_back(base+group->Idx[n]);
            }
            if(groups.back().alpha_blend) {
                VECTOR3 centre{};
                for(size_t v=base;v<vertices.size();++v)
                    for(unsigned k=0;k<3;++k)centre.data[k]+=vertices[v].position[k];
                for(unsigned k=0;k<3;++k)groups.back().sort_center[k]=float(centre.data[k]/group->nVtx);
            }
        }
        }
        uint64_t uploaded=0;
        if(!vertices.empty()) {
            uploaded=orb_gpu_mesh_create(gpu_,vertices.data(),vertices.size(),indices.data(),indices.size());
            if (!uploaded) throw std::runtime_error(orb_gpu_error());
            if (!orb_gpu_mesh_groups(gpu_,uploaded,groups.data(),groups.size())) {
                const std::string error=orb_gpu_error(); orb_gpu_mesh_release(gpu_,uploaded); throw std::runtime_error(error);
            }
        }
        auto existing=Find(output_);
        auto target=existing && existing->width==width && existing->height==height?output_:clbkCreateSurfaceEx(width,height,0);
        if (!target) {if(uploaded)orb_gpu_mesh_release(gpu_,uploaded);return false;}
        if(target==output_)clbkIncrSurfaceRef(target);
        if(!uploaded)clbkFillSurface(target,0);
        if (render_->mesh) orb_gpu_mesh_release(gpu_,render_->mesh);
        render_->mesh=uploaded;
        render_->groups=std::move(groups);
        render_->baseVertices=std::move(vertices);
        render_->animatedVertices=render_->baseVertices;render_->previousPoses.clear();
        std::copy(mvp,mvp+16,render_->mvp);
        SetOutput(target);
        clbkReleaseSurface(target);
        return true;
    }
    void UpdateVesselVisual(OBJHANDLE object,bool cockpit) {
        auto vessel=oapiGetVesselInterface(object);
        EnsureVisual(object,vessel,cockpit);
        const DWORD width=g_pOrbiter->ViewW(),height=g_pOrbiter->ViewH();
        // Use the local cockpit position, avoiding subtraction of astronomical
        // global positions. Camera and vessel rotations remain double precision.
        VECTOR3 eye{};
        if(cockpit) {const auto localEye=g_camera->CockpitPos();eye=_V(localEye.x,localEye.y,localEye.z);}
        else {
            VECTOR3 cameraPosition{},vesselPosition{};MATRIX3 vesselRotation{};
            oapiCameraGlobalPos(&cameraPosition);vessel->GetGlobalPos(vesselPosition);vessel->GetRotationMatrix(vesselRotation);
            eye=tmul(vesselRotation,cameraPosition-vesselPosition);
        }
        MATRIX3 camera,rotation; oapiCameraRotationMatrix(&camera); vessel->GetRotationMatrix(rotation);
        MATRIX3 local{};
        for (int r=0;r<3;++r) for (int c=0;c<3;++c)
            for (int k=0;k<3;++k) local.data[r*3+c]+=camera.data[k*3+r]*rotation.data[k*3+c];
        const double f=1/std::tan(oapiCameraAperture());
        const double scales[3]={f*double(height)/width,f,1};
        float mvp[16]{};
        for (int row=0;row<3;++row) {
            const int dest=row==2?3:row;
            for (int col=0;col<3;++col) mvp[col*4+dest]=float(scales[row]*local.data[row*3+col]);
            mvp[12+dest]=float(-scales[row]*(local.data[row*3]*eye.x+local.data[row*3+1]*eye.y+local.data[row*3+2]*eye.z));
        }
        mvp[14]=0.01f; // infinite far, reversed Z
        std::array<uint64_t,MAXMFD> layout;layout.fill(UINT64_MAX);
        if(cockpit && object==oapiGetFocusObject())for(int id=0;id<MAXMFD;++id) {
            const VCMFDSPEC *spec=nullptr;GetVCMFDSurface(id,&spec);
            if(spec)layout[id]=(uint64_t(spec->nmesh)<<32)|spec->ngroup;
        }
        if(layout!=visual_->mfdLayout) {visual_->mfdLayout=layout;visual_->dirty=true;}
        auto output=Find(output_);
        if (visual_->dirty || !output || output->width!=width || output->height!=height) {
            std::vector<MeshInstance> instances;
            for (UINT i=0;i<vessel->GetMeshCount();++i) {
                const auto visibility=vessel->GetMeshVisibilityMode(i);
                const bool visible=visibility&(cockpit?(MESHVIS_VC|MESHVIS_COCKPIT):MESHVIS_EXTERNAL);
                const bool shadowOnly=cockpit && !visible && (visibility&MESHVIS_EXTERNAL);
                if(!visible && !shadowOnly)continue;
                VECTOR3 offset{};vessel->GetMeshOffset(i,offset);
                instances.push_back({visual_->meshes[i].get(),offset,i,shadowOnly});
            }
            if (instances.empty()) {
                if(render_->mesh) {orb_gpu_mesh_release(gpu_,render_->mesh);render_->mesh=0;}
                render_->shadowOnlyGroups=0;
                std::copy(mvp,mvp+16,render_->mvp); // Exhausts still need the current vessel transform.
                visual_->dirty=false;return;
            }
            if (!PreviewMeshes(instances,mvp,width,height)) throw std::runtime_error("Cannot create vessel cockpit mesh");
            visual_->dirty=false;
        }
        ANIMATION *animations=nullptr;const auto count=vessel->GetAnimPtr(&animations);
        const auto poses=orbiter::graphics::AnimationPoses(animations,count,render_->animatedGroups);
        std::vector<OrbMeshRange> updates;
        bool transparentGroupsMoved=false;
        for(size_t g=0;g<poses.size();++g) {
            auto &range=render_->animatedRanges[g];const auto &pose=poses[g];
            if(!range.edited && render_->previousPoses.size()==poses.size() &&
               std::equal(pose.r.data,pose.r.data+9,render_->previousPoses[g].r.data) &&
               std::equal(pose.t.data,pose.t.data+3,render_->previousPoses[g].t.data))continue;
            range.edited=false;
            if(!updates.empty() && updates.back().first+updates.back().count==range.start)updates.back().count+=uint32_t(range.count);
            else updates.push_back({uint32_t(range.start),uint32_t(range.count)});
            for(size_t i=range.start;i<range.start+range.count;++i) {
                auto &vertex=render_->animatedVertices[i];vertex=render_->baseVertices[i];
                const auto point=pose.Point(_V(vertex.position[0],vertex.position[1],vertex.position[2])-range.offset)+range.offset;
                const auto normal=pose.Normal(_V(vertex.normal[0],vertex.normal[1],vertex.normal[2]));
                for(int k=0;k<3;++k) {
                    if(!std::isfinite(point.data[k]) || !std::isfinite(normal.data[k]))
                        throw std::runtime_error("Invalid animation result mesh "+std::to_string(render_->animatedGroups[g].mesh)+" group "+std::to_string(render_->animatedGroups[g].group)+" vertex "+std::to_string(i-range.start)+" base normal "+std::to_string(vertex.normal[0])+","+std::to_string(vertex.normal[1])+","+std::to_string(vertex.normal[2]));
                    vertex.position[k]=float(point.data[k]);vertex.normal[k]=float(normal.data[k]);
                }
            }
            if(render_->groups[g].alpha_blend) {
                VECTOR3 centre{};
                for(size_t i=range.start;i<range.start+range.count;++i)
                    for(unsigned k=0;k<3;++k)centre.data[k]+=render_->animatedVertices[i].position[k];
                for(unsigned k=0;k<3;++k)render_->groups[g].sort_center[k]=float(centre.data[k]/range.count);
                transparentGroupsMoved=true;
            }
        }
        if(!visual_->dirtyMaterials.empty()) {
            for(const auto &range:render_->animatedRanges) {
                auto mesh=visual_->dirtyMaterials.find(range.mesh);
                if(mesh==visual_->dirtyMaterials.end() || !mesh->second.count(range.material))continue;
                updates.push_back({uint32_t(range.start),uint32_t(range.count)});
                const auto lighting=MeshLighting(range.mesh,range.material);
                for(size_t i=range.start;i<range.start+range.count;++i) {
                    const float light[]={lighting.light.r,lighting.light.g,lighting.light.b,lighting.light.a};
                    const float emission[]={lighting.emission.r,lighting.emission.g,lighting.emission.b,lighting.emission.a};
                    std::copy(light,light+4,render_->baseVertices[i].light);std::copy(emission,emission+4,render_->baseVertices[i].emission);
                    std::copy(light,light+4,render_->animatedVertices[i].light);std::copy(emission,emission+4,render_->animatedVertices[i].emission);
                }
            }
            visual_->dirtyMaterials.clear();
        }
        bool screenTexturesChanged=false;
        if(cockpit)for(size_t g=0;g<render_->animatedRanges.size();++g) {
            const auto &address=render_->animatedGroups[g];Surface *surface=nullptr;
            if(!CockpitMFD(address.mesh,address.group,surface))continue;
            auto &group=render_->groups[g];const uint64_t texture=surface?surface->id:0;
            if(group.texture!=texture || group.emission!=texture) {group.texture=group.emission=texture;screenTexturesChanged=true;}
            const auto &range=render_->animatedRanges[g];const float value=surface?1.f:0.f;
            if(render_->baseVertices[range.start].color[0]!=value || render_->baseVertices[range.start].emission[0]!=1) {
                updates.push_back({uint32_t(range.start),uint32_t(range.count)});
                for(size_t i=range.start;i<range.start+range.count;++i) {
                    for(unsigned k=0;k<3;++k) {
                        render_->baseVertices[i].color[k]=render_->animatedVertices[i].color[k]=value;
                        render_->baseVertices[i].emission[k]=render_->animatedVertices[i].emission[k]=1;
                    }
                    for(unsigned k=0;k<4;++k)render_->baseVertices[i].specular[k]=render_->animatedVertices[i].specular[k]=0;
                }
            }
        }
        // Animation and material changes can overlap; upload each span once.
        std::sort(updates.begin(),updates.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        size_t merged=0;
        for(const auto range:updates) {
            if(merged && range.first<=updates[merged-1].first+updates[merged-1].count) {
                auto &last=updates[merged-1];last.count=std::max(last.first+last.count,range.first+range.count)-last.first;
            } else updates[merged++]=range;
        }
        updates.resize(merged);
        if (!updates.empty() && render_->mesh && !orb_gpu_mesh_update_ranges(gpu_,render_->mesh,render_->animatedVertices.data(),render_->animatedVertices.size(),updates.data(),updates.size()))throw std::runtime_error(orb_gpu_error());
        if((transparentGroupsMoved || screenTexturesChanged) && render_->mesh && !orb_gpu_mesh_groups(gpu_,render_->mesh,render_->groups.data(),render_->groups.size()))throw std::runtime_error(orb_gpu_error());
        render_->previousPoses=poses;
        std::copy(mvp,mvp+16,render_->mvp);
    }
    void clbkCloseSession(bool) override {streams_.clear();orb_gpu_particles_upload(gpu_,nullptr,0);orb_gpu_stars_upload(gpu_,nullptr,0);starsLoaded_=false;starCount_=0;DestroyVisuals();ClearTerrain();}
    void clbkDeleteVessel(OBJHANDLE object) override {DestroyVisual(object);if(liveVessel_==object)liveVessel_=nullptr;}
    MESHHANDLE clbkGetMesh(VISHANDLE visual,UINT index) override {
        for(const auto &[object,entry]:visuals_)
            if(visual==reinterpret_cast<VISHANDLE>(entry.get()))return index<entry->meshes.size()?entry->meshes[index].get():nullptr;
        return nullptr;
    }
    int clbkGetMeshGroup(DEVMESHHANDLE mesh,DWORD group,GROUPREQUESTSPEC *request) override {
        auto owned=DeviceMesh(mesh);return owned && request?owned->GetGroup(group,request):-2;
    }
    int clbkEditMeshGroup(DEVMESHHANDLE mesh,DWORD group,GROUPEDITSPEC *edit) override {
        auto owner=MeshOwner(mesh);auto owned=owner?reinterpret_cast<Mesh *>(mesh):nullptr;if(!owned || !edit)return -2;
        auto data=owned->GetGroup(group);if(!data)return 1;
        if((edit->flags&GRPEDIT_VTXMOD) && edit->nVtx && !edit->Vtx)return -2;
        const DWORD flags=data->UsrFlag;
        std::vector<NTVERTEX> before;
        if(edit->flags&GRPEDIT_VTXMOD)before.assign(data->Vtx,data->Vtx+data->nVtx);
        const int result=owned->EditGroup(group,edit);
        if(!result) {
            if((flags^data->UsrFlag)&2)owner->dirty=true;
            // Device edits change the unanimated group only. Preserve topology,
            // other groups, and their cached animation poses.
            if(!before.empty() && std::memcmp(before.data(),data->Vtx,before.size()*sizeof(NTVERTEX))) {
                for(size_t g=0;g<owner->render.animatedRanges.size();++g) {
                    auto &range=owner->render.animatedRanges[g];
                    if(range.mesh!=owned || owner->render.animatedGroups[g].group!=group)continue;
                    for(size_t i=0;i<range.count;++i) {
                        auto &vertex=owner->render.baseVertices[range.start+i];const auto &v=data->Vtx[i];
                        vertex.position[0]=float(v.x+range.offset.x);vertex.position[1]=float(v.y+range.offset.y);vertex.position[2]=float(v.z+range.offset.z);
                        vertex.normal[0]=v.nx;vertex.normal[1]=v.ny;vertex.normal[2]=v.nz;
                        vertex.uv[0]=v.tu;vertex.uv[1]=v.tv;
                    }
                    range.edited=true;
                }
            }
        }
        return result;
    }
    bool clbkSetMeshTexture(DEVMESHHANDLE mesh,DWORD index,SURFHANDLE texture) override {
        auto owner=MeshOwner(mesh);auto owned=owner?reinterpret_cast<Mesh *>(mesh):nullptr;if(!owned || !index || !Find(texture))return false;
        clbkIncrSurfaceRef(texture);
        if(!owned->SetTexture(index-1,texture)) {clbkReleaseSurface(texture);return false;}
        owner->dirty=true;return true;
    }
    int clbkSetMeshMaterial(DEVMESHHANDLE mesh,DWORD index,const MATERIAL *material) override {
        auto owner=MeshOwner(mesh);auto owned=owner?reinterpret_cast<Mesh *>(mesh):nullptr;if(!owned || !material)return 3;
        auto destination=owned->GetMaterial(index);if(!destination)return 4;
        static_assert(sizeof(MATERIAL)==sizeof(D3DMATERIAL7));
        if(std::memcmp(destination,material,sizeof(MATERIAL))) {
            std::memcpy(destination,material,sizeof(MATERIAL));owner->dirty=true;
        }
        return 0;
    }
    int clbkMeshMaterial(DEVMESHHANDLE mesh,DWORD index,MATERIAL *material) override {
        auto owner=MeshOwner(mesh);auto owned=owner?reinterpret_cast<Mesh *>(mesh):nullptr;if(!owned || !material)return 3;
        auto source=owned->GetMaterial(index);if(!source)return 4;
        std::memcpy(material,source,sizeof(MATERIAL));return 0;
    }
    int clbkSetMeshMaterialEx(DEVMESHHANDLE mesh,DWORD index,MatProp property,const oapi::FVECTOR4 *value) override {
        auto owner=MeshOwner(mesh);auto owned=owner?reinterpret_cast<Mesh *>(mesh):nullptr;if(!owned || !value)return 3;
        auto material=owned->GetMaterial(index);if(!material)return 4;
        if(!std::isfinite(value->r)||!std::isfinite(value->g)||!std::isfinite(value->b)||!std::isfinite(value->a))return 2;
        if(property==Diffuse || property==Specular) {
            MATERIAL copy;std::memcpy(&copy,material,sizeof(copy));
            if(property==Diffuse)copy.diffuse={value->r,value->g,value->b,value->a};
            else {copy.specular.r=value->r;copy.specular.g=value->g;copy.specular.b=value->b;copy.power=value->a;}
            return clbkSetMeshMaterial(mesh,index,&copy);
        }
        if(property!=Light && property!=Emission)return 2;
        auto lighting=MeshLighting(owned,index);auto &component=property==Light?lighting.light:lighting.emission;
        if(std::memcmp(&component,value,sizeof(*value))) {
            component=*value;owner->lighting[owned][index]=lighting;owner->dirtyMaterials[owned].insert(index);
        }
        return 0;
    }
    int clbkMeshMaterialEx(DEVMESHHANDLE mesh,DWORD index,MatProp property,oapi::FVECTOR4 *value) override {
        auto owner=MeshOwner(mesh);auto owned=owner?reinterpret_cast<Mesh *>(mesh):nullptr;if(!owned || !value)return 3;
        auto material=owned->GetMaterial(index);if(!material)return 4;
        if(property==Diffuse) {*value={material->diffuse.r,material->diffuse.g,material->diffuse.b,material->diffuse.a};return 0;}
        if(property==Specular) {*value={material->specular.r,material->specular.g,material->specular.b,material->power};return 0;}
        const auto lighting=MeshLighting(owned,index);
        if(property==Light) {*value=lighting.light;return 0;}
        if(property==Emission) {*value=lighting.emission;return 0;}
        return 2;
    }
    int clbkVisEvent(OBJHANDLE object,VISHANDLE,DWORD message,DWORD_PTR context) override {
        auto found=visuals_.find(object);if(found==visuals_.end())return 0;
        auto visual_=found->second.get();
        if(message==EVENT_VESSEL_MESHVISMODE && context<visual_->visibility.size()) {
            auto &previous=visual_->visibility[context];
            const WORD current=oapiGetVesselInterface(object)->GetMeshVisibilityMode(UINT(context));
            const auto mask=visual_->cockpit?(MESHVIS_VC|MESHVIS_COCKPIT):MESHVIS_EXTERNAL;
            const bool changed=bool(previous&mask)!=bool(current&mask) ||
                (visual_->cockpit && bool(previous&MESHVIS_EXTERNAL)!=bool(current&MESHVIS_EXTERNAL));
            previous=current;if(!changed)return 1;
        }
        if(message==EVENT_VESSEL_INSMESH || message==EVENT_VESSEL_DELMESH || message==EVENT_VESSEL_MODMESHGROUP)visual_->reset=true;
        visual_->dirty=true;return 1;
    }
    oapi::Font *clbkCreateFont(int height,bool proportional,const char *face,FontStyle style,int orientation) const override {
        try {return new orbiter::graphics::NativeFont(height,proportional,face,style,orientation);}
        catch(const std::exception &error) {oapiWriteLog(const_cast<char *>(error.what()));return nullptr;}
    }
    void clbkReleaseFont(oapi::Font *font) const override {delete font;}
    oapi::Pen *clbkCreatePen(int style,int width,DWORD color) const override {
        return new orbiter::graphics::NativePen(style,width,color);
    }
    void clbkReleasePen(oapi::Pen *pen) const override {delete pen;}
    oapi::Brush *clbkCreateBrush(DWORD color) const override {
        return new orbiter::graphics::NativeBrush(color);
    }
    void clbkReleaseBrush(oapi::Brush *brush) const override {delete brush;}
    oapi::Sketchpad *clbkGetSketchpad(SURFHANDLE surface) override {
        if (!surface) surface=output_; // SDK null target denotes the render window.
        if(!Writable(Find(surface)))return nullptr;
        clbkIncrSurfaceRef(surface);
        return new orbiter::graphics::NativeSketchpad(*this,surface);
    }
    void clbkReleaseSketchpad(oapi::Sketchpad *sketchpad) override {
        if(!sketchpad)return;
        auto surface=sketchpad->GetSurface();delete sketchpad;clbkReleaseSurface(surface);
    }
    bool clbkUseLaunchpadVideoTab() const override { return false; }
    bool clbkFullscreenMode() const override { return false; }
    void clbkGetViewportSize(DWORD *w, DWORD *h) const override { *w=g_pOrbiter->ViewW(); *h=g_pOrbiter->ViewH(); }
    bool clbkGetRenderParam(DWORD, DWORD *) const override { return false; }
    VECTOR3 SunDirection(const VECTOR3 &position) const {
        for(DWORD i=0;i<oapiGetGbodyCount();++i) {
            auto star=oapiGetGbodyByIndex(i);if(oapiGetObjectType(star)!=OBJTP_STAR)continue;
            VECTOR3 sun{};oapiGetGlobalPos(star,&sun);const auto direction=sun-position;
            const double distance=length(direction);
            if(std::isfinite(distance) && distance>0)return direction/distance;
        }
        return _V(0,0,0);
    }
    bool particlesVisible_=true;
    void AppendExhausts(std::vector<OrbMeshDraw> &draws) {
        if(!g_camera)return;
        std::unordered_set<OBJHANDLE> active;
        if(exhaustVisible_)for(const auto &[object,visual]:visuals_) {
            const auto vessel=oapiGetVesselInterface(object);
            VECTOR3 eye{},position{};MATRIX3 rotation{};
            oapiCameraGlobalPos(&eye);vessel->GetGlobalPos(position);vessel->GetRotationMatrix(rotation);
            eye=tmul(rotation,eye-position);
            std::vector<OrbMeshVertex> vertices;
            std::vector<uint32_t> indices;
            std::vector<OrbMeshGroup> groups;
            for(DWORD i=0;i<vessel->GetExhaustCount();++i) {
                const double level=vessel->GetExhaustLevel(i);
                if(!(level>0))continue;
                EXHAUSTSPEC spec{};
                if(!vessel->GetExhaustSpec(i,&spec))continue;
                auto texture=Find(spec.tex);
                if(!texture) {
                    if(!defaultExhaust_)defaultExhaust_=clbkLoadSurface("Exhaust.dds",0,false);
                    texture=Find(defaultExhaust_);
                }
                if(!texture)continue;
                // Deterministic visual flicker never consumes simulation RNG state.
                const double flicker=0.5+0.5*std::sin(oapiGetSimTime()*83.0+i*2.399963);
                const float alpha=float(std::clamp(level,0.0,1.0)*(1-std::clamp(spec.modulate,0.0,1.0)*flicker));
                std::array<OrbMeshVertex,8> flame;
                if(!ExhaustGeometry(spec,eye,alpha,flame))continue;
                const auto base=uint32_t(vertices.size());
                for(unsigned quad=0;quad<2;++quad) {
                    OrbMeshGroup group{uint32_t(indices.size()+quad*6),6,texture->id,0,1,1,0};
                    for(unsigned vertex=quad*4;vertex<quad*4+4;++vertex)
                        for(unsigned axis=0;axis<3;++axis)group.sort_center[axis]+=flame[vertex].position[axis]*0.25f;
                    groups.push_back(group);
                }
                vertices.insert(vertices.end(),flame.begin(),flame.end());
                for(auto index:{0u,1u,2u,3u,2u,1u,4u,5u,6u,7u,6u,5u})indices.push_back(base+index);
            }
            if(vertices.empty())continue;
            active.insert(object);auto &batch=exhaustMeshes_[object];
            if(batch.vertices!=vertices.size()) {
                if(batch.mesh)orb_gpu_mesh_release(gpu_,batch.mesh);
                batch.mesh=orb_gpu_mesh_create(gpu_,vertices.data(),vertices.size(),indices.data(),indices.size());
                batch.vertices=vertices.size();
                if(!batch.mesh)throw std::runtime_error(orb_gpu_error());
            } else if(!orb_gpu_mesh_update(gpu_,batch.mesh,vertices.data(),vertices.size()))throw std::runtime_error(orb_gpu_error());
            if(!orb_gpu_mesh_groups(gpu_,batch.mesh,groups.data(),groups.size()))throw std::runtime_error(orb_gpu_error());
            OrbMeshDraw draw{};draw.mesh=batch.mesh;
            std::copy(visual->render.mvp,visual->render.mvp+16,draw.mvp);
            draw.sunlight[3]=1;draws.push_back(draw);
        }
        for(auto it=exhaustMeshes_.begin();it!=exhaustMeshes_.end();) {
            if(active.count(it->first)){++it;continue;}
            if(it->second.mesh)orb_gpu_mesh_release(gpu_,it->second.mesh);
            it=exhaustMeshes_.erase(it);
        }
    }
    void PrepareParticles() {
        if(!particlesVisible_ || !g_pOrbiter->Cfg()->CfgVisualPrm.bParticleStreams ||
           streams_.empty() || !g_camera || !g_camera->Target()) {
            orb_gpu_particles_upload(gpu_,nullptr,0);return;
        }
        std::vector<OrbParticle> batch;
        std::vector<orbiter::graphics::ParticleOccluder> occluders;
        VECTOR3 sun{};
        for(DWORD i=0;i<oapiGetGbodyCount();++i) {
            auto body=oapiGetGbodyByIndex(i);VECTOR3 position{};oapiGetGlobalPos(body,&position);
            if(oapiGetObjectType(body)==OBJTP_STAR)sun=position;
            else if(oapiGetObjectType(body)==OBJTP_PLANET)occluders.push_back({position,oapiGetSize(body)});
        }
        VECTOR3 eye{};MATRIX3 camera{};
        oapiCameraGlobalPos(&eye);oapiCameraRotationMatrix(&camera);
        const double fy=1/std::tan(oapiCameraAperture());
        const double fx=fy*double(g_pOrbiter->ViewH())/g_pOrbiter->ViewW();
        for(const auto &stream:streams_)for(const auto &p:stream->Particles()) {
            if(batch.size()>=1000000)break;
            const auto view=tmul(camera,p.position-eye);
            if(!(view.z>0.01) || !std::isfinite(view.z) || !(p.size>0))continue;
            const double radius=p.size; // Orbiter particle size is the billboard half-extent.
            if(std::abs(view.x*fx)>view.z+radius*fx || std::abs(view.y*fy)>view.z+radius*fy)continue;
            const float alpha=float(std::clamp(p.opacity*(1-p.age/p.lifetime),0.0,1.0));
            if(alpha<=0)continue;
            const float shade=orbiter::graphics::ParticleBrightness(stream->Emissive(),p.position,sun,occluders);
            OrbParticle record{{float(view.x*fx),float(view.y*fy),0.01f,float(view.z)},
                {float(radius*fx),float(radius*fy)},{shade,shade,shade,alpha}};
            if(auto texture=Find(stream->Texture())) {record.texture=texture->id;record.sprite=p.sprite;}
            batch.push_back(record);
            if(batch.size()==1000000)break;
        }
        std::stable_sort(batch.begin(),batch.end(),[](const auto &a,const auto &b){return a.center[3]>b.center[3];});
        if(!orb_gpu_particles_upload(gpu_,batch.data(),batch.size()))throw std::runtime_error(orb_gpu_error());
    }
    void PrepareAtmosphere() {
        char name[]="Earth";auto earth=oapiGetGbodyByName(name);
        if(!g_camera || !g_camera->Target() || !earth) {orb_gpu_atmosphere(gpu_,nullptr);return;}
        VECTOR3 eye{},center{};MATRIX3 camera{};
        oapiCameraGlobalPos(&eye);oapiGetGlobalPos(earth,&center);oapiCameraRotationMatrix(&camera);
        const auto relative=(eye-center)/1000.0;
        // Earth Rayleigh profile. Other atmospheres need their own coefficients.
        const double radius=oapiGetSize(earth)/1000.0;
        // Avoid loss of precision for a sub-pixel Earth from remote viewpoints.
        if(length(relative)>radius*100) {orb_gpu_atmosphere(gpu_,nullptr);return;}
        const double vertical=std::tan(oapiCameraAperture());
        const double horizontal=vertical*double(g_pOrbiter->ViewW())/g_pOrbiter->ViewH();
        float sky[20]{};const auto sun=SunDirection(center);
        for(int k=0;k<3;++k) {
            sky[k]=float(camera.data[k*3]*horizontal);
            sky[4+k]=float(camera.data[k*3+1]*vertical);
            sky[8+k]=float(camera.data[k*3+2]);
            sky[12+k]=float(relative.data[k]);sky[16+k]=float(sun.data[k]);
        }
        sky[15]=float(radius);sky[19]=float(radius+100);
        sky[3]=g_pOrbiter->Cfg()->CfgVisualPrm.bHaze?0.0f:1.0f;
        sky[7]=g_pOrbiter->Cfg()->CfgVisualPrm.bFog?0.0f:1.0f;
        if(!orb_gpu_atmosphere(gpu_,sky))throw std::runtime_error(orb_gpu_error());
    }
    void PrepareBackground() {
        const auto &settings=g_pOrbiter->Cfg()->CfgVisualPrm;
        if(!g_camera || !g_camera->Target()) {orb_gpu_background(gpu_,0,nullptr);orb_gpu_star_image(gpu_,0,nullptr);return;}
        // Orbiter's legacy celestial images use its galactic coordinate frame.
        const double lambda=173.60*RAD,phi=90.03*RAD,theta=60.19*RAD;
        const auto galactic=mul(_M(cos(lambda),0,sin(lambda),0,1,0,-sin(lambda),0,cos(lambda)),
            mul(_M(1,0,0,0,cos(theta),sin(theta),0,-sin(theta),cos(theta)),
                _M(cos(phi),0,sin(phi),0,1,0,-sin(phi),0,cos(phi))));
        MATRIX3 camera{};oapiCameraRotationMatrix(&camera);camera=mul(galactic,camera);
        const double vertical=std::tan(oapiCameraAperture()),horizontal=vertical*double(g_pOrbiter->ViewW())/g_pOrbiter->ViewH();
        float values[16]{};
        for(int k=0;k<3;++k) {values[k]=float(camera.data[k*3]*horizontal);values[4+k]=float(camera.data[k*3+1]*vertical);values[8+k]=float(camera.data[k*3+2]);}
        auto layer=[&](bool enabled,const char *path,double intensity,uint64_t &texture,unsigned &level,std::string &loaded,auto upload) {
            if(!enabled) {upload(gpu_,0,nullptr);return;}
            const std::string name=std::string(path)+".tex";
            if(loaded!=name) {
                if(texture)orb_gpu_surface_release(gpu_,texture);
                texture=0;loaded=name;
                try {
                    char resolved[256];g_pOrbiter->Cfg()->TexPath(resolved,name.c_str());
                    std::ifstream input(orbiter::platform::ResolvePath(resolved),std::ios::binary);
                    const auto sky=ReadSkyTexture(input);level=sky.level;
                    texture=orb_gpu_dds_create(gpu_,sky.dds.data(),sky.dds.size());
                    if(!texture)throw std::runtime_error(orb_gpu_error());
                }catch(const std::exception &error) {oapiWriteLog(const_cast<char *>(error.what()));}
            }
            values[12]=float(std::clamp(intensity,0.0,1.0));values[13]=float(level);
            if(!upload(gpu_,texture,values))throw std::runtime_error(orb_gpu_error());
        };
        layer(settings.bUseBgImage,settings.CSphereBgPath,settings.CSphereBgIntens,skyTexture_,skyLevel_,skyPath_,orb_gpu_background);
        layer(settings.bUseStarImage,settings.StarImagePath,1,starImage_,starImageLevel_,starImagePath_,orb_gpu_star_image);
    }
    void PrepareStars() {
        if(!g_camera || !liveVessel_)return;
        if(!g_pOrbiter->Cfg()->CfgVisualPrm.bUseStarDots) {
            if(starsLoaded_) {
                if(!orb_gpu_stars_upload(gpu_,nullptr,0))throw std::runtime_error(orb_gpu_error());
                starsLoaded_=false;starCount_=0;
            }
            return;
        }
        if(!starsLoaded_) {
            starsLoaded_=true;
            try {
                const auto path=orbiter::platform::ResolvePath(std::string(g_pOrbiter->Cfg()->CfgDirPrm.ConfigDir)+"CSphere/Data/star.bin");
                std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Star catalog not found: "+path.string());
                const auto stars=orbiter::graphics::LoadStarCatalog(input,g_pOrbiter->Cfg()->CfgVisualPrm.StarPrm);
                if(!orb_gpu_stars_upload(gpu_,stars.data(),stars.size()))throw std::runtime_error(orb_gpu_error());
                starCount_=stars.size();
            }catch(const std::exception &error) {oapiWriteLog(const_cast<char *>(error.what()));}
        }
        MATRIX3 camera{};oapiCameraRotationMatrix(&camera);
        const double f=1/std::tan(oapiCameraAperture());
        const double scale[]={f*double(g_pOrbiter->ViewH())/g_pOrbiter->ViewW(),f,1};
        float matrix[16]{};
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)
            matrix[col*4+(row==2?3:row)]=float(scale[row]*camera.data[col*3+row]);
        if(!orb_gpu_stars_camera(gpu_,matrix))throw std::runtime_error(orb_gpu_error());
    }
    void ExteriorLights(OrbMeshDraw &draw,const VECTOR3 &position,const MATRIX3 &rotation,
                        double radius,const VECTOR3 &origin=_V(0,0,0)) {
        size_t count=0;
        for(DWORD index=0;index<oapiGetVesselCount() && count<ORB_MAX_LOCAL_LIGHTS;++index) {
            auto vessel=oapiGetVesselInterface(oapiGetVesselByIndex(index));
            VECTOR3 sourcePosition{};MATRIX3 sourceRotation{};
            vessel->GetGlobalPos(sourcePosition);vessel->GetRotationMatrix(sourceRotation);
            for(DWORD i=0;i<vessel->LightEmitterCount() && count<ORB_MAX_LOCAL_LIGHTS;++i) {
                auto source=vessel->GetLightEmitter(i);OrbLocalLight candidate{};
                if(source->IsCameraRelative() || !CockpitLight(*source,candidate,_V(0,0,0),_M(1,0,0,0,1,0,0,0,1),LightEmitter::VIS_EXTERNAL))continue;
                // Subtract astronomical positions before rotating, and the tile
                // origin before narrowing to floats. Terrain uses local vertices.
                const auto localPosition=tmul(rotation,(sourcePosition-position)+mul(sourceRotation,source->GetPosition()))-origin;
                if(length(localPosition)>candidate.position[3]+radius)continue;
                const auto direction=tmul(rotation,mul(sourceRotation,_V(candidate.direction[0],candidate.direction[1],candidate.direction[2])));
                for(int k=0;k<3;++k) {candidate.position[k]=float(localPosition.data[k]);candidate.direction[k]=float(direction.data[k]);}
                draw.lights[count++]=candidate;
            }
        }
    }
    void AppendPlanets(std::vector<OrbMeshDraw> &draws) {
        if(!g_camera || !liveVessel_)return;
        VECTOR3 eye{};oapiCameraGlobalPos(&eye);
        MATRIX3 camera{};oapiCameraRotationMatrix(&camera);
        const double f=1/std::tan(oapiCameraAperture());
        const double scale[3]={f*double(g_pOrbiter->ViewH())/g_pOrbiter->ViewW(),f,1};
        for(DWORD i=0;i<oapiGetGbodyCount();++i) {
            auto body=oapiGetGbodyByIndex(i);if(oapiGetObjectType(body)!=OBJTP_PLANET)continue;
            const double bodyRadius=oapiGetSize(body);if(!(bodyRadius>0) || !std::isfinite(bodyRadius))continue;
            double cloudAltitude=0;
            const bool hasCloud=reinterpret_cast<Planet *>(body)->CloudParam(cloudAltitude) && cloudAltitude>0;
            for(bool cloud:{false,true}) {
            if(cloud && (!hasCloud || !g_pOrbiter->Cfg()->CfgVisualPrm.bClouds))continue;
            const double radius=bodyRadius+(cloud?cloudAltitude:0);
            VECTOR3 position{};oapiGetGlobalPos(body,&position);
            MATRIX3 rotation{};oapiGetRotationMatrix(body,&rotation);
            if(cloud) {
                const double angle=reinterpret_cast<Planet *>(body)->CloudRotationAngle();
                const double c=std::cos(angle),s=std::sin(angle);
                rotation=mul(rotation,_M(c,0,s,0,1,0,-s,0,c));
            }
            const auto localEye=tmul(rotation,eye-position);
            const auto selected=orbiter::terrain::SelectTerrainTiles({localEye.x,localEye.y,localEye.z},radius,f*g_pOrbiter->ViewH()/2,16,cloud?9:18,cloud?128:512);
            auto &cache=(cloud?cloudMeshes_:planetMeshes_)[body];
            std::unordered_set<uint64_t> used;
            char name[256]{};oapiGetObjectName(body,name,sizeof(name));
            const bool earth=std::strcmp(name,"Earth")==0;
            auto &trees=cloud?cloudTrees_:surfaceTrees_;
            auto tree=trees.find(body);
            if(tree==trees.end()) {
                char path[256];g_pOrbiter->Cfg()->PTexPath(path,name);
                std::unique_ptr<ZTreeMgr> reader;
                if(g_pOrbiter->Cfg()->CfgPRenderPrm.TileLoadFlags&2)reader.reset(ZTreeMgr::CreateFromFile(path,cloud?ZTreeMgr::LAYER_CLOUD:ZTreeMgr::LAYER_SURF));
                tree=trees.emplace(body,std::move(reader)).first;
            }
            auto *manager=cloud?nullptr:reinterpret_cast<Planet *>(body)->ElevMgr();
            std::vector<ElevationTile> elevationCache(16);
            const auto sun=tmul(rotation,SunDirection(position));
            for(const auto &address:selected) {
                // Standard Earth/Moon relief lies within +/-32 km. Keep other
                // bodies until their configured elevation bounds are available.
                if((earth || std::strcmp(name,"Moon")==0) &&
                   !orbiter::terrain::TerrainTileAboveHorizon(address,
                       {localEye.x,localEye.y,localEye.z},bodyRadius,32000+(cloud?cloudAltitude:0)))continue;
                const uint64_t key=(uint64_t(address.level)<<56)|(uint64_t(address.latitude)<<28)|address.longitude;
                used.insert(key);
                auto &tile=cache[key];
                if(!tile.mesh) {
                    std::function<double(double,double)> height;
                    if(manager && manager->HasSources() && !std::getenv("ORBITER_DIAGNOSTIC_FLAT_TERRAIN"))height=[&](double lon,double lat) {
                        lat=std::clamp(lat,-PI/2+1e-10,PI/2-1e-10);
                        lon=std::remainder(lon,2*PI);
                        if(lon>=PI)lon=-PI;
                        return manager->Elevation(lat,lon,address.level+3,&elevationCache);
                    };
                    const double span=std::ldexp(PI,4-int(address.level));
                    const double skirt=cloud?0:std::min(radius*0.1,std::max(10.0,radius*span*span/32));
                    const auto geometry=orbiter::terrain::BuildTileGeometry(address.level,address.latitude,address.longitude,radius,nullptr,32,skirt,height);
                    tile.origin=geometry.origin;
                    std::array<float,3> textureUV{};
                    tile.texture=LoadTerrainTexture(body,name,tree->second.get(),address,textureUV,cloud);
                    if(cloud && !tile.texture)continue;
                    if(const char *trace=std::getenv("ORBITER_TERRAIN_TRACE")) {
                        std::ofstream out(trace,std::ios::app);
                        out.precision(17);
                        out<<name<<','<<cloud<<','<<address.level<<','<<address.latitude<<','<<address.longitude;
                        const auto source=tile.texture?tile.texture->source:orbiter::terrain::TileAddress{};
                        out<<','<<source.level<<','<<source.latitude<<','<<source.longitude;
                        for(auto uv:textureUV)out<<','<<uv;
                        // Recover world-space corner positions without narrowing the origin.
                        for(unsigned i:{0u,32u,1056u,1088u})for(unsigned k=0;k<3;++k)
                            out<<','<<geometry.origin[k]+double(geometry.vertices[i].position[k]);
                        out<<'\n';
                    }

                    std::vector<OrbMeshVertex> vertices;vertices.reserve(geometry.vertices.size());
                    for(const auto &source:geometry.vertices) {
                        OrbMeshVertex vertex{};
                        std::copy(source.position.begin(),source.position.end(),vertex.position);
                        std::copy(source.normal.begin(),source.normal.end(),vertex.normal);
                        vertex.uv[0]=textureUV[0]+source.uv[0]*textureUV[2];
                        vertex.uv[1]=textureUV[1]+source.uv[1]*textureUV[2];
                        vertex.color[0]=earth?0.10f:0.45f;vertex.color[1]=earth?0.28f:0.45f;vertex.color[2]=earth?0.65f:0.45f;vertex.color[3]=1;
                        if(tile.texture)vertex.color[0]=vertex.color[1]=vertex.color[2]=1;
                        vertices.push_back(vertex);
                    }
                    tile.mesh=orb_gpu_mesh_create(gpu_,vertices.data(),vertices.size(),geometry.indices.data(),geometry.indices.size());
                    if(!tile.mesh)throw std::runtime_error(orb_gpu_error());
                    if(tile.texture) {
                        const OrbMeshGroup group{0,static_cast<uint32_t>(geometry.indices.size()),tile.texture->id,0,1,cloud?1u:0u};
                        if(!orb_gpu_mesh_groups(gpu_,tile.mesh,&group,1))throw std::runtime_error(orb_gpu_error());
                    }
                }
                // Keep body rotation, astronomical subtraction and local origins
                // in double precision until the final camera-relative transform.
                const auto relative=(position-eye)+mul(rotation,_V(tile.origin[0],tile.origin[1],tile.origin[2]));
                OrbMeshDraw draw{};draw.mesh=tile.mesh;
                for(int k=0;k<3;++k)draw.sunlight[k]=float(sun.data[k]);
                draw.sunlight[3]=0.03f;
                if(earth)draw.sunlight[3]=orbiter::graphics::EarthSkyAmbient(
                    position+mul(rotation,_V(tile.origin[0],tile.origin[1],tile.origin[2])),
                    position,bodyRadius,SunDirection(position));
                ExteriorLights(draw,position,rotation,radius*std::ldexp(PI,4-int(address.level))*2+20000,
                               _V(tile.origin[0],tile.origin[1],tile.origin[2]));
                for(int row=0;row<3;++row) {
                    const int dest=row==2?3:row;
                    double translation=0;
                    for(int col=0;col<3;++col) {
                        double basis=0;
                        for(int k=0;k<3;++k)basis+=camera.data[k*3+row]*rotation.data[k*3+col];
                        draw.mvp[col*4+dest]=float(scale[row]*basis);
                        translation+=camera.data[col*3+row]*relative.data[col];
                    }
                    draw.mvp[12+dest]=float(scale[row]*translation);
                }
                // Diagnostic capture mode separates texture/geometry seams from lighting.
                if(!cloud && std::getenv("ORBITER_DIAGNOSTIC_UNLIT_TERRAIN")) {
                    draw.sunlight[3]=1;
                    for(auto &light:draw.lights)light={};
                }
                draw.mvp[14]=0.01f;draws.push_back(draw);
            }
            for(auto it=cache.begin();it!=cache.end();) {
                if(!used.count(it->first)){ReleaseTerrainMesh(it->second);it=cache.erase(it);}else ++it;
            }
            auto &textures=(cloud?cloudTextures_:terrainTextures_)[body];
            for(auto it=textures.begin();it!=textures.end();) {
                if(it->second.expired())it=textures.erase(it);else ++it;
            }
            }
        }
    }
    void clbkRenderScene() override {
        const bool liveCockpit=g_camera && oapiCameraInternal() && oapiCockpitMode()==COCKPIT_VIRTUAL;
        if(g_camera && g_camera->Target()) {
            liveVessel_=oapiCameraTarget();
            for(DWORD i=0;i<oapiGetVesselCount();++i) {
                auto object=oapiGetVesselByIndex(i);if(object!=liveVessel_)UpdateVesselVisual(object,false);
            }
            if(liveVessel_ && oapiGetObjectType(liveVessel_)==OBJTP_VESSEL)UpdateVesselVisual(liveVessel_,liveCockpit);
        }
        PrepareStars();
        PrepareBackground();
        PrepareAtmosphere();
        PrepareParticles();
        if (!FlushAll()) throw std::runtime_error(orb_gpu_error());
        if (render_->mesh || liveVessel_) {
            auto target=Find(output_);
            std::vector<OrbMeshDraw> draws;AppendPlanets(draws);
            std::vector<orbiter::graphics::ParticleOccluder> shadowBodies;
            VECTOR3 sunPosition{};
            for(DWORD i=0;i<oapiGetGbodyCount();++i) {
                const auto body=oapiGetGbodyByIndex(i);VECTOR3 position{};oapiGetGlobalPos(body,&position);
                if(oapiGetObjectType(body)==OBJTP_STAR)sunPosition=position;
                else if(oapiGetObjectType(body)==OBJTP_PLANET)shadowBodies.push_back({position,oapiGetSize(body)});
            }
            for(const auto &[object,visual]:visuals_) {
                if((liveCockpit && object==liveVessel_) || !visual->render.mesh)continue;
                OrbMeshDraw external{};external.mesh=visual->render.mesh;
                std::copy(visual->render.mvp,visual->render.mvp+16,external.mvp);
                VECTOR3 position{};MATRIX3 rotation{};auto vessel=oapiGetVesselInterface(object);
                vessel->GetGlobalPos(position);vessel->GetRotationMatrix(rotation);
                VECTOR3 globalEye{};oapiCameraGlobalPos(&globalEye);
                const auto localEye=tmul(rotation,globalEye-position);
                for(unsigned k=0;k<3;++k)external.eye[k]=float(localEye.data[k]);
                external.eye[3]=g_pOrbiter->Cfg()->CfgVisualPrm.bSpecular?1.f:0.f;
                ExteriorLights(external,position,rotation,vessel->GetSize());
                const auto sun=orbiter::graphics::SunVisible(position,sunPosition,shadowBodies)
                    ?tmul(rotation,SunDirection(position)):_V(0,0,0);
                for(int k=0;k<3;++k)external.sunlight[k]=float(sun.data[k]);
                external.sunlight[3]=0.03f;
                char earthName[]="Earth";
                if(auto earth=oapiGetGbodyByName(earthName)) {
                    VECTOR3 center{};oapiGetGlobalPos(earth,&center);
                    external.sunlight[3]=orbiter::graphics::EarthSkyAmbient(
                        position,center,oapiGetSize(earth),SunDirection(position));
                }
                draws.push_back(external);
            }
            if(render_->mesh && (liveCockpit || !liveVessel_)) {
                OrbMeshDraw cockpit{};cockpit.mesh=render_->mesh;std::copy(render_->mvp,render_->mvp+16,cockpit.mvp);
                VECTOR3 light=_V(0.2,0.5,-1);
                float ambient=0.35f;
                if(liveVessel_) {
                    VECTOR3 position{};oapiGetGlobalPos(liveVessel_,&position);
                    MATRIX3 rotation{};oapiGetVesselInterface(liveVessel_)->GetRotationMatrix(rotation);
                    light=tmul(rotation,SunDirection(position));
                    if(!orbiter::graphics::SunVisible(position,sunPosition,shadowBodies)) {
                        light=_V(0,0,0);ambient=0.03f;
                    }
                    auto vessel=oapiGetVesselInterface(liveVessel_);
                    const auto eye=g_camera->CockpitPos();const auto view=g_camera->CockpitRotation();
                    const VECTOR3 cameraPosition=_V(eye.x,eye.y,eye.z);
                    for(unsigned k=0;k<3;++k)cockpit.eye[k]=float(cameraPosition.data[k]);
                    cockpit.eye[3]=g_pOrbiter->Cfg()->CfgVisualPrm.bSpecular?1.f:0.f;
                    MATRIX3 cameraRotation{};std::copy(view.data,view.data+9,cameraRotation.data);
                    size_t count=0;
                    for(DWORD i=0;i<vessel->LightEmitterCount() && count<ORB_MAX_LOCAL_LIGHTS;++i) {
                        OrbLocalLight candidate{};
                        if(CockpitLight(*vessel->GetLightEmitter(i),candidate,cameraPosition,cameraRotation))cockpit.lights[count++]=candidate;
                    }
                }
                for(int k=0;k<3;++k)cockpit.sunlight[k]=float(light.data[k]);
                cockpit.sunlight[3]=ambient;draws.push_back(cockpit);
            }
            if((g_pOrbiter->Cfg()->CfgVisualPrm.bShadows || g_pOrbiter->Cfg()->CfgVisualPrm.bVesselShadows) &&
               liveVessel_ && oapiGetObjectType(liveVessel_)==OBJTP_VESSEL && g_camera) {
                VECTOR3 eye{},focus{};MATRIX3 camera{};
                oapiCameraGlobalPos(&eye);oapiGetGlobalPos(liveVessel_,&focus);oapiCameraRotationMatrix(&camera);
                const auto globalSun=SunDirection(focus);
                const auto sun=tmul(camera,globalSun);
                // Choose the light-map basis in the global frame. Choosing it
                // in camera space rotates the shadow texel grid during mouse-look.
                const auto up=tmul(camera,std::abs(globalSun.y)<0.9?_V(0,1,0):_V(1,0,0));
                const double extent=std::max(50.0,oapiGetVesselInterface(liveVessel_)->GetSize()*4);
                VECTOR3 anchor{};
                if(const auto body=oapiGetVesselInterface(liveVessel_)->GetSurfaceRef())oapiGetGlobalPos(body,&anchor);
                const auto correction=ShadowFocusCorrection(focus-anchor,globalSun,extent);
                const auto center=tmul(camera,(focus-eye)+correction);
                const double fy=1/std::tan(oapiCameraAperture()),fx=fy*double(g_pOrbiter->ViewH())/g_pOrbiter->ViewW();
                for(auto &draw:draws)FocusedShadow(draw,center,sun,extent,fx,fy,up);
            }
            AppendExhausts(draws);
            if (target && !draws.empty() && !orb_gpu_mesh_render_scene(gpu_,target->id,draws.data(),draws.size()))
                throw std::runtime_error(orb_gpu_error());
        }
    }
    void clbkImGuiNewFrame() override {clbkImGuiInit();}
    void clbkImGuiRenderDrawData() override {
        auto data=ImGui::GetDrawData();
        if(!data)return;
        if(data->Textures)for(auto texture:*data->Textures) {
            if(texture->Status==ImTextureStatus_WantCreate) {
                if(texture->Format!=ImTextureFormat_RGBA32)throw std::runtime_error("ImGui texture must be RGBA32");
                const auto surface=clbkCreateSurfaceEx(texture->Width,texture->Height,0);
                if(!surface)throw std::runtime_error("Cannot create ImGui texture");
                texture->BackendUserData=surface;texture->SetTexID(clbkImGuiSurfaceTexture(surface));
            }
            if(texture->Status==ImTextureStatus_WantCreate || texture->Status==ImTextureStatus_WantUpdates) {
                // Atlas changes are infrequent; upload once per dirty texture.
                if(!clbkUpdateSurface(texture->BackendUserData,static_cast<const BYTE *>(texture->GetPixels()),texture->GetPitch()))
                    throw std::runtime_error("Cannot upload ImGui texture");
                texture->SetStatus(ImTextureStatus_OK);
            }
            if(texture->Status==ImTextureStatus_WantDestroy) {
                if(texture->BackendUserData)clbkReleaseSurface(texture->BackendUserData);
                texture->BackendUserData=nullptr;texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
        if(auto target=Find(output_)) {
            if(!FlushAll())throw std::runtime_error(orb_gpu_error());
            orbiter::graphics::RenderImGuiDrawData(gpu_,target->id,target->width,target->height,data);
        }
    }
    void clbkImGuiInit() override {
        if(imguiInitialized_)return;
        IMGUI_CHECKVERSION();auto &io=ImGui::GetIO();
        if(io.BackendRendererUserData)throw std::runtime_error("ImGui renderer already initialized");
        io.BackendRendererUserData=this;io.BackendRendererName="orbiter_wgpu";
        io.BackendFlags|=ImGuiBackendFlags_RendererHasVtxOffset|ImGuiBackendFlags_RendererHasTextures;
        auto &platform=ImGui::GetPlatformIO();platform.Renderer_TextureMaxWidth=platform.Renderer_TextureMaxHeight=8192;
        imguiInitialized_=true;
    }
    void clbkImGuiShutdown() override {
        if(!imguiInitialized_)return;
        for(auto texture:ImGui::GetPlatformIO().Textures)if(texture->BackendUserData) {
            clbkReleaseSurface(texture->BackendUserData);texture->BackendUserData=nullptr;
            texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);
        }
        auto &io=ImGui::GetIO();io.BackendRendererUserData=nullptr;io.BackendRendererName=nullptr;
        io.BackendFlags&=~(ImGuiBackendFlags_RendererHasVtxOffset|ImGuiBackendFlags_RendererHasTextures);
        imguiInitialized_=false;
    }
    uint64_t clbkImGuiSurfaceTexture(SURFHANDLE handle) override {auto surface=Find(handle);return surface?surface->id:0;}
    SURFHANDLE clbkCreateSurfaceEx(DWORD width, DWORD height, DWORD) override {
        auto id = orb_gpu_surface_create(gpu_, width, height);
        if (!id) return nullptr;
        auto handle = reinterpret_cast<SURFHANDLE>(static_cast<uintptr_t>(id));
        surfaces_.emplace(handle, Surface{id, width, height});
        return handle;
    }
    SURFHANDLE clbkCreateSurface(DWORD w, DWORD h, SURFHANDLE) override { return clbkCreateSurfaceEx(w,h,0); }
    SURFHANDLE clbkCreateTexture(DWORD w, DWORD h) override { return clbkCreateSurfaceEx(w,h,0); }
    void clbkIncrSurfaceRef(SURFHANDLE handle) override {
        if (auto surface=Find(handle)) {
            if (surface->references == UINT_MAX) throw std::overflow_error("Surface reference count");
            ++surface->references;
        }
    }
    bool clbkReleaseSurface(SURFHANDLE handle) override {
        auto surface=Find(handle);
        if (!surface) return false;
        if (surface->references>1) { --surface->references; return true; }
        // Commands can still reference this source: submit them before release.
        if (!FlushAll()) return false;
        bool result = orb_gpu_surface_release(gpu_, surface->id) != 0;
        const auto emission=surface->emission;
        surfaces_.erase(handle);
        if(emission)clbkReleaseSurface(emission);
        return result;
    }
    void clbkReleaseTexture(SURFHANDLE handle) override { clbkReleaseSurface(handle); }
    bool clbkGetSurfaceSize(SURFHANDLE handle, DWORD *w, DWORD *h) override {
        auto surface=Find(handle);
        *w=surface ? surface->width : 0; *h=surface ? surface->height : 0;
        return surface != nullptr;
    }
    bool clbkUpdateSurface(SURFHANDLE handle, const BYTE *rgba, DWORD pitch) override {
        auto surface=Find(handle);
        if (!Writable(surface) || !FlushAll()) return false;
        return orb_gpu_surface_upload(gpu_,surface->id,rgba,size_t(pitch)*surface->height,pitch) != 0;
    }
    bool clbkFillSurface(SURFHANDLE handle, DWORD col) const override {
        auto surface=Find(handle);
        return surface && clbkFillSurface(handle,0,0,surface->width,surface->height,col);
    }
    bool clbkFillSurface(SURFHANDLE handle, DWORD x, DWORD y, DWORD w, DWORD h, DWORD col) const override {
        auto surface=Find(handle);
        if (!Writable(surface)) return false;
        surface->commands.push_back({++sequence_,0,int32_t(x),int32_t(y),w,h,
            0,0,0,0,col,0,0,0});
        return true;
    }
    bool clbkSetSurfaceColourKey(SURFHANDLE handle, DWORD key) override {
        auto surface=Find(handle);
        if (!surface) return false;
        surface->key=key&0xffffff;
        surface->hasKey=key!=0xffffffff;
        return true;
    }
    bool clbkBlt(SURFHANDLE target, DWORD x, DWORD y, SURFHANDLE source, DWORD flags) const override {
        auto src=Find(source);
        return src && clbkBlt(target,x,y,source,0,0,src->width,src->height,flags);
    }
    bool clbkBlt(SURFHANDLE target, DWORD x, DWORD y, SURFHANDLE source, DWORD sx, DWORD sy,
                 DWORD width, DWORD height, DWORD flags) const override {
        return clbkScaleBlt(target,x,y,width,height,source,sx,sy,width,height,flags);
    }
    bool clbkScaleBlt(SURFHANDLE target, DWORD x, DWORD y, DWORD width, DWORD height,
                     SURFHANDLE source, DWORD sx, DWORD sy, DWORD sw, DWORD sh, DWORD flags) const override {
        return clbkScaleBltColourKey(target,x,y,width,height,source,sx,sy,sw,sh,
            (flags&BLT_SRCCOLORKEY)?SURF_PREDEF_CK:SURF_NO_CK);
    }
    bool clbkScaleBltColourKey(SURFHANDLE target,DWORD x,DWORD y,DWORD width,DWORD height,
        SURFHANDLE source,DWORD sx,DWORD sy,DWORD sw,DWORD sh,DWORD key) const override {
        auto dst=Find(target ? target : output_);auto src=Find(source);
        if(!src || !Writable(dst))return false;
        if(!width || !height || !sw || !sh)return true;
        const bool keyed=key!=SURF_NO_CK && (key!=SURF_PREDEF_CK || src->hasKey);
        const DWORD selected=key==SURF_PREDEF_CK?src->key:key;
        dst->commands.push_back({++sequence_,src->id,int32_t(x),int32_t(y),width,height,
            int32_t(sx),int32_t(sy),sw,sh,0,selected&0xffffff,keyed?1u:0u,0});
        return true;
    }
    SURFHANDLE clbkLoadSurface(const char *name, DWORD attrib, bool fullpath) override {
        if (!name) return nullptr;
        auto path = fullpath ? orbiter::platform::ResolvePath(name) :
            orbiter::platform::ResolvePath(g_pOrbiter->Cfg()->TexPath(name, ""));
        std::ifstream input(path,std::ios::binary);
        char signature[4]{}; input.read(signature,4);
        if (input && std::string(signature,4)=="DDS ") {
            input.seekg(0,std::ios::end); const auto length=input.tellg();
            if (length<128 || length>512*1024*1024) return nullptr;
            std::vector<uint8_t> bytes(static_cast<size_t>(length));
            input.seekg(0);input.read(reinterpret_cast<char *>(bytes.data()),bytes.size());
            if (!input) return nullptr;
            const auto id=orb_gpu_dds_create(gpu_,bytes.data(),bytes.size());
            if (!id) throw std::runtime_error(path.string()+": "+orb_gpu_error());
            const auto word=[&](size_t at) {return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|(uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);};
            const auto handle=reinterpret_cast<SURFHANDLE>(static_cast<uintptr_t>(id));
            surfaces_.emplace(handle,Surface{id,word(16),word(12)});
            Find(handle)->writable=false;
            return handle;
        }
        int w,h,channels;
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(stbi_load(path.string().c_str(), &w,&h,&channels,4),stbi_image_free);
        if (!pixels) return nullptr;
        auto surface=clbkCreateSurfaceEx(w,h,attrib);
        if (surface && !clbkUpdateSurface(surface,pixels.get(),DWORD(w)*4)) {
            clbkReleaseSurface(surface); return nullptr;
        }
        return surface;
    }
    SURFHANDLE clbkLoadTexture(const char *name, DWORD flags) override {
        auto source=clbkLoadSurface(name,0,false);
        if(!source)return nullptr;
        if(flags&2) {
            // Mesh TEXTURES entries marked D need writable dynamic textures.
            auto info=Find(source);
            auto writable=clbkCreateSurfaceEx(info->width,info->height,0);
            const bool copied=writable && clbkBlt(writable,0,0,source,0) && FlushAll();
            clbkReleaseSurface(source);
            if(!copied) {if(writable)clbkReleaseSurface(writable);return nullptr;}
            source=writable;
        }
        // NASSP's integral lighting uses companion emission atlases. Ownership
        // follows the base surface, including writable panel textures.
        std::filesystem::path emissionName(name);
        emissionName.replace_filename(emissionName.stem().string()+"_emis"+emissionName.extension().string());
        const auto emission=clbkLoadSurface(emissionName.string().c_str(),0,false);
        Find(source)->emission=emission;
        return source;
    }

    void TerrainStats(size_t *meshes,size_t *textured,size_t *textures) const {
        *meshes=*textured=*textures=0;std::unordered_set<uint64_t> ids;
        for(const auto &[body,tiles]:planetMeshes_)for(const auto &[key,tile]:tiles) {
            ++*meshes;if(tile.texture){++*textured;ids.insert(tile.texture->id);}
        }
        *textures=ids.size();
    }
    void SetParticlesVisible(bool visible) {particlesVisible_=visible;}
    void SetExhaustVisible(bool visible) {exhaustVisible_=visible;}
    void ParticleStats(size_t *count,size_t *textured) const {
        *count=*textured=0;
        for(const auto &stream:streams_) {
            *count+=stream->Particles().size();
            if(Find(stream->Texture()))*textured+=stream->Particles().size();
        }
    }
    size_t StarCount() const {return starCount_;}
    uint64_t OutputMesh() const {return render_->mesh;}
    size_t ShadowOnlyGroups() const {return render_->shadowOnlyGroups;}
    bool ReadOutput(BYTE *pixels, size_t bytes, DWORD pitch) { return Read(output_,pixels,bytes,pitch); }
    bool Read(SURFHANDLE handle, BYTE *pixels, size_t bytes, DWORD pitch) {
        auto surface=Find(handle);
        return surface && FlushAll() && orb_gpu_surface_read(gpu_,surface->id,pixels,bytes,pitch);
    }
};
std::unique_ptr<WgpuClient> client;
}
DLLCLBK void InitModule(HINSTANCE instance) {
    auto candidate=std::make_unique<WgpuClient>(instance);
    if (!oapiRegisterGraphicsClient(candidate.get())) throw std::runtime_error("Graphics client already registered");
    client=std::move(candidate);
}
DLLCLBK void ExitModule(HINSTANCE) {
    if (client) oapiUnregisterGraphicsClient(client.get());
    client.reset();
}
// Readback hook for integration tests and diagnostic captures, not per-frame presentation.
DLLCLBK bool orb_wgpu_read_surface(SURFHANDLE surface, BYTE *pixels, size_t bytes, DWORD pitch) {
    return client && client->Read(surface,pixels,bytes,pitch);
}

// Diagnostic presentation source until the scene compositor owns the output.
DLLCLBK bool orb_wgpu_preview_surface(SURFHANDLE surface) {
    return client && client->SetOutput(surface);
}

DLLCLBK bool orb_wgpu_preview_mesh(MESHHANDLE mesh, const float *mvp, DWORD width, DWORD height) {
    return client && client->PreviewMesh(mesh,mvp,width,height);
}

DLLCLBK bool orb_wgpu_read_output(BYTE *pixels, size_t bytes, DWORD pitch) {
    return client && client->ReadOutput(pixels,bytes,pitch);
}

// Diagnostic identity for detecting unintended topology rebuilds.
DLLCLBK uint64_t orb_wgpu_output_mesh() {return client ? client->OutputMesh() : 0;}
DLLCLBK size_t orb_wgpu_shadow_only_groups() {return client ? client->ShadowOnlyGroups() : 0;}

DLLCLBK size_t orb_wgpu_star_count() {return client ? client->StarCount() : 0;}

DLLCLBK bool orb_wgpu_terrain_stats(size_t *meshes,size_t *textured,size_t *textures) {
    if(!client || !meshes || !textured || !textures)return false;
    client->TerrainStats(meshes,textured,textures);return true;
}

DLLCLBK bool orb_wgpu_particle_stats(size_t *count,size_t *textured) {
    if(!client || !count || !textured)return false;
    client->ParticleStats(count,textured);return true;
}

DLLCLBK void orb_wgpu_particles_visible(bool visible) {if(client)client->SetParticlesVisible(visible);}
DLLCLBK void orb_wgpu_exhaust_visible(bool visible) {if(client)client->SetExhaustVisible(visible);}
