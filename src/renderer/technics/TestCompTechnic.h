#pragma once

#include "renderer/VulkanTechnic.h"
namespace jsr {
	
	class TestCompTechnic : public VulkanTechnic {
		// Inherited via VulkanTechnic
		virtual std::vector<VkClearValue> getClearValues() const override;
		virtual Type getType() const { return Type::Compute; }
		virtual VkResult createPipeline(const ComputePipelineShaderBinary& shaders) override;
	};
}