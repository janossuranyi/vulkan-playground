#include "DepthPassTechnic.h"
#include "renderer/VulkanRenderer.h"
#include "renderer/Vertex.h"
#include "jsrlib/jsr_resources.h"
#include "jsrlib/jsr_logger.h"

VkResult jsr::DepthPassTechnic::createPipeline(const GraphichPipelineShaderBinary& shaders)
{
    if (!m_device || !m_engine) {
        return VK_ERROR_UNKNOWN;
    }

	VkAttachmentDescription depthAttachmentDescr = {};
	depthAttachmentDescr.format = VK_FORMAT_D24_UNORM_S8_UINT;
	depthAttachmentDescr.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachmentDescr.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentDescr.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentDescr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachmentDescr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachmentDescr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachmentDescr.samples = VK_SAMPLE_COUNT_1_BIT;
	
	const VkAttachmentDescription attachments[] = { depthAttachmentDescr };

	VkAttachmentReference depthRef = {};
	depthRef.attachment = 0;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pDepthStencilAttachment = &depthRef;

	VkSubpassDependency dependencyZ = {};
	dependencyZ.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencyZ.dstSubpass = 0;
	dependencyZ.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencyZ.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencyZ.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencyZ.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency dependencies[] = { dependencyZ };

	VkRenderPassCreateInfo passCreateInfo = {};
	passCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	passCreateInfo.attachmentCount = 1;
	passCreateInfo.pAttachments = attachments;
	passCreateInfo.subpassCount = 1;
	passCreateInfo.pSubpasses = &subpass;
	passCreateInfo.dependencyCount = 1;
	passCreateInfo.pDependencies = dependencies;
	VK_CHECK(vkCreateRenderPass(m_device->vkDevice, &passCreateInfo, nullptr, &m_renderPass));

	VkDescriptorSetLayoutBinding data_binding[] = {
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1)
	};
	VkDescriptorSetLayoutCreateInfo data_layout = {};
	data_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	data_layout.bindingCount = 2;
	data_layout.pBindings = data_binding;
	VK_CHECK(vkCreateDescriptorSetLayout(m_device->vkDevice, &data_layout, nullptr, &m_perFrameLayout));

	VkDescriptorSetLayout layouts[] = { m_engine->getGlobalVarsLayout(), m_engine->getGlobalTexturesLayout(), m_perFrameLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
	pipelineLayoutInfo.setLayoutCount = 3;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	VkResult vk_result = vkCreatePipelineLayout(m_device->vkDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
	VK_CHECK(vk_result);

	auto module_vert = shaders.vertexSPIRV;
	auto module_frag = shaders.fragmentSPIRV;

	VkShaderModuleCreateInfo shaderModules[2] = {};
	VkShaderModule modules[2];
	shaderModules[0].sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModules[0].codeSize = module_vert.size();
	shaderModules[0].pCode = (uint32_t*)module_vert.data();
	shaderModules[1].sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModules[1].codeSize = module_frag.size();
	shaderModules[1].pCode = (uint32_t*)module_frag.data();
	VK_CHECK(vkCreateShaderModule(*m_device, &shaderModules[0], nullptr, &modules[0]));
	VK_CHECK(vkCreateShaderModule(*m_device, &shaderModules[1], nullptr, &modules[1]));
	VkPipelineShaderStageCreateInfo stages[2];
	stages[0] = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, modules[0]);
	stages[1] = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, modules[1]);

	const auto vertexInputDescr = Vertex::vertex_input_description_position_only();
	VkPipelineVertexInputStateCreateInfo vertexInput = vkinit::vertex_input_state_create_info();
	vertexInput.vertexBindingDescriptionCount = (uint32_t)vertexInputDescr.bindings.size();
	vertexInput.pVertexBindingDescriptions = vertexInputDescr.bindings.data();
	vertexInput.vertexAttributeDescriptionCount = (uint32_t)vertexInputDescr.attributes.size();
	vertexInput.pVertexAttributeDescriptions = vertexInputDescr.attributes.data();
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	VkPipelineViewportStateCreateInfo viewportState = {};

	VkViewport viewport = { 0.0f,0.0f,1.0f,1.0f,0.0f,1.0f };
	VkRect2D scissor = {};
	scissor.extent = { 1,1 };
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;
	VkPipelineRasterizationStateCreateInfo rasterization = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo multisample = vkinit::multisampling_state_create_info();
	VkPipelineDepthStencilStateCreateInfo depthState = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = vkinit::color_blend_attachment_state();
	colorBlendAttachmentState.colorWriteMask = 0;

	VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterization;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pDepthStencilState = &depthState;
	pipelineInfo.pColorBlendState = nullptr;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = m_renderPass;

	VK_CHECK(vkCreateGraphicsPipelines(*m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));

	vkDestroyShaderModule(*m_device, modules[0], nullptr);
	vkDestroyShaderModule(*m_device, modules[1], nullptr);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_perFrameLayout;

	for (uint32_t i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i) {
		VK_CHECK(vkAllocateDescriptorSets(m_device->vkDevice, &allocInfo, &m_perFrameSets[i]));
	}

	m_perObjectLayout = VK_NULL_HANDLE;

	jsrlib::Info("DepthPassTechnic: VkPipeline created %p", m_pipeline);

	return VK_SUCCESS;
}

std::vector<VkClearValue> jsr::DepthPassTechnic::getClearValues() const
{
	VkClearValue zclear = {};
	zclear.depthStencil.depth = 1.0f;

	return std::vector<VkClearValue>{zclear};
}
