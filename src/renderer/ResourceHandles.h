#pragma once

#include "pch.h"

namespace jsr {

	enum class ImageHandleType : uint8_t { Default, Transient, Swapchain };

	namespace handle {
		const uint32_t INVALID_INDEX = ~0;

		struct UniformBuffer { uint32_t index = INVALID_INDEX; };
		struct StorageBuffer { uint32_t index = INVALID_INDEX; };
		struct Image { 
			uint32_t index = INVALID_INDEX; 
			ImageHandleType type = ImageHandleType::Default;
		};
		struct Mesh { uint32_t index = INVALID_INDEX; };
		struct Sampler { uint32_t index = INVALID_INDEX; };
		struct GraphicsPipeline { uint32_t index = INVALID_INDEX; };
		struct ComputePipeline { uint32_t index = INVALID_INDEX; };
		struct DescriptorSet { uint32_t index = INVALID_INDEX; };

		template<class T>
		inline bool is_valid(const T& h) {
			return h.index != INVALID_INDEX;
		}
	}
}