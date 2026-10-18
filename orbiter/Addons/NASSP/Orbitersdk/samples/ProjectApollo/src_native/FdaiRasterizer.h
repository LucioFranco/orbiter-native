/* Modified for native portability. Publication notice added 2026-10-18.
 * Original copyright, license grants and linking exceptions remain unchanged.
 * Behavioral changes are inventoried in upstream/nassp-v9/manifest.json.
 */
#pragma once
#include "Platform/Image.h"
#include <array>
namespace nassp {
// CPU replacement for the legacy 180-pixel GLU FDAI sphere.
class FdaiRasterizer {
public:
    static constexpr unsigned size=180;
    using Pixels=std::array<unsigned char,size*size*4>;
    static Pixels Render(const orbiter::platform::ImageRGBA &texture,double roll,double yaw,double pitch);
};
}
