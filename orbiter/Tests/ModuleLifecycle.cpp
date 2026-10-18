#include "Platform/ModuleLibrary.h"
#include <iostream>
#include <stdexcept>

using namespace orbiter::platform;
static int exits = 0;
static void observeExit() { ++exits; }
static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
template<class Function>
Function symbol(HINSTANCE handle, const char *name)
{
    auto address = LibrarySymbol(handle, name);
    require(address != nullptr, name);
    return reinterpret_cast<Function>(address);
}

static void exercise(const char *path)
{
    const int previousExits = exits;
    auto first = OpenModule(path);
    require(first != nullptr, "load first module instance");
    auto second = OpenModule(path);
    require(first == second, "vessels share one loaded addon image");
    auto count = symbol<int (*)()>(first, "InitializationCount");
    require(count() == 1, "InitModule called exactly once");
    require(symbol<HINSTANCE (*)()>(first, "InitializedHandle")() == first,
        "InitModule receives the actual native handle");
    require(symbol<int (*)()>(first, "GetModuleVersion")() > 0,
        "portable SDK exports module version");
    symbol<void (*)(void (*)())>(first, "SetExitObserver")(observeExit);
    require(CloseModule(first), "close first reference");
    require(exits == previousExits && count() == 1, "remaining vessel keeps module alive");
    require(CloseModule(second), "close last reference");
    require(exits == previousExits + 1, "ExitModule called on last reference");
    require(!CloseModule(second), "duplicate close cannot unload an unrelated library");
}

int main(int argc, char **argv)
{
    try {
        require(argc == 3, "modern and legacy fixture paths");
        require(!CloseModule(nullptr), "null module close");
        exercise(argv[1]);
        exercise(argv[2]);
        std::cout << "Native addon initialization, shared lifetime and shutdown passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
