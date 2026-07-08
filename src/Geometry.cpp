#include "spatial/Geometry.hpp"

#include <cmath>

namespace spatial {

MeshData createTexturedQuad() {
    MeshData mesh;

    mesh.vertices = {
        // position xyz, uv
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    };

    mesh.indices = {
        0, 1, 2,
        2, 3, 0,
    };

    return mesh;
}

MeshData createInvertedSphere(std::uint32_t latitudeSegments, std::uint32_t longitudeSegments, float radius) {
    MeshData mesh;

    if (latitudeSegments < 3 || longitudeSegments < 3 || radius <= 0.0f) {
        return mesh;
    }

    const float pi = 3.14159265358979323846f;

    for (std::uint32_t lat = 0; lat <= latitudeSegments; ++lat) {
        const float v = static_cast<float>(lat) / static_cast<float>(latitudeSegments);
        const float theta = v * pi;

        for (std::uint32_t lon = 0; lon <= longitudeSegments; ++lon) {
            const float u = static_cast<float>(lon) / static_cast<float>(longitudeSegments);
            const float phi = u * (2.0f * pi);

            const float x = radius * std::sin(theta) * std::cos(phi);
            const float y = radius * std::cos(theta);
            const float z = radius * std::sin(theta) * std::sin(phi);

            mesh.vertices.push_back(x);
            mesh.vertices.push_back(y);
            mesh.vertices.push_back(z);
            mesh.vertices.push_back(u);
            mesh.vertices.push_back(v);
        }
    }

    const std::uint32_t rowVertices = longitudeSegments + 1;
    for (std::uint32_t lat = 0; lat < latitudeSegments; ++lat) {
        for (std::uint32_t lon = 0; lon < longitudeSegments; ++lon) {
            const std::uint32_t current = lat * rowVertices + lon;
            const std::uint32_t next = current + rowVertices;

            // Inverted winding for inside-facing sphere.
            mesh.indices.push_back(current);
            mesh.indices.push_back(next + 1);
            mesh.indices.push_back(next);

            mesh.indices.push_back(current);
            mesh.indices.push_back(current + 1);
            mesh.indices.push_back(next + 1);
        }
    }

    return mesh;
}

} // namespace spatial
