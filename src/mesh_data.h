#pragma once
#include "pch.h"
#include "bounds.h"
#include "vkjs/vkjs.h"

namespace jsr {
    
    struct MeshData {
        std::vector<uint32_t>  indices;

        std::vector<uint8_t> positions;
        std::vector<uint8_t> normals;
        std::vector<uint8_t> tangents;
        std::vector<uint8_t> uvs;

        Bounds aabb;
        int material;
    };

    struct VertexCacheEntry {
        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t baseVertex;
    };

}