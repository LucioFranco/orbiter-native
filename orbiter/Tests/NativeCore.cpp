#include "Config.h"
#include "CoreState.h"
#include "Element.h"
#include "Log.h"
#include "Platform/Vfs.h"
#include "Platform/DynamicLibrary.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace fs = std::filesystem;
using namespace orbiter::platform;

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Scratch {
    fs::path previous = fs::current_path();
    fs::path path;
    Scratch() {
        path = fs::temp_directory_path() / ("orbiter-core-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(fs::create_directory(path), "create temporary directory");
        fs::current_path(path);
    }
    ~Scratch() { fs::current_path(previous); fs::remove_all(path); }
};

static std::string callbackLog;
static void captureLog(const char *message) { callbackLog = message; }

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "module fixture argument");
        auto module = fs::absolute(argv[1]);
        Scratch scratch;
        fs::create_directories("Config/MixedCase");
        fs::create_directories("Scenarios/Apollo");
        fs::create_directory("Meshes");
        std::ofstream("Config/MixedCase/Orbit.cfg") <<
            "Epoch = 2000.0\nSemiMajorAxis = 7000000\nEccentricity = 0.1\n";
        std::ofstream("Scenarios/Apollo/Launch.scn") << "BEGIN_ENVIRONMENT\n  System Sol\nEND_ENVIRONMENT\n";
        std::ofstream("Meshes/Probe.msh") << "MSHX1\nGROUPS 0\n";
        std::ofstream("Orbiter.cfg") << "ConfigDir = .\\Config\\\nScenarioDir = .\\Scenarios\\\n";

        require(fs::equivalent(ResolvePath("config\\MIXEDCASE\\orbit.CFG"),
            "Config/MixedCase/Orbit.cfg"), "case-folded Windows path");
        require(ResolvePath("").empty(), "empty paths stay empty");
        require(ResolvePath("config/New.cfg").parent_path().filename() == "Config",
            "new files retain resolved parent");

        // This assertion runs on case-sensitive filesystems; default macOS and
        // Windows volumes cannot create the conflicting pair.
        std::ofstream("Config/Collision") << "one";
        if (!fs::exists("Config/collision")) {
            std::ofstream("Config/collision") << "two";
            bool ambiguous = false;
            try { ResolvePath("Config/COLLISION"); }
            catch (const std::runtime_error &) { ambiguous = true; }
            require(ambiguous, "ambiguous asset paths must fail");
            require(fs::equivalent(ResolvePath("Config/Collision"), "Config/Collision"),
                "exact spelling wins over folded ambiguity");
        }
#ifndef _WIN32 // Creating symlinks on Windows can require additional privileges.
        fs::create_directories("Actual/Child");
        std::ofstream("Actual/Target") << "target";
        fs::create_directory_symlink("Actual/Child", "Link");
        require(fs::equivalent(ResolvePath("link/../target"), "Actual/Target"),
            "dot-dot traverses symlinks before normalization");
