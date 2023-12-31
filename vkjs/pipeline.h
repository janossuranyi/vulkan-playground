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
		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
	};

}
#endif // !VKJS_PIPELINE_H_
