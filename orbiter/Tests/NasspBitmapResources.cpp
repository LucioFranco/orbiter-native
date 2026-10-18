#include "NasspBitmapResources.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

using namespace nassp::resources;
// Both vessels use 533 for their caution/warning lights, with different artwork.
static_assert(std::string_view(FindBitmap(csm, 533)) != FindBitmap(lm, 533));
static_assert(FindBitmap(csm, 0) == nullptr);
static_assert(FindBitmap(lm, 0xffffffffu) == nullptr);
static_assert(FindBitmap(mfd, 101) != nullptr);

template<std::size_t N>
bool check(const std::filesystem::path &root, const Bitmap (&table)[N])
{
    for (const auto &entry : table) {
        const auto path = root / entry.path;
        std::ifstream file(path, std::ios::binary);
        char signature[2] = {};
        file.read(signature, sizeof signature);
        if (!file || signature[0] != 'B' || signature[1] != 'M') {
            std::cerr << "Bitmap " << entry.id << " does not resolve to a BMP: " << path << '\n';
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    return check(argv[1], csm) && check(argv[1], lm) && check(argv[1], mfd) ? 0 : 1;
}
