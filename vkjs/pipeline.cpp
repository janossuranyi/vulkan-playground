#include "pipeline.h"
namespace vkjs {

	VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
	{
		//make viewport state from our stored viewport and scissor.
		//at the moment we won't support multiple viewports or scissors
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.pNext = nullptr;

		viewportState.viewportCount = 1;
		viewportState.pViewports = nullptr;
		viewportState.scissorCount = 1;
		viewportState.pScissors = nullptr;

		//setup dummy color blending. We aren't using transparent objects yet
		//the blending is just "no blend", but we do write to the color attachment
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.pNext = nullptr;

		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		if (_colorBlendAttachments.empty())
		{
			colorBlending.attachmentCount = 0;
			colorBlending.pAttachments = nullptr;
		}
		else
		{
			colorBlending.attachmentCount = static_cast<uint32_t>(_colorBlendAttachments.size());
			colorBlending.pAttachments = _colorBlendAttachments.data();
		}

		//build the actual pipeline
		//we now use all of the info structs we have been writing into into this one to create the pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = _pNext;

		pipelineInfo.stageCount = static_cast<unsigned>(_shaderStages.size());
		pipelineInfo.pStages = _shaderStages.data();
		pipelineInfo.pVertexInputState = &_vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &_inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &_rasterizer;
		pipelineInfo.pMultisampleState = &_multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = _pipelineLayout;
		pipelineInfo.renderPass = pass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.pDepthStencilState = &_depthStencil;
		pipelineInfo.pDynamicState = &_dynamicStates;
		//it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
		VkPipeline newPipeline;
		if (vkCreateGraphicsPipelines(
			device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
			std::cout << "failed to create pipeline\n";
			return VK_NULL_HANDLE; // failed to create graphics pipeline
		}
		else
		{
			return newPipeline;
		}
	}

	VulkanPipeline& VulkanPipeline::bind_image(uint32_t index, uint32_t binding, const VkDescriptorImageInfo& inf)
	{
		assert(_activeResourceCount < _resourceBindings.size());

		_resourceBindings[_activeResourceCount].index = index;
		_resourceBindings[_activeResourceCount].binding = binding;
		_resourceBindings[_activeResourceCount].type = DBT_IMAGE;
		_resourceBindings[_activeResourceCount].image = inf;
		++_activeResourceCount;

		return *this;
	}
	VulkanPipeline& VulkanPipeline::bind_buffer(uint32_t index, uint32_t binding, const VkDescriptorBufferInfo& inf)
	{
		assert(_activeResourceCount < _resourceBindings.size());

		_resourceBindings[_activeResourceCount].index = index;
		_resourceBindings[_activeResourceCount].binding = binding;
		_resourceBindings[_activeResourceCount].type = DBT_BUFFER;
		_resourceBindings[_activeResourceCount].buffer = inf;
		++_activeResourceCount;

		return *this;
	}
	VulkanPipeline& VulkanPipeline::bind_sampler(uint32_t index, uint32_t binding, const VkDescriptorImageInfo& inf)
	{
		assert(_activeResourceCount < _resourceBindings.size());

		_resourceBindings[_activeResourceCount].index = index;
		_resourceBindings[_activeResourceCount].binding = binding;
		_resourceBindings[_activeResourceCount].type = DBT_SAMPLER;
		_resourceBindings[_activeResourceCount].image = inf;
		++_activeResourceCount;

		return *this;
	}
	VkPipelineBindPoint GraphicsPipeline::pipeline_bind_point() const
	{
		return VK_PIPELINE_BIND_POINT_GRAPHICS;
	}
	VkResult GraphicsPipeline::bind(VkCommandBuffer cmd)
	{
		return VK_ERROR_UNKNOWN;
	}
	void VulkanPipeline::prepare()
	{
		Resource* mapping[MAX_DESCRIPTOR_SET_COUNT][MAX_BINDING_COUNT] = {};

		for (uint32_t i = 0; i < _activeResourceCount; ++i)
		{
			Resource* rc = &this->_resourceBindings[i];
			mapping[rc->index][rc->binding] = rc;
		}
		for (uint32_t set = 0; set < _descriptorSetCount; ++set)
		{
			size_t hash = 0;
			const Bindings& b = _bindings[set];
			for (uint32_t binding = 0; binding < b.bindingCount; ++binding)
			{
				const Resource* rc = mapping[set][binding];
				if (rc == nullptr)
				{
					throw "Missing pipeline resource";
				}
				hash = ((size_t)rc->type << 63) | ((size_t)rc->index << 62) | ((size_t)rc->binding << 60);

			}

		}
	}
}
