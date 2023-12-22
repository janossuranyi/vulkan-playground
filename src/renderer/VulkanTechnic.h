#pragma once

#include "renderer/Vulkan.h"
#include "renderer/VulkanDevice.h"
#include "renderer/Resources.h"
#include "renderer/ResourceDescriptions.h"
#include "renderer/FrameCounter.h"

namespace jsr {

	class VulkanRenderer;
	class VulkanTechnic {
	public:
		enum Type { Graphics, Compute };
		VulkanTechnic();
		VulkanTechnic(VulkanDevice* device, VulkanRenderer* renderer, VkDescriptorPool pool);
		VkPipeline getPipeline() const;
		void setDevice(VulkanDevice* device);
		void setEngine(VulkanRenderer* renderer);
		void setDescriptorPool(VkDescriptorPool pool);
		VkPipelineLayout getPipelineLayout() const;
		VkDescriptorSetLayout getDescriptorSetLayout() const;
		VkDescriptorSetLayout getPerObjectLayout() const;
		VkDescriptorSet getDescriptorSet(uint32_t index) const;
		VkRenderPass getRenderPass() const;
		virtual uint32_t getPushConstantsSize() const { return 0; }
		virtual VkResult createPipeline(const GraphichPipelineShaderBinary& shaders);
		virtual VkResult createPipeline(const ComputePipelineShaderBinary& shaders);
		virtual std::vector<VkClearValue> getClearValues() const = 0;
		virtual Type getType() const = 0;
		virtual ~VulkanTechnic();
	protected:
		virtual void shutdown();
		VulkanDevice* m_device;
		VulkanRenderer* m_engine;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VkDescriptorSetLayout m_perFrameLayout;
		VkDescriptorSetLayout m_perObjectLayout;
		VkDescriptorSet m_perFrameSets[FrameCounter::INFLIGHT_FRAMES];
		VkDescriptorPool m_descriptorPool;
		VkRenderPass m_renderPass;
	};
}