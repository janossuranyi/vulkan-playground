#pragma once

#include "renderer/VulkanPipeline.h"
namespace jsr {
	
	class CompImageGen : public VulkanPipeline {
		// Inherited via VulkanTechnic
		virtual std::vector<VkClearValue> getClearValues() const override;
		virtual Type getType() const { return Type::Compute; }
		virtual VkResult createPipeline(const ComputePipelineShaderBinary& shaders) override;
	};
}