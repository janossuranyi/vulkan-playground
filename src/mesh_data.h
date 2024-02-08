#pragma once
#include "pch.h"
#include "bounds.h"
#include "vkjs/vkjs.h"

namespace jsr {
    
    struct MeshData {
        std::vector<uint32_t>  indices;

        std::vector<vkjs::vec3> positions;
        std::vector<vkjs::vec3> normals;
        std::vector<vkjs::vec4> tangents;
        std::vector<vkjs::vec2> uvs;

        Bounds aabb;
        int material;
    };

    struct VertexCacheEntry {
        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t baseVertex;
    };

}