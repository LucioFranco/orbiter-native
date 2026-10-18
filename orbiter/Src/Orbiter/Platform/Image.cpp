#include "Image.h"
#include "Vfs.h"
#include <memory>
#include <stdexcept>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
namespace orbiter::platform {
ImageRGBA LoadImageRGBA(std::string_view name) {
    const auto path=ResolvePath(name).string();
    int width=0,height=0,channels=0;
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> data(stbi_load(path.c_str(),&width,&height,&channels,4),stbi_image_free);
    if(!data)throw std::runtime_error("Cannot decode image " + path);
    if(width<=0 || height<=0 || std::uint64_t(width)*height>64*1024*1024)
        throw std::runtime_error("Image dimensions exceed CPU image limit: " + path);
    return {unsigned(width),unsigned(height),{data.get(),data.get()+std::size_t(width)*height*4}};
}
}
