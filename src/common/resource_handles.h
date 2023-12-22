#pragma once

#include <cinttypes>
#include <limits>

static constexpr uint32_t invalidIndex = std::numeric_limits<uint32_t>::max();

struct GraphicsPassHandle {
	uint32_t index = invalidIndex;
};

struct ImageHandle {
	uint32_t index = invalidIndex;
};

struct SamplerHandle {
	uint32_t index = invalidIndex;
};

struct UniformBufferHandle {	
	uint32_t index = invalidIndex;
};

struct StorageBufferHandle {
	uint32_t index = invalidIndex;
};

struct BufferHandle {
	uint32_t index = invalidIndex;
};

struct MeshHandle {
	uint32_t index = invalidIndex;
};

struct RenderPassHandle {
	uint32_t index = invalidIndex;
};

struct ComputeShaderHandle {
	uint32_t index = invalidIndex;
};

struct GraphicShadersHandle {
	uint32_t index = invalidIndex;
};

struct MeshRenderInfo {
	MeshHandle handle;
	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
	int32_t vertexOffset = 0;
	int textures[4]{};
};
