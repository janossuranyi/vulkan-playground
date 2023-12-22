#pragma once
#include "renderer/VulkanTechnic.h"
#include "renderer/FrameCounter.h"

namespace jsr {

	class DepthPassTechnic : public VulkanTechnic {
	public:
		DepthPassTechnic() {};


		// Inherited via VulkanTechnic
		virtual VkResult createPipeline(const GraphichPipelineShaderBinary& shaders) override;

		virtual std::vector<VkClearValue> getClearValues() const override;
		virtual Type getType() const { return Type::Graphics; }
	};
}