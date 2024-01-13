#include "renderer/VulkanPipeline.h"
#include "renderer/VulkanImage.h"
#include "renderer/Vertex.h"
#include "renderer/FrameCounter.h"
#include "renderer/VulkanInitializers.h"
#include "renderer/Pipeline.h"
#include "jsrlib/jsr_resources.h"

namespace jsr {

	VkBlendFactor blendFactorToVkBlendFactor(BlendFactor x) {
		switch (x) {
		case BlendFactor_One: return VK_BLEND_FACTOR_ONE;
		case BlendFactor_Zero: return VK_BLEND_FACTOR_ZERO;
		case BlendFactor_OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor_SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
		default:
			assert(false);
		}
	}
	VkBlendOp blendOpToVkBlendOp(BlendOp x) {
		switch (x) {
		case BlendOp_Add: return VK_BLEND_OP_ADD;
		case BlendOp_Subtract: return VK_BLEND_OP_SUBTRACT;
		case BlendOp_RevSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendOp_Max: return VK_BLEND_OP_MAX;
		case BlendOp_Min: return VK_BLEND_OP_MIN;
		default:
			assert(false);
		}
	}
	VkCompareOp compareOpToVkCompareOp(CompareOp x) {
		switch (x) {
		case CompareOp_Always: return VK_COMPARE_OP_ALWAYS;
		case CompareOp_EQ: return VK_COMPARE_OP_EQUAL;
		case CompareOp_GT: return VK_COMPARE_OP_GREATER;
		case CompareOp_GTE: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp_LT: return VK_COMPARE_OP_LESS;
		case CompareOp_LTE: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp_Never: return VK_COMPARE_OP_NEVER;
		default:
			assert(false);
		}
	}
	VkFrontFace frontFaceToVkFrontFace(FrontFace x) {
		switch (x) {
		case FrontFace_CCW: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		case FrontFace_CW: return VK_FRONT_FACE_CLOCKWISE;
		default:
			assert(false);
		}
	}
	VkCullModeFlags cullModeToVkCullModeFlags(CullMode x) {
		switch (x) {
		case CullMode_Back: return VK_CULL_MODE_BACK_BIT;
		case CullMode_Front: return VK_CULL_MODE_FRONT_BIT;
		case CullMode_FrontAndBack: return VK_CULL_MODE_FRONT_AND_BACK;
		case CullMode_None: return VK_CULL_MODE_NONE;
		default:
			assert(false);
		}
	}
	VkFormat formatToVkFormat(Format x) {
		switch (x) {
		case Format_R8: return VK_FORMAT_R8_UNORM;
		case Format_R32F: return VK_FORMAT_R32_SFLOAT;
		case Format_D24S8: return VK_FORMAT_D24_UNORM_S8_UINT;
		case Format_D32F: return VK_FORMAT_D32_SFLOAT;
		case Format_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
		case Format_R11G11B10F: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case Format_RG16F: return VK_FORMAT_R16G16_SFLOAT;
		case Format_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case Format_RGBA32F: return VK_FORMAT_R32G32B32_SFLOAT;
		default: assert(false);
		}
	}
	VkAttachmentLoadOp loadOpToVkAttachmentLoadOp(LoadOp x) {
		switch (x) {
		case LoadOp_Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case LoadOp_DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		case LoadOp_Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
		default: assert(false);
		}
	}
	VkAttachmentStoreOp storeOpToVkAttachmentStoreOp(StoreOp x) {
		switch (x) {
		case StoreOp_DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		case StoreOp_Store: return VK_ATTACHMENT_STORE_OP_STORE;
		default: assert(false);
		}
	}
	VkDescriptorType resourceTypeToVkDescriptorType(ResourceType x) {
		switch (x) {
		case ResourceType_Image: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case ResourceType_StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case ResourceType_Texture: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case ResourceType_SSBO: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case ResourceType_SSBO_Dyn: VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		case ResourceType_UBO: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case ResourceType_UBO_Dyn: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		default: assert(false);
		}
	}
	VkPolygonMode polygonModeToVkPolygonMode(PolygonMode x) {
		switch (x) {
		case PolygonMode_Fill: return VK_POLYGON_MODE_FILL;
		case PolygonMode_Line: return VK_POLYGON_MODE_LINE;
		case PolygonMode_Point: return VK_POLYGON_MODE_POINT;
		default:
			assert(false);
		}
	}

	VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
	{
		//make viewport state from our stored viewport and scissor.
		//at the moment we won't support multiple viewports or scissors
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.pNext = nullptr;

		viewportState.viewportCount = 1;
		viewportState.pViewports = &_viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &_scissor;

		//setup dummy color blending. We aren't using transparent objects yet
		//the blending is just "no blend", but we do write to the color attachment
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.pNext = nullptr;

		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		if (_colorBlendAttachments.empty())
		{
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &_colorBlendAttachment;
		}
		else
		{
			colorBlending.attachmentCount = static_cast<uint32_t>( _colorBlendAttachments.size() );
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


	VulkanPipeline::VulkanPipeline() :
		m_device(),
		m_engine(),
		m_descriptorPool(),
		m_perFrameSets(),
		m_passLayout(),
		m_renderPass(),
		m_pipeline(),
		m_pipelineLayout(),
		m_perObjectLayout()
	{
	}
	VulkanPipeline::VulkanPipeline(VulkanDevice* device, VulkanRenderer* renderer, VkDescriptorPool pool) :
		m_device(device),
		m_engine(renderer),
		m_descriptorPool(pool),
		m_perFrameSets(),
		m_passLayout(),
		m_perObjectLayout(),
		m_renderPass(),
		m_pipeline(),
		m_pipelineLayout()
	{
	}
	VkPipeline VulkanPipeline::getPipeline() const
	{
		return m_pipeline;
	}
	void VulkanPipeline::setDevice(VulkanDevice* device)
	{
		this->m_device = device;
	}
	void VulkanPipeline::setEngine(VulkanRenderer* renderer)
	{
		m_engine = renderer;
	}
	void VulkanPipeline::setDescriptorPool(VkDescriptorPool pool)
	{
		m_descriptorPool = pool;
	}
	VkPipelineLayout VulkanPipeline::getPipelineLayout() const
	{
		return m_pipelineLayout;
	}
	VkDescriptorSetLayout VulkanPipeline::getDescriptorSetLayout() const
	{
		return m_passLayout;
	}
	VkDescriptorSetLayout VulkanPipeline::getPerObjectLayout() const
	{
		return m_perObjectLayout;
	}
	VkDescriptorSet VulkanPipeline::getDescriptorSet(uint32_t index) const
	{
		return m_perFrameSets[index % FrameCounter::INFLIGHT_FRAMES];
	}
	VkRenderPass VulkanPipeline::getRenderPass() const
	{
		return m_renderPass;
	}
	VkResult VulkanPipeline::createPipeline(const GraphichPipelineShaderBinary& shaders)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	VkResult VulkanPipeline::createPipeline(const ComputePipelineShaderBinary& shaders)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	VulkanPipeline::~VulkanPipeline()
	{
		shutdown();
	}
	void VulkanPipeline::shutdown()
	{
		if (m_pipeline) {
			vkDestroyPipeline(m_device->vkDevice, m_pipeline, nullptr);
		}

		if (m_pipelineLayout) {
			vkDestroyPipelineLayout(m_device->vkDevice, m_pipelineLayout, nullptr);
		}
#if 0
		if (m_perFrameSets[0]) {
			vkFreeDescriptorSets(m_device->vkDevice, m_descriptorPool, 1, &m_perFrameSets[0]);
			vkFreeDescriptorSets(m_device->vkDevice, m_descriptorPool, 1, &m_perFrameSets[1]);
		}
#endif
		if (m_passLayout) {
			vkDestroyDescriptorSetLayout(m_device->vkDevice, m_passLayout, nullptr);
		}
		if (m_perObjectLayout) {
			vkDestroyDescriptorSetLayout(m_device->vkDevice, m_perObjectLayout, nullptr);
		}

		if (m_renderPass) {
			vkDestroyRenderPass(m_device->vkDevice, m_renderPass, nullptr);
		}
	}

	bool VkGraphicsPipeline::compile()
	{
		PipelineBuilder builder = {};

		VkPipelineViewportStateCreateInfo viewportState = {};

		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = nullptr;
		viewportState.scissorCount = 1;
		viewportState.pScissors = nullptr;

		const VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };
		VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		builder._multisampling = vkinit::multisampling_state_create_info();
		builder._dynamicStates = dynamicState;

		std::vector<VkAttachmentDescription> attachmentDescList(attachments.size());
		std::vector<VkAttachmentReference> colorAttachmentRefs;

		VkAttachmentReference depthRef{};

		bool hasDepth = false;
		for (size_t i(0); i < attachments.size(); ++i)
		{
			const auto& a = attachments[i];
			auto& d = attachmentDescList[i];
			d.format = formatToVkFormat(a.format);
			d.finalLayout = VulkanImage::isDepthFormat(d.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			d.initialLayout = VulkanImage::isDepthFormat(d.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
			d.loadOp = loadOpToVkAttachmentLoadOp(a.loadOp);
			d.storeOp = storeOpToVkAttachmentStoreOp(a.storeOp);
			d.flags = 0;
			d.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			d.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			d.samples = VK_SAMPLE_COUNT_1_BIT;


			if (VulkanImage::isDepthFormat(d.format)) {
				assert(!hasDepth);
				depthRef.attachment = i;
				depthRef.layout = d.finalLayout;
				hasDepth = true;
			}
			else 
			{
				VkAttachmentReference ref{};
				ref.attachment = i;
				ref.layout = d.finalLayout;
				colorAttachmentRefs.push_back(ref);
			}
		}

		VkSubpassDescription subpass = {};
		subpass.colorAttachmentCount = uint32_t( colorAttachmentRefs.size() );
		subpass.pColorAttachments = colorAttachmentRefs.data();
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;
		
		std::vector<VkSubpassDependency> subpassDeps;
		if (!colorAttachmentRefs.empty())
		{
			VkSubpassDependency dependency = {};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			subpassDeps.push_back(dependency);
		}
		if (hasDepth)
		{
			VkSubpassDependency dependency = {};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			subpassDeps.push_back(dependency);
		}

		std::vector<VkDescriptorSetLayout> layouts(base.resources.size() + precompLayouts.size());

		for (size_t set(0); set < base.resources.size(); ++set) {
			std::vector<VkDescriptorSetLayoutBinding> resource_bindings;
			for (size_t bind(0); bind < base.resources[set].bindings.size(); ++bind) {
				const auto& binding = base.resources[set].bindings[bind];
				VkDescriptorSetLayoutBinding e = {};
				e.binding = binding.binding;
				e.descriptorCount = 1;
				e.descriptorType = resourceTypeToVkDescriptorType(binding.type);
				e.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
				resource_bindings.push_back(e);
			}

			VkDescriptorSetLayoutCreateInfo data_layout = {};
			data_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			data_layout.bindingCount = static_cast<uint32_t>(resource_bindings.size());
			data_layout.pBindings = resource_bindings.data();
			assert(layouts[base.resources[set].index] == VK_NULL_HANDLE);
			VK_CHECK(vkCreateDescriptorSetLayout(vulkanDevice, &data_layout, nullptr, &layouts[base.resources[set].index]));
		}

		for (const auto& e : precompLayouts) {
			assert(layouts[e.index] == VK_NULL_HANDLE);
			layouts[e.index] = e.layout;
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
		pipelineLayoutInfo.setLayoutCount = uint32_t(layouts.size());
		pipelineLayoutInfo.pSetLayouts = layouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		VkResult vk_result = vkCreatePipelineLayout(vulkanDevice, &pipelineLayoutInfo, nullptr, &this->layout);
		VK_CHECK(vk_result);
		builder._pipelineLayout = this->layout;

		VkRenderPassCreateInfo passCreateInfo = {};
		passCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		passCreateInfo.attachmentCount = uint32_t(attachmentDescList.size());
		passCreateInfo.pAttachments = attachmentDescList.data();
		passCreateInfo.subpassCount = 1;
		passCreateInfo.pSubpasses = &subpass;
		passCreateInfo.dependencyCount = uint32_t(subpassDeps.size());
		passCreateInfo.pDependencies = subpassDeps.data();
		VK_CHECK(vkCreateRenderPass(vulkanDevice, &passCreateInfo, nullptr, &renderPass));
		// rasterizer

		VkPipelineRasterizationStateCreateInfo rasterization = vkinit::rasterization_state_create_info(polygonModeToVkPolygonMode(rasterizerState.polygonMode));
		rasterization.cullMode = cullModeToVkCullModeFlags(rasterizerState.cullMode);
		rasterization.frontFace = frontFaceToVkFrontFace(rasterizerState.frontFace);
		builder._rasterizer = rasterization;

		const VkPipelineDepthStencilStateCreateInfo depthState = vkinit::depth_stencil_create_info(depthTest, depthWrite, compareOpToVkCompareOp(depthCompareOp));
		builder._depthStencil = depthState;

		std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
		for (size_t i(0); i < attachments.size(); ++i)
		{

			if (attachments[i].isDepth) { continue; }
			VkColorComponentFlags colorMask = 0;
			if (attachments[i].writeMask.r)	colorMask |= VK_COLOR_COMPONENT_R_BIT;
			if (attachments[i].writeMask.g)	colorMask |= VK_COLOR_COMPONENT_G_BIT;
			if (attachments[i].writeMask.b)	colorMask |= VK_COLOR_COMPONENT_B_BIT;
			if (attachments[i].writeMask.a)	colorMask |= VK_COLOR_COMPONENT_A_BIT;

			VkPipelineColorBlendAttachmentState colorBlendAttachmentState = vkinit::color_blend_attachment_state();
			colorBlendAttachmentState.colorWriteMask = colorMask;
			if (attachments[i].blendState) {
				colorBlendAttachmentState.blendEnable = VK_TRUE;
				colorBlendAttachmentState.alphaBlendOp = blendOpToVkBlendOp(attachments[i].blendState.blendOp);
				colorBlendAttachmentState.colorBlendOp = blendOpToVkBlendOp(attachments[i].blendState.blendOp);
				colorBlendAttachmentState.srcColorBlendFactor = blendFactorToVkBlendFactor(attachments[i].blendState.srcFactor);
				colorBlendAttachmentState.dstColorBlendFactor = blendFactorToVkBlendFactor(attachments[i].blendState.dstFactor);
				colorBlendAttachmentState.srcAlphaBlendFactor = blendFactorToVkBlendFactor(attachments[i].blendState.srcFactor);
				colorBlendAttachmentState.dstAlphaBlendFactor = blendFactorToVkBlendFactor(attachments[i].blendState.dstFactor);
			}
			colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
		}
		builder._colorBlendAttachments = std::move(colorBlendAttachmentStates);

		VkShaderModule vert, frag;
		std::vector<uint8_t> vertSPIRV,fragSPIRV;
		vertSPIRV = jsrlib::Filesystem::root.ReadFile(stages.vert.source);
		fragSPIRV = jsrlib::Filesystem::root.ReadFile(stages.frag.source);
		assert(!vertSPIRV.empty() && !fragSPIRV.empty());

		VkShaderModuleCreateInfo vertCI, fragCI;

		vertCI = vkinit::shader_module_create_info(vertSPIRV.size(), (uint32_t*)vertSPIRV.data());
		fragCI = vkinit::shader_module_create_info(fragSPIRV.size(), (uint32_t*)fragSPIRV.data());
		VK_CHECK(vkCreateShaderModule(vulkanDevice, &vertCI, nullptr, &vert));
		VK_CHECK(vkCreateShaderModule(vulkanDevice, &fragCI, nullptr, &frag));
		VkPipelineShaderStageCreateInfo stages[2];
		stages[0] = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vert);
		stages[1] = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, frag);
		builder._shaderStages.push_back(stages[0]);
		builder._shaderStages.push_back(stages[1]);



		vkDestroyShaderModule(vulkanDevice, vert, nullptr);
		vkDestroyShaderModule(vulkanDevice, frag, nullptr);

		return true;
	}

	VkGraphicsPipeline::VkGraphicsPipeline(VulkanRenderer* rd, VulkanDevice* dev)
	{
		renderer = rd;
		device = dev;
		vulkanDevice = dev->vkbDevice;
	}

}