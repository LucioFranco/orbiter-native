#include "Orbiter.h"
#include "CoreState.h"
#include "Psys.h"
#include "Vessel.h"
#include "../Src/WgpuClient/NativeParticleStream.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

extern PlanetarySystem *g_psys;
extern Vessel *g_focusobj;
namespace fs = std::filesystem;
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
struct ParticleProbe : oapi::ParticleStream {
    bool &destroyed;
    ParticleProbe(PARTICLESTREAMSPEC *spec,bool &flag):ParticleStream(nullptr,spec),destroyed(flag){}
    ~ParticleProbe() override {destroyed=true;}
    double Lifetime() const {return specification.lifetime;}
};
struct Scratch {
    fs::path old = fs::current_path();
    fs::path path = fs::temp_directory_path() / ("orbiter-world-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    explicit Scratch(const char *fixture) {
        fs::copy(fixture, path, fs::copy_options::recursive);
        fs::current_path(path);
    }
    ~Scratch() { fs::current_path(old); fs::remove_all(path); }
};
int main(int argc, char **argv)
{
    try {
        require(argc >= 2 && argc <= 4, "fixture path required; optional NASSP mesh path");
        bool destroyed=false;
        PARTICLESTREAMSPEC streamSpec{};streamSpec.lifetime=3.5;
        auto stream=new ParticleProbe(&streamSpec,destroyed);
        streamSpec.lifetime=0;
        require(stream->Lifetime()==3.5,"particle stream owns its specification");
        delete static_cast<oapi::ParticleStream *>(stream);
        require(destroyed,"renderer particle cleanup runs through base ownership");
        Scratch scratch(argv[1]);
        if (argc >= 3) {
            fs::create_directories("Meshes/ProjectApollo");
            fs::copy_file(argv[2], "Meshes/ProjectApollo/CM-HatchF.msh");
            fs::copy_file(fs::path(argv[2]).parent_path() / "CM-Docktgt.msh",
                          "Meshes/ProjectApollo/CM-Docktgt.msh");
        }
        VECTOR3 savedPosition{};
        double savedMJD = 0;
        {
            Orbiter host;
            if (argc == 4) {
                fs::path plugin=argv[3];
                require(host.LoadModule(plugin.parent_path().string().c_str(),plugin.stem().string().c_str()) &&
                    host.GetGraphicsClient() && host.GetGraphicsClient()->clbkInitialise(), "load GPU client for world test");
            }
            host.Launch("orbit");
            // A paused origin shift must immediately update cached SDK surface
            // readings; NASSP moves from the mesh origin to its CoG on load.
            {
                auto sdk=oapiGetFocusInterface();VECTOR3 before{},after{};MATRIX3 r{};
                sdk->GetRelativePos(sdk->GetGravityRef(),before);sdk->GetRotationMatrix(r);
                const auto shift=tmul(r,unit(before)*2.0);
                const double altitude=sdk->GetAltitude(ALTMODE_MEANRAD);
                sdk->ShiftCentreOfMass(shift);sdk->GetRelativePos(sdk->GetGravityRef(),after);
                require(std::abs(sdk->GetAltitude(ALTMODE_MEANRAD)-altitude-2)<1e-5,
                        "paused CoG shift refreshes cached altitude without a timestep");
                sdk->ShiftCentreOfMass(-shift);
                require(std::abs(sdk->GetAltitude(ALTMODE_MEANRAD)-altitude)<1e-5 && length(after-before)>1.99,
                        "reversing the origin shift restores position and surface readings");
            }
            for (auto object : {oapiGetFocusObject(), oapiGetGbodyByName(const_cast<char *>("Earth"))}) {
                require(object != nullptr,"world transform object exists");
                const VECTOR3 local=_V(13.25,-8.5,91.75); VECTOR3 global{},roundtrip{};
                oapiLocalToGlobal(object,&local,&global);oapiGlobalToLocal(object,&global,&roundtrip);
                require(length(roundtrip-local)<1e-3,"SDK local/global transforms preserve vessel and planet coordinates");
            }
            {
                PARTICLESTREAMSPEC spec{};spec.amin=1;spec.srcrate=10;spec.lifetime=1;spec.v0=5;
                spec.levelmap=PARTICLESTREAMSPEC::LVL_LIN;spec.atmsmap=PARTICLESTREAMSPEC::ATM_FLAT;
                orbiter::graphics::NativeParticleStream stream(nullptr,&spec);
                double level=1;const auto source=oapiGetFocusObject();
                const VECTOR3 offset=_V(2,0,0),direction=_V(0,1,0);
                stream.Attach(source,offset,direction,&level);stream.Advance(0.1);
                VECTOR3 expected{};oapiGetFocusInterface()->Local2Global(offset,expected);
                require(stream.Particles().size()==1 && length(stream.Particles()[0].position-expected)<1e-5,"particle emitter follows real vessel global transform");
                level=0;stream.Advance(0.1);
                require(stream.Particles().size()==1,"variable source level controls emission");
                stream.Detach();stream.Advance(2);
                require(stream.Finished(),"detached vessel stream expires without source access");
            }
            if (argc == 4) {
                host.Cfg()->CfgVisualPrm.bParticleStreams = true;
                PARTICLESTREAMSPEC spec{};
                Vector pos(0,0,0), dir(0,0,1);
                double level=1;
                ThrustSpec thruster{};
                require(!g_focusobj->AddParticleStream(&spec,pos,dir,&level), "unsupported particle stream returns null");
                require(!g_focusobj->AddExhaustStream(&thruster,&spec,&pos,&dir), "unsupported exhaust does not dereference null");
                spec.amin=1;spec.srcrate=10;spec.lifetime=1;spec.levelmap=PARTICLESTREAMSPEC::LVL_LIN;
                auto live=g_focusobj->AddParticleStream(&spec,pos,dir,&level);
                require(live,"valid native graphics particle stream created");
                host.StepSimulation(0.1);
                require(!static_cast<orbiter::graphics::NativeParticleStream *>(live)->Particles().empty(),"graphics simulation callback advances vessel particles");
                require(g_focusobj->DelParticleStream(live),"vessel detaches renderer-owned stream");
                host.StepSimulation(2);
                require(!g_focusobj->AddReentryStream(&spec), "unsupported reentry stream returns null");
                require(!g_focusobj->DelExhaustStream(nullptr) && !g_focusobj->DelReentryStream(nullptr),
                    "failed stream creation does not retain null entries");
            }
            if (argc >= 3) {
                const auto mesh = oapiLoadMeshGlobal("ProjectApollo\\CM-HatchF");
                require(mesh && oapiMeshGroupCount(mesh) == 1, "real NASSP hatch mesh loads");
                const auto group = oapiMeshGroup(mesh, 0);
                require(group && group->nVtx == 110 && group->nIdx == 432,
                        "NASSP hatch geometry survives native mesh parsing");
                for (DWORD i = 0; i < group->nIdx; ++i)
                    require(group->Idx[i] < group->nVtx, "mesh triangle indices are valid");
                require(oapiMeshMaterialCount(mesh) == 1, "NASSP hatch material loads");
                require(oapiLoadMeshGlobal("ProjectApollo\\CM-HatchF") == mesh,
                        "persistent mesh cache reuses the parsed geometry");
                require(!oapiLoadMeshGlobal("ProjectApollo\\MissingMesh"),
                        "missing mesh is rejected");
                const auto dockTarget = oapiLoadMeshGlobal("ProjectApollo/CM-Docktgt");
                require(dockTarget && oapiMeshGroupCount(dockTarget) == 4,
                        "empty groups retain NASSP animation group indices");
                require(oapiMeshGroup(dockTarget, 2)->nVtx == 0 &&
                        oapiMeshGroup(dockTarget, 3)->nVtx == 63,
                        "geometry after an empty group remains in its original slot");
            }
            require(g_psys && g_psys->nVessel() == 1, "actual scenario creates one vessel");
            require(g_focusobj && std::string(g_focusobj->Name()) == "Probe", "scenario focus applied");
            auto earth = g_psys->GetPlanet("Earth");
            require(earth != nullptr, "Earth loaded into real planetary system");
            char earthName[] = "Earth";
            char probeName[] = "Probe";
            OBJHANDLE probe = oapiGetVesselByName(probeName);
            OBJHANDLE earthHandle = oapiGetGbodyByName(earthName);
            require(probe == oapiGetFocusObject() && probe == oapiGetVesselByIndex(0),
                    "SDK lookup and focus refer to the simulated vessel");
            require(oapiGetVesselCount() == 1 && !oapiGetVesselByIndex(1), "SDK vessel bounds");
            require(std::abs(oapiGetMass(earthHandle) / 5.972e24 - 1) < 1e-12,
                    "SDK exposes configured planetary mass");
            require(oapiGetSize(earthHandle) == 6371000, "SDK exposes planetary radius");
            require(oapiCameraTarget() == probe, "SDK camera follows scenario focus");
            oapiCameraSetAperture(0.4);
            require(std::abs(oapiCameraAperture() - 0.4) < 1e-12,
                    "SDK camera aperture changes actual host camera");
            Vector start = g_focusobj->GPos() - earth->GPos();
            require(std::abs(start.length() - 7e6) < 1, "scenario relative position loaded");
            const double initialTime=oapiGetSimTime();
            for (int i = 0; i < 600; ++i) {
                host.StepSimulation(0.1);
                require(std::isfinite(g_focusobj->GPos().length()), "vessel state stays finite at every step");
            }
            Vector end = g_focusobj->GPos() - earth->GPos();
            require(std::isfinite(end.length()), "world propagation remains finite");
            require((end - start).length() > 400000, "vessel advances through actual world dynamics");
            require(std::abs(end.length() - 7e6) < 100, "near-circular orbit remains bounded");
            require(std::abs(td.SimT0 - initialTime - 60) < 1e-8, "native host advances core clock");
            VECTOR3 position;
            oapiGetRelativePos(probe, earthHandle, &position);
            require(std::abs(length(position) - end.length()) < 1e-6,
                    "SDK relative position reflects the integrated world");
            require(std::abs(oapiGetSimTime() - initialTime - 60) < 1e-8 &&
                    std::abs(oapiGetSimStep() - 0.1) < 1e-12,
                    "SDK simulation clock matches the host");
            VECTOR3 cameraPosition, cameraDirection;
            oapiCameraGlobalPos(&cameraPosition);
            oapiCameraGlobalDir(&cameraDirection);
            require(std::isfinite(length(cameraPosition)) &&
                    std::abs(length(cameraDirection) - 1) < 1e-12,
                    "SDK camera state stays finite and direction normalized");
            VESSELSTATUS stageStatus{};
            oapiGetVesselInterface(probe)->GetStatus(stageStatus);
            OBJHANDLE stage = oapiCreateVessel("Stage", "Probe", stageStatus);
            require(stage && oapiGetVesselCount() == 2, "SDK creates a staging vessel");
            require(oapiDeleteVessel(probe), "SDK queues focus-vessel deletion");
            require(oapiGetVesselCount() == 2, "deletion stays deferred until frame boundary");
            host.StepSimulation(0.1);
            require(oapiGetVesselCount() == 1 && oapiGetFocusObject() == stage &&
                    oapiCameraTarget() == stage,
                    "frame-boundary deletion transfers focus and camera to surviving stage");
            VESSELSTATUS2 debrisStatus{};
            debrisStatus.version = 2;
            oapiGetVesselInterface(stage)->GetStatusEx(&debrisStatus);
            OBJHANDLE debris = oapiCreateVesselEx("Debris", "Probe", &debrisStatus);
            require(debris && oapiGetVesselCount() == 2, "extended SDK creates a vessel");
            require(oapiDeleteVessel(debris), "SDK queues non-focus deletion");
            host.StepSimulation(0.1);
            require(oapiGetVesselCount() == 1 && oapiGetFocusObject() == stage,
                    "non-focus deletion preserves the active vessel");
            oapiGetRelativePos(stage, earthHandle, &savedPosition);
            savedMJD = oapiGetSimMJD();
            require(!oapiSaveScenario("MissingDirectory/Save", "Invalid destination"),
                    "scenario save reports an unavailable destination");
            require(oapiSaveScenario("SavedStage", "Native staging round trip"),
                    "SDK saves the integrated world");
            std::cout << "Real world integration: 600 steps, radius " << end.length() << " m\n";
        }
        require(!g_psys && !g_focusobj, "native host releases world state");
        {
            Orbiter restored;
            restored.Launch("SavedStage");
            require(oapiGetVesselCount() == 1 && std::string(g_focusobj->Name()) == "Stage",
                    "saved scenario restores the surviving vessel and focus");
            char earthName[] = "Earth";
            VECTOR3 position;
            oapiGetRelativePos(oapiGetFocusObject(), oapiGetGbodyByName(earthName), &position);
            require(length(position - savedPosition) < 1,
                    "saved vessel state preserves position to scenario precision");
            require(std::abs(oapiGetSimMJD() - savedMJD) < 1e-9,
                    "saved scenario preserves simulation date");
            restored.StepSimulation(0.1);
            require(std::isfinite(g_focusobj->GPos().length()), "restored world resumes integration");
            auto control=oapiGetVesselInterface(oapiGetFocusObject());
            control->SetEmptyMass(1000);control->SetPMI(_V(1,1,1));control->SetAttitudeMode(RCS_ROT);
            auto propellant=control->CreatePropellantResource(10);
            auto thruster=control->CreateThruster(_V(0,0,1),_V(0,1,0),100,propellant,1000);
            control->CreateThrusterGroup(&thruster,1,THGROUP_ATT_PITCHUP);
            VECTOR3 angularBefore{},angularAfter{};control->GetAngularVel(angularBefore);
            restored.NativeKeyEvent(OAPI_KEY_NUMPAD2,true);
            for(int i=0;i<10;++i)restored.StepSimulation(0.1);
            control->GetAngularVel(angularAfter);
            require(length(angularAfter-angularBefore)>0.001,"native keyboard fires real thruster and changes angular velocity");
            restored.ReleaseNativeKeys();restored.StepSimulation(0.1);
            require(control->GetThrusterLevel(thruster)==0,"released keyboard stops manual thruster override");

        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
