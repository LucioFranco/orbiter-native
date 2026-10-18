#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace orbiter::terrain {
// Orbiter ELE v1: 257 interior samples plus a one-sample border on every side.
// Rows run south to north; columns run west to east. Heights are metres.
class ElevationTile {
public:
    static constexpr unsigned stride = 259, intervals = 256;
    static std::optional<ElevationTile> Decode(const std::uint8_t *bytes, std::size_t size);
    // Applies ELE modification masks atomically; malformed/overflowing patches
    // leave the destination unchanged. Only the original overlay formats apply.
    static bool ApplyModification(const std::uint8_t *bytes, std::size_t size,
                                  double resolution, std::int16_t *destination);
    const std::vector<double> &Heights() const { return heights_; }
    // (0,0) is the southwest interior corner, (1,1) the northeast. The padding
    // can be sampled for normals; coordinates beyond the padded extent clamp.
    double Sample(double east, double north) const;
private:
    std::vector<double> heights_;
};
}
