#pragma once
#include "renderer/VulkanPipeline.h"
#include "renderer/FrameCounter.h"

namespace jsr {

	class DepthPrePass : public VulkanPipeline {
	public:
		DepthPrePass() {};


		// Inherited via VulkanTechnic
		virtual VkResult createPipeline(const GraphichPipelineShaderBinary& shaders) override;

		virtual std::vector<VkClearValue> getClearValues() const override;
		virtual Type getType() const { return Type::Graphics; }
	};
}