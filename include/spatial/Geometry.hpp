#pragma once

#include <cstdint>
#include <vector>

namespace spatial {

struct MeshData {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
};

MeshData createTexturedQuad();
MeshData createInvertedSphere(std::uint32_t latitudeSegments, std::uint32_t longitudeSegments, float radius);

} // namespace spatial
