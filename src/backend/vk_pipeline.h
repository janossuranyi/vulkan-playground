#pragma once

#include "pch.h"
#include "vk.h"

namespace vks {

	struct VertexInputDescription {

		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;

		VkPipelineVertexInputStateCreateFlags flags = 0;
	};

	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule);

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

	VertexInputDescription default_vertex_input_description();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);

	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);

	VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

	VkPipelineColorBlendAttachmentState color_blend_attachment_state();

	VkPipelineLayoutCreateInfo pipeline_layout_create_info();

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

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

}