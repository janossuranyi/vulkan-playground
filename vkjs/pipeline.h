#ifndef VKJS_PIPELINE_H_
#define VKJS_PIPELINE_H_

#include "vkjs.h"

namespace vkjs {

	struct PipelineBuilder {

		std::vector<VkPipelineShaderStageCreateInfo>		_shaderStages;
		VkPipelineVertexInputStateCreateInfo				_vertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo				_inputAssembly;
		VkPipelineRasterizationStateCreateInfo				_rasterizer;
		std::vector<VkPipelineColorBlendAttachmentState>	_colorBlendAttachments;
		VkPipelineMultisampleStateCreateInfo				_multisampling;
		VkPipelineLayout									_pipelineLayout;
		VkPipelineDepthStencilStateCreateInfo				_depthStencil;
		VkPipelineDynamicStateCreateInfo					_dynamicStates;
		void*												_pNext;
		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
	};

	struct PipelineCommandBuffer {
		VkCommandBuffer cmd;
		VkCommandPool pool;
	};

	class VulkanPipeline {
	public:
		static const uint32_t MAX_DESCRIPTOR_SET_COUNT = 4;
		static const uint32_t MAX_BINDING_COUNT = 32;
		static const uint32_t MAX_PARALELL_CMD_BUFFER_COUNT = 16;

		virtual ~VulkanPipeline() {}
		virtual VulkanPipeline& begin()
		{
			_activeResourceCount = 0;
			return *this;
		}
		virtual VkPipelineBindPoint pipeline_bind_point() const = 0;
		virtual VkResult bind(VkCommandBuffer cmd) = 0;
		
		VulkanPipeline& bind_image(uint32_t index, uint32_t binding, const VkDescriptorImageInfo& inf);
		VulkanPipeline& bind_buffer(uint32_t index, uint32_t binding, const VkDescriptorBufferInfo& inf);
		VulkanPipeline& bind_sampler(uint32_t index, uint32_t binding, const VkDescriptorImageInfo& inf);

		VkPipeline pipeline() const;
		VkPipelineLayout pipeline_layout() const;
		VkDescriptorPool descriptor_pool() const;

	protected:

		void prepare();

		enum DescriptorBindingType { DBT_IMAGE, DBT_SAMPLER, DBT_BUFFER, DBT_COUNT };

		struct Resource {
			uint32_t index;
			uint32_t binding;
			DescriptorBindingType type = DBT_COUNT;
			union {
				VkDescriptorImageInfo image;
				VkDescriptorBufferInfo buffer;
			};
		};

		struct Bindings {
			uint32_t bindingCount = 0;
			std::array<VkDescriptorSetLayoutBinding, MAX_BINDING_COUNT> bindings;
		};

		VkPipeline _pipeline;
		VkPipelineLayout _pipelineLayout;
		VkDescriptorPool _descriptorPool;

		uint32_t _descriptorSetCount = 0;
		uint32_t _paralellCmdBufferCount = 0;
		uint32_t _activeResourceCount = 0;
		std::array<PipelineCommandBuffer, MAX_PARALELL_CMD_BUFFER_COUNT> _paralellCmdBuffers;
		std::array<Bindings, MAX_DESCRIPTOR_SET_COUNT> _bindings;
		std::array<VkDescriptorSetLayout, MAX_DESCRIPTOR_SET_COUNT> _descriptorLayouts;
		std::array<Resource, 32> _resourceBindings;
	};

	class GraphicsPipeline : public VulkanPipeline {
	
	public:
		struct GraphicsShaderInfo {
			std::vector<uint8_t> vertexShaderSPIRV;
			std::vector<uint8_t> fragmentShaderSPIRV;
		};
		
		GraphicsPipeline(const GraphicsShaderInfo& shaders) {};


		// Inherited via VulkanPipeline
		VkPipelineBindPoint pipeline_bind_point() const override;

		VkResult bind(VkCommandBuffer cmd) override;

	};
	class ComputePipeline : public VulkanPipeline {};

	inline VkPipeline VulkanPipeline::pipeline() const
	{
		return _pipeline;
	}
	inline VkPipelineLayout VulkanPipeline::pipeline_layout() const
	{
		return _pipelineLayout;
	}
	inline VkDescriptorPool VulkanPipeline::descriptor_pool() const
	{
		return _descriptorPool;
	}
}
#endif // !VKJS_PIPELINE_H_
