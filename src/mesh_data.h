#pragma once
#include "pch.h"
#include "bounds.h"
namespace jsr {
    
    struct MeshData {
        struct PosType { float v[3]; };

        std::vector<uint32_t>  indices;

        std::vector<PosType> positions;
        std::vector<PosType> normals;
        std::vector<glm::vec4> tangents;
        std::vector<glm::vec2> uvs;

        Bounds aabb;
        int material;
    };

    struct VertexCacheEntry {
        uint32_t firstIndex;
        uint32_t indexCount;
        uint32_t baseVertex;
    };

}