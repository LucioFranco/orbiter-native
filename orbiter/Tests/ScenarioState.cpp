#include "State.h"
#include "CoreState.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
    if (argc != 3) return 1;
    try {
        State apollo;
        if (!apollo.Read(argv[1]) || std::string(apollo.Solsys()) != "ProjectApollo\\Sol_VirtualAGC" ||
            std::string(apollo.Focus()) != "Columbia" || std::abs(apollo.Mjd() - 40418.397222333) > 1e-10)
            throw std::runtime_error("Real NASSP Apollo 11 scenario metadata did not load");

        // Round-trip the engine's environment/focus state, not the vessel block.
        const std::filesystem::path output = argv[2];
        td.Reset(apollo.Mjd());
        td.BeginStep(10.0, true);
        apollo.Update("Columbia");
        { std::ofstream stream(output); apollo.Write(stream, "native core round-trip"); }
        State reloaded;
        if (!reloaded.Read(output.string().c_str()) ||
            std::abs(reloaded.Mjd() - (40418.397222333 + 10.0/86400.0)) > 1e-9 ||
            std::string(reloaded.Focus()) != "Columbia")
            throw std::runtime_error("Scenario environment state failed round-trip");

        { std::ofstream stream(output); stream <<
            "BEGIN_ENVIRONMENT\nSystem Sol\nSplashScreen #123456 images\\launch.png\nEND_ENVIRONMENT\n"; }
        State splash;
        if (!splash.Read(output.string().c_str()) || splash.SplashColor() != RGB(0x12, 0x34, 0x56))
            throw std::runtime_error("Splash color parsing failed");
        { std::ofstream stream(output); splash.Write(stream); }
        if (!reloaded.Read(output.string().c_str()) || reloaded.SplashColor() != splash.SplashColor() ||
            std::string(reloaded.SplashScreen()) != splash.SplashScreen())
            throw std::runtime_error("Splash color and path failed round-trip");
        std::filesystem::remove(output);
        std::cout << "NASSP Apollo 11 environment and focus loaded and round-tripped\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
