#pragma once

#include "renderer/VulkanTechnic.h"
#include "renderer/FrameCounter.h"

namespace jsr {

	class DefaultTechnic : public VulkanTechnic {

	public:

		DefaultTechnic() {};

		// Inherited via VulkanTechnic
		virtual VkResult createPipeline(const GraphichPipelineShaderBinary& shaders) override;
		virtual uint32_t getPushConstantsSize() const;		
		virtual std::vector<VkClearValue> getClearValues() const override;
		virtual Type getType() const { return Type::Graphics; }
	};
}