#pragma once
#include "renderer/Bounds.h"
#include "renderer/Material.h"

namespace jsr {
    struct MeshData {
        std::vector<uint32_t>  indices;

        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec4> tangents;
        std::vector<glm::vec2> uvs;

        Bounds aabb;
        int material;
    };
}