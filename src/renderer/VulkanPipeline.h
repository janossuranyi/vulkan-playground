#pragma once

#include "pch.h"
#include "renderer/Vulkan.h"
#include "renderer/FrameCounter.h"
#include "renderer/ResourceDescriptions.h"
#include "renderer/Pipeline.h"

namespace jsr {

	class PipelineBuilder {
	public:

		std::vector<VkPipelineShaderStageCreateInfo>		_shaderStages;
		VkPipelineVertexInputStateCreateInfo				_vertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo				_inputAssembly;
		VkViewport											_viewport;
		VkRect2D											_scissor;
		VkPipelineRasterizationStateCreateInfo				_rasterizer;
		VkPipelineColorBlendAttachmentState					_colorBlendAttachment;
		std::vector<VkPipelineColorBlendAttachmentState>	_colorBlendAttachments;
		VkPipelineMultisampleStateCreateInfo				_multisampling;
		VkPipelineLayout									_pipelineLayout;
		VkPipelineDepthStencilStateCreateInfo				_depthStencil;
		VkPipelineDynamicStateCreateInfo					_dynamicStates;
		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
	};

	class VulkanRenderer;
	struct VulkanDevice;
	class VulkanPipeline {
	public:
		enum Type { Graphics, Compute };
		VulkanPipeline();
		VulkanPipeline(VulkanDevice* device, VulkanRenderer* renderer, VkDescriptorPool pool);
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
		virtual ~VulkanPipeline();
	protected:
		virtual void shutdown();
		VulkanDevice* m_device;
		VulkanRenderer* m_engine;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VkDescriptorSetLayout m_passLayout;
		VkDescriptorSetLayout m_perObjectLayout;
		VkDescriptorSet m_perFrameSets[FrameCounter::INFLIGHT_FRAMES];
		VkDescriptorPool m_descriptorPool;
		VkRenderPass m_renderPass;
	};

	struct VkLayoutInfo {
		uint32_t index;
		VkDescriptorSetLayout layout;
	};

	struct VkGraphicsPipeline : public IGraphicsPipeline {
		// Inherited via GraphicsPipeline
		bool compile() override;

		std::vector<VkLayoutInfo> precompLayouts;

		VulkanRenderer* renderer = nullptr;
		VulkanDevice* device = nullptr;

		VkDevice vulkanDevice = VK_NULL_HANDLE;
		VkPipeline vulkanPipeline = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkGraphicsPipeline(VulkanRenderer *rd, VulkanDevice *dev);
	};
}