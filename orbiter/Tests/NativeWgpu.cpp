#include "WgpuBridge.h"
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
static_assert(sizeof(OrbFill) == 20 && offsetof(OrbFill, rgba) == 16, "Rust C ABI layout");
static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(std::string(message) + ": " + orb_gpu_error());
}
int main() {
    try {
        std::unique_ptr<OrbGpu, decltype(&orb_gpu_destroy)> gpu(orb_gpu_create(), orb_gpu_destroy);
        require(bool(gpu), "create hardware renderer");
        constexpr unsigned width = 7, height = 5, pitch = 36; // intentionally unaligned GPU rows
        auto surface = orb_gpu_surface_create(gpu.get(), width, height);
        require(surface != 0, "create surface");
        std::vector<uint8_t> pixels(pitch * height, 99);
        for (unsigned y=0; y<height; ++y) for (unsigned x=0; x<width; ++x) {
            auto p = &pixels[y*pitch+x*4];
            p[0]=10; p[1]=20; p[2]=30; p[3]=255;
        }
        require(orb_gpu_surface_upload(gpu.get(), surface, pixels.data(), pixels.size(), pitch), "upload");
        const OrbFill batch[] = {
            {-2, 1, 5, 3, {255, 0, 0, 255}}, // clipping at left
            {2, 2, 3, 2, {0, 255, 0, 128}}, // later fill overwrites overlap including alpha
            {100, 100, 2, 2, {1, 2, 3, 4}}, // completely outside
        };
        require(orb_gpu_surface_fill_batch(gpu.get(), surface, batch, 3), "batched fills");
        require(orb_gpu_surface_read(gpu.get(), surface, pixels.data(), pixels.size(), pitch), "readback");
        for (unsigned y=0; y<height; ++y) for (unsigned x=0; x<width; ++x) {
            std::array<uint8_t,4> expected{10,20,30,255};
            if (x<3 && y>=1 && y<4) expected = {255,0,0,255};
            if (x>=2 && x<5 && y>=2 && y<4) expected = {0,255,0,128};
            for(unsigned c=0;c<4;++c) require(pixels[y*pitch+x*4+c]==expected[c], "GPU pixel mismatch");
        }
        for (unsigned y=0;y<height;++y) for(unsigned x=width*4;x<pitch;++x)
            require(pixels[y*pitch+x]==99, "readback overwrote row padding");
        require(!orb_gpu_surface_upload(gpu.get(), surface, pixels.data(), 1, pitch), "reject short input");
        require(!orb_gpu_surface_create(gpu.get(), 0, height), "reject empty surface");
        require(orb_gpu_surface_release(gpu.get(), surface), "release surface");
        require(!orb_gpu_surface_read(gpu.get(), surface, pixels.data(), pixels.size(), pitch), "reject stale handle");
        std::cout << "GPU upload, batched clipped fills and readback verified\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
