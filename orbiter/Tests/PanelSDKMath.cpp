// Exercise the actual NASSP archive without replacing unresolved engine APIs
// with stubs. Matrix and vector objects have no simulation-core dependencies.
#include "Matrix.h"
#include "Vectors.h"
#include <cmath>
#include <cstdio>

int main() {
    matrix rotation(0.31, -0.72, 1.14);
    vector3 original(2.0, -3.0, 5.0);
    vector3 rotated = rotation * original;
    matrix transpose = !rotation;
    vector3 recovered = transpose * rotated;
    auto near = [](double a, double b) { return std::abs(a - b) < 1e-12; };
    if (!near(rotated.length(), original.length()) ||
        !near(recovered.x, original.x) || !near(recovered.y, original.y) ||
        !near(recovered.z, original.z) || !near(rotation.det(), 1.0)) {
        std::fprintf(stderr, "NASSP rotation failed length/determinant/round-trip checks\n");
        return 1;
    }
    vector3 normalized = original.normalize();
    if (!near(normalized.length(), 1.0)) {
        std::fprintf(stderr, "NASSP vector normalization failed\n");
        return 1;
    }
    return 0;
}
