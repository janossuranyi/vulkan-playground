#include "pipeline.h"
#include "jsrlib/jsr_logger.h"
#include "spirv_utils.h"
#include "vk_descriptors.h"
#include "VulkanInitializers.hpp"
#include "vkcheck.h"
namespace jvk {

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
		
		result = vkCreateGraphicsPipelines(
			device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline);

		if (result != VK_SUCCESS) {
			std::cout << "failed to create pipeline\n";
			return VK_NULL_HANDLE; // failed to create graphics pipeline
		}
		else
		{
			return newPipeline;
		}
	}

	VulkanPipeline& VulkanPipeline::begin()
	{
		for (uint32_t i = 0; i < _resourceBindings.size(); ++i)
			for (uint32_t j = 0; j < _resourceBindings[i].size(); ++j)
			{
				_resourceBindings[i][j].type = BindingType::Empty;
			}
		return *this;
	}

	VulkanPipeline& VulkanPipeline::bind_image(uint32_t index, uint32_t binding, const span<VkDescriptorImageInfo> inf)
	{
		_resourceBindings[index][binding].type = BindingType::Image;
		_resourceBindings[index][binding].image = inf.data();
		_resourceBindings[index][binding].descriptorCount = (uint32_t) inf.size();
		return *this;
	}

	VulkanPipeline& VulkanPipeline::bind_buffer(uint32_t index, uint32_t binding, const span<VkDescriptorBufferInfo> inf)
	{
		_resourceBindings[index][binding].type = BindingType::Buffer;
		_resourceBindings[index][binding].buffer = inf.data();
		_resourceBindings[index][binding].descriptorCount = (uint32_t) inf.size();
		return *this;
	}
	VulkanPipeline& VulkanPipeline::bind_sampler(uint32_t index, uint32_t binding, const span<VkDescriptorImageInfo> inf)
	{
		_resourceBindings[index][binding].type = BindingType::Sampler;
		_resourceBindings[index][binding].image = inf.data();
		_resourceBindings[index][binding].descriptorCount = (uint32_t) inf.size();
		return *this;
	}
	void VulkanPipeline::set_name(const std::string& name)
	{
		_name = name;
	}
	GraphicsPipeline::GraphicsPipeline(Device* dev, const GraphicsShaderInfo& shaderInfo, vkutil::DescriptorManager* descMgr) :
		_shaders(),
		_paralellCmdBuffers(),
		_device(dev),
		_descMgr(descMgr)
	{
		_descriptorSetsInfo = {};
		_pipelineLayout = VK_NULL_HANDLE;
		_pipeline = VK_NULL_HANDLE;

		auto merged_sets = jvk::extract_descriptor_set_layout_data({ shaderInfo.vert,shaderInfo.frag });
		
		_descriptorSetCount = static_cast<uint32_t>( merged_sets.size() );

		for (const auto& it_set : merged_sets) {
			auto& xset = _descriptorSetsInfo[it_set.set_number];
			for (const auto& it_bind : it_set.bindings)
			{
				xset.bindings[xset.bindingCount++] = it_bind;
			}
		}

		_builder._vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		_builder._shaderStages.resize(2);
		_builder._shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		_builder._shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		_builder._shaderStages[0].pName = "main";
		_builder._shaderStages[0].module = shaderInfo.vert->module();
		_builder._shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		_builder._shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		_builder._shaderStages[1].pName = "main";
		_builder._shaderStages[1].module = shaderInfo.frag->module();

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
		for (const auto& dsl : merged_sets)
		{
			descriptorSetLayouts.push_back(_descMgr->create_descriptor_layout(&dsl.create_info));
		}

		VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo((uint32_t)descriptorSetLayouts.size());
		plci.pSetLayouts = descriptorSetLayouts.data();
		VK_CHECK(vkCreatePipelineLayout(*_device, &plci, nullptr, &_builder._pipelineLayout));
		_pipelineLayout = _builder._pipelineLayout;
	}
	GraphicsPipeline::~GraphicsPipeline()
	{
		if (_pipeline) {
			vkDestroyPipeline(*_device, _pipeline, nullptr);
		}
		if (_pipelineLayout) {
			vkDestroyPipelineLayout(*_device, _pipelineLayout, nullptr);
		}
	}
	VkPipelineBindPoint GraphicsPipeline::pipeline_bind_point() const
	{
		return VK_PIPELINE_BIND_POINT_GRAPHICS;
	}
	void GraphicsPipeline::bind(VkCommandBuffer cmd)
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
	}

	GraphicsPipeline& GraphicsPipeline::set_depth_func(VkCompareOp op)
	{
		_depthCompareOp = op;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_polygon_mode(VkPolygonMode polygonMode)
	{
		_polygonMode = polygonMode;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_cull_mode(VkCullModeFlags flags)
	{
		_cullMode = flags;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_front_face(VkFrontFace face)
	{
		_frontFace = face;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_depth_test(bool b)
	{
		_depthTest = b;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_depth_mask(bool b)
	{
		_depthMask = b;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_draw_mode(VkPrimitiveTopology t)
	{
		_drawMode = t;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_sample_count(VkSampleCountFlagBits flags)
	{
		_sampleCount = flags;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_sample_shading(bool enable, float f)
	{
		_sampleShading = enable;
		_minSampleShading = f;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_vertex_input_description(const VertexInputDescription& vinp)
	{
		_builder._vertexInputInfo = vks::initializers::pipelineVertexInputStateCreateInfo(vinp.bindings, vinp.attributes);
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_render_pass(VkRenderPass p)
	{
		_renderPass = p;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::set_next_chain(const void* ptr)
	{
		_builder._pNext = ptr;
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::add_dynamic_state(VkDynamicState s)
	{
		_dynamicStates.push_back(s);
		return *this;
	}
	GraphicsPipeline& GraphicsPipeline::add_attachment_blend_state(const VkPipelineColorBlendAttachmentState& r)
	{
		_builder._colorBlendAttachments.push_back(r);
		return *this;
	}
	void GraphicsPipeline::set_specialization_info(VkShaderStageFlagBits stageBits, const VkSpecializationInfo* info)
	{
		switch (stageBits)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			_builder._shaderStages[0].pSpecializationInfo = info;
			break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			_builder._shaderStages[1].pSpecializationInfo = info;
			break;
		default:
			assert(false);
		}
	}
	VkResult GraphicsPipeline::build_pipeline()
	{
		_builder._rasterizer = vks::initializers::pipelineRasterizationStateCreateInfo(_polygonMode, _cullMode, _frontFace, 0);
		_builder._depthStencil = vks::initializers::pipelineDepthStencilStateCreateInfo(VkBool32(_depthTest), VkBool32(_depthMask), _depthCompareOp);
		_builder._dynamicStates = vks::initializers::pipelineDynamicStateCreateInfo(_dynamicStates);
		_builder._inputAssembly = vks::initializers::pipelineInputAssemblyStateCreateInfo(_drawMode, 0, VK_FALSE);
		_builder._multisampling = vks::initializers::pipelineMultisampleStateCreateInfo(_sampleCount, 0);
		_builder._multisampling.sampleShadingEnable = _sampleShading;
		_builder._multisampling.minSampleShading = _minSampleShading;
		
		_pipeline = _builder.build_pipeline(*_device, _renderPass);

		if (_pipeline && !_name.empty())
		{
			_device->set_object_name((uint64_t)_pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, _name.c_str());
		}

		return _builder.result;
	}
	void VulkanPipeline::prepare()
	{
		// check resources
		for (uint32_t i = 0; i < _descriptorSetCount; ++i)
			for (uint32_t j = 0; j < _descriptorSetsInfo[i].bindingCount; ++j)
			{
				if ( _resourceBindings[ i ][ _descriptorSetsInfo[ i ].bindings[ j ].binding ].type == BindingType::Empty )
				{
					jsrlib::Error("Missing resource binding [%d:%d]", i, j);
				}
			}
	}
}