#endif

        Config config;
        char blank[] = " \t\r\n";
        char empty[] = "";
        require(*trim_string(blank) == '\0' && *trim_string(empty) == '\0', "blank CRLF config lines");
        require(config.Load("ORBITER.CFG"), "load actual Config parser through VFS");
        const char *configPath = config.ConfigPath("mixedcase\\ORBIT");
        require(fs::exists(configPath), "ConfigPath resolves asset");
        const std::string savedConfigPath(configPath);
        require(fs::exists(config.MeshPath("probe")), "MeshPath resolves asset");
        require(savedConfigPath == configPath, "different path families retain their buffers");
        require(fs::exists(config.ScnPath("apollo\\launch")), "ScnPath resolves asset");
        auto absoluteScenario = fs::absolute("Scenarios/Apollo/Launch.scn").string();
        require(fs::equivalent(config.ScnPath(absoluteScenario.c_str()), absoluteScenario),
            "absolute POSIX scenario path");
        require(config.Load(config.Root), "reload config using its owned filename");
        require(config.Write("config/Saved.cfg"), "write Config through resolved parent");
        require(fs::exists("Config/Saved.cfg"), "case-preserving config write");

        td.Reset(MJD2000);
        td.SetFixedStep(0.02);
        Elements orbit(config.ConfigPath("mixedcase\\orbit"));
        orbit.Setup(10000.0, 5.972e24, MJD2000);
        const double mu = Ggrav * (5.972e24 + 10000.0);
        const double expectedEnergy = -mu / (2.0 * orbit.a);
        for (int i = 0; i != 5000; ++i) {
            td.BeginStep(0.02, true);
            Vector position, velocity;
            orbit.Update(position, velocity);
            const double energy = 0.5 * velocity.length2() - mu / position.length();
            require(std::abs((energy - expectedEnergy) / expectedEnergy) < 1e-12,
                "Kepler propagation conserves orbital energy");
            td.EndStep(true);
        }
        require(std::abs(td.SimT0 - 100.0) < 1e-10, "5000 fixed timesteps");
        Vector start, startVelocity, end, endVelocity;
        orbit.PosVel(start, startVelocity, 0.0);
        orbit.PosVel(end, endVelocity, orbit.OrbitT());
        require((end - start).length() < 1e-5, "orbital period closes trajectory");

        DynamicLibrary library(module.string());
        auto clock = reinterpret_cast<TimeData *(*)()>(library.FindSymbol("SharedClock"));
        auto radius = reinterpret_cast<double (*)(double)>(library.FindSymbol("OrbitalRadius"));
        require(clock && radius, "native module exports");
        require(clock() == &td && clock()->SimT0 == td.SimT0,
            "module and executable share liborbiter clock");
        require(radius(0.0) > 6e6 && radius(0.0) < 7e6, "module executes real core orbital solver");
        require(!library.FindSymbol("MissingOptionalCallback"), "missing optional symbol");
        DynamicLibrary moved(std::move(library));
        require(!library.FindSymbol("SharedClock") && moved.FindSymbol("SharedClock"), "module ownership transfer");
        bool failedLoad = false;
        try { DynamicLibrary missing("NoSuchModule.dylib"); }
        catch (const std::runtime_error &e) { failedLoad = std::string(e.what()).find("NoSuchModule") != std::string::npos; }
        require(failedLoad, "missing module reports path");

        fs::create_directory("Modules");
        fs::copy_file(module, fs::path("Modules") / (std::string("Fixture") + ModuleSuffix()));
        auto rawModule = OpenLibrary("modules\\fixture.DlL");
        require(rawModule != nullptr, "legacy module path maps to native suffix through VFS");
        auto sharedClock = reinterpret_cast<TimeData *(*)()>(LibrarySymbol(rawModule, "SharedClock"));
        require(sharedClock && sharedClock() == &td, "low-level native loader shares core state");
        CloseLibrary(rawModule);
        require(OpenLibrary("Modules/Absent.dll") == nullptr && !LibraryError().empty(),
            "low-level native loader preserves a diagnostic on failure");
        require(!LibrarySymbol(nullptr, "SharedClock"), "null module cannot search global symbols");

        InitLog("native.log", false);
        SetLogOutFunc(captureLog);
        LogOut("mixed %d %.2f %s", 7, 2.5, "payload");
        require(callbackLog == "mixed 7 2.50 payload", "logger clones va_list for callback");
        const auto expectedLog = callbackLog;
        LogOut_Warning(__func__, __FILE__, __LINE__, "warning payload %d", 42);
        SetLogOutFunc(nullptr);
        std::ifstream log("native.log");
        std::string contents((std::istreambuf_iterator<char>(log)), {});
        require(contents.find(expectedLog) != std::string::npos, "file and callback formatting agree");
        require(contents.find("warning payload 42") != std::string::npos, "warning API formats its payload");
        std::cout << "Native core: paths, configuration, 5000 orbital steps and shared-library module passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
