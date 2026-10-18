#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
namespace orbiter::platform {
struct ImageRGBA {
    unsigned width=0,height=0;
    std::vector<std::uint8_t> pixels;
};
// Decode CPU image data, using the same case-insensitive asset lookup as the core.
// Pixel rows run top to bottom. File signatures, rather than extensions, select the decoder.
ImageRGBA LoadImageRGBA(std::string_view path);
}
