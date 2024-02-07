#include "renderer/VulkanRenderer.h"
#include "renderer/VulkanBarrier.h"
#include "renderer/VulkanPipeline.h"
#include "renderer/Vertex.h"
#include "renderer/FrameCounter.h"
#include "renderer/ResourceDescriptions.h"
#include "jsrlib/jsr_resources.h"
#include "jsrlib/jsr_logger.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#define asuint(x) static_cast<uint32_t>(x)

namespace jsr {

	static void check_vk_result(VkResult err)
	{
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	std::vector<char> toCharArray(size_t size, const void* data)
	{
		std::vector<char> buf(size);
		memcpy(buf.data(), data, size);
		return buf;
	}

	VulkanRenderer::VulkanRenderer(VulkanDevice* device, VulkanSwapchain* swapchain, int numDrawThreads) : m_device(device), m_swapchain(swapchain), m_numThreads(numDrawThreads), gpass_data()
	{
		init();
	}

	VulkanRenderer::~VulkanRenderer()
	{
		shutdown();
	}

	void VulkanRenderer::setGraphicsPass(const GraphicsPassInfo& passInfo)
	{
		RenderPassInfo inf;
		inf.type = RenderPassType::Graphics;
		inf.index = asuint(m_activeGraphicsPasses.size());
		m_activeGraphicsPasses.push_back(passInfo);
		m_activePasses.push_back(inf);
	}

	void VulkanRenderer::setComputePass(const ComputePassInfo& passInfo)
	{
		RenderPassInfo inf;
		inf.type = RenderPassType::Compute;
		inf.index = asuint(m_activeComputePasses.size());
		m_activeComputePasses.push_back(passInfo);
		m_activePasses.push_back(inf);
	}

	void VulkanRenderer::sendDrawCmd(uint32_t numVerts, handle::GraphicsPipeline hPipe, uint32_t threadId, const void* pushConstants)
	{
		auto& pipe = m_graphicsPipelines[hPipe.index];
		const uint32_t offset = m_numThreads * FrameCounter::getFrameIndex();
		VkCommandBuffer cmd = pipe.drawCommandBuffers[offset + threadId];

		if (pushConstants && pipe.vkPipeline->getPushConstantsSize()) {
			vkCmdPushConstants(cmd, pipe.vkPipeline->getPipelineLayout(),
				VK_SHADER_STAGE_ALL_GRAPHICS, 0, pipe.vkPipeline->getPushConstantsSize(), pushConstants);
		}

		vkCmdDraw(cmd, numVerts, 1, 0, 0);
	}

	void VulkanRenderer::drawMeshes(const std::vector<DrawMeshDescription>& descr, uint32_t threadId, const void* pushConstants)
	{
		size_t pc_offset = 0;
		for (size_t drawIdx = 0; drawIdx < descr.size(); ++drawIdx)
		{
			const auto& pipeline = m_graphicsPipelines[ descr[ drawIdx ].pipeline.index ];
			const uint32_t offset = m_numThreads * FrameCounter::getFrameIndex();
			VkCommandBuffer cmd = pipeline.drawCommandBuffers[offset + threadId];
			if (pushConstants && pipeline.vkPipeline->getPushConstantsSize()) {

				const char* ptr = ((const char*)pushConstants) + pc_offset;

				vkCmdPushConstants(cmd, pipeline.vkPipeline->getPipelineLayout(),
					VK_SHADER_STAGE_ALL_GRAPHICS, 0, pipeline.vkPipeline->getPushConstantsSize(), ptr);

				pc_offset += pipeline.vkPipeline->getPushConstantsSize();
			}

			if (!handle::is_valid(m_mergedMesh) || descr[drawIdx].mesh.index != m_mergedMesh.index)
			{
				const auto& MESH = m_meshes[descr[drawIdx].mesh.index];
				const VkBuffer vBuffer = MESH.vertexBuffer->handle;
				const VkBuffer iBuffer = MESH.indexBuffer->handle;
				const VkDeviceSize vOffset = 0ULL;

				vkCmdBindVertexBuffers(cmd, 0, 1, &vBuffer, &vOffset);
				vkCmdBindIndexBuffer(cmd, iBuffer, 0, MESH.indexType);
			}

			if (handle::is_valid(descr[drawIdx].meshResources))
			{
				vkCmdBindDescriptorSets(
					cmd,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipeline.vkPipeline->getPipelineLayout(),
					3u,
					1u,
					&m_descriptorSets[descr[drawIdx].meshResources.index].set,
					0u,
					nullptr);
			}

			vkCmdDrawIndexed(cmd,
				descr[drawIdx].vc.indexCount, 
				descr[drawIdx].instanceCount, 
				descr[drawIdx].vc.firstIndex, 
				descr[drawIdx].vc.baseVertex, 
				descr[drawIdx].firstInstance);
		}
	}

	bool VulkanRenderer::prepareNextFrame()
	{
		using namespace std::chrono;

		VkDevice device = m_device->vkDevice;

		const uint32_t frameNumber	= FrameCounter::nextFrame(); // call this only one place
		const uint32_t currentFrame	= FrameCounter::getFrameIndex();		

		auto& FRAME = m_framedata[currentFrame];

		auto extent = m_swapchain->getSwapChainExtent();
		gpass_data.uFramenumber = frameNumber;
		gpass_data.vDisplaySize.x = float(extent.width);
		gpass_data.vDisplaySize.y = float(extent.height);
		gpass_data.vDisplaySize.z = 1.0f / gpass_data.vDisplaySize.x;
		gpass_data.vDisplaySize.w = 1.0f / gpass_data.vDisplaySize.y;

		auto t = steady_clock::now();
		VK_CHECK(vkWaitForFences(device, 1, &FRAME.renderFence, true, (uint64_t)(1e9)));
		auto dt = steady_clock::now() - t;
		
		//if (dt.count() >= 17 * 1e6) jsrlib::Info(">16ms: %d, %f", currentFrame, dt.count() / 1e6);

		SwapchainImage swapchain_image;
		VkResult acqResult = m_swapchain->acquaireImage(FRAME.presentSemaphore, swapchain_image);

		if (acqResult == VK_SUBOPTIMAL_KHR || acqResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_swapchain->recreateSwapchain();
			freeImage(m_zBuffer); m_zBuffer = handle::Image{};
			init_depth_image();
			init_swapchain_images();

			m_windowResized = false;
			return false;
		}
		else if (acqResult != VK_SUCCESS) {
			jsrlib::Error("*** SwapChain acquaireImage failed ! ***");
			return false;
		}

		cleanupTemporaryFrameBuffers(&FRAME);
		m_activePasses.clear();
		m_activeGraphicsPasses.clear();
		m_activeComputePasses.clear();
		m_deferredUniformBufferFills.clear();
		m_deferredStorageBufferFills.clear();
		updateUniforms(FRAME);

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		return true;
	}

	VkDescriptorSetLayout VulkanRenderer::getGlobalVarsLayout() const
	{
		return m_globalShaderVarsLayout;
	}

	VkDescriptorSetLayout VulkanRenderer::getGlobalTexturesLayout() const
	{
		return m_gloalTexturesLayout;
	}

	void VulkanRenderer::init()
	{
		m_graphicQueue = m_device->getGraphicsQueue();
		VkDevice device = m_device->vkbDevice;

		auto defpoolCI = vkinit::command_pool_create_info(m_device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VK_CHECK(vkCreateCommandPool(device, &defpoolCI, nullptr, &m_defaultCommandPool));

		// init per-frame data
		for (size_t i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i) {
			// create fence
			m_device->createFence(VK_FENCE_CREATE_SIGNALED_BIT, &m_framedata[i].renderFence);
			// create semaphores
			m_device->createSemaphore(0, &m_framedata[i].renderSemaphore);
			m_device->createSemaphore(0, &m_framedata[i].presentSemaphore);
			auto framepoolCI = vkinit::command_pool_create_info(m_device->queueFamilyIndices.graphics, 0);
			VK_CHECK(vkCreateCommandPool(device, &framepoolCI, nullptr, &m_framedata[i].commandPool));
			m_device->createCommandBuffer(m_framedata[i].commandPool, 0, false, &m_framedata[i].commandBuffer, false);

			/* init global uniform buffer */
			m_framedata[i].globalVarsUbo = createUniformBuffer(UniformBufferDescription{ sizeof(gpass_data),&gpass_data });
		}

		for (int i = 0; i < m_numThreads; ++i)
		{
			auto poolCI = vkinit::command_pool_create_info(m_device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
			m_drawCommandPools.push_back({});
			VK_CHECK(vkCreateCommandPool(device, &poolCI, nullptr, &m_drawCommandPools.back()));
		}

		m_stagingBuffer.reset(new VulkanBuffer(m_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 4U * 1024U * 1024U, VMA_MEMORY_USAGE_CPU_ONLY));

		init_swapchain_images();
		init_samplers();
		init_gvars_descriptor_set();
		init_global_texture_descriptor_set();
		init_descriptor_pool();
		init_default_images();
		init_depth_image();
		init_imgui();
	}

	void VulkanRenderer::init_swapchain_images()
	{
		m_swapchainImages.resize(m_swapchain->getImageCount());
		for (int i = 0; i < m_swapchain->getImageCount(); ++i)
		{
			m_swapchainImages[i].reset(new VulkanImage(m_device, m_swapchain, i));
		}
	}

	void VulkanRenderer::init_samplers()
	{
		/* init global samplers */
		vkinit::SamplerBuilder sb = {};
		sb.addressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
			.anisotropyEnable(true)
			.maxAnisotropy(8.0f)
			.filter(VK_FILTER_LINEAR)
			.mipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);

		VkSamplerCreateInfo samplerCI = sb.build();
		m_globalSamplers.anisotropeLinearRepeat = createSampler(samplerCI);

		sb.addressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE); samplerCI = sb.build();
		m_globalSamplers.anisotropeLinearEdgeClamp = createSampler(samplerCI);

		sb.filter(VK_FILTER_NEAREST).mipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST).anisotropyEnable(false); samplerCI = sb.build();
		m_globalSamplers.nearestEdgeClamp = createSampler(samplerCI);
	}

	void VulkanRenderer::init_gvars_descriptor_set()
	{
		VkDevice device = m_device->vkDevice;
		VkShaderStageFlags stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;

		const std::vector<VkDescriptorSetLayoutBinding> bindings = {
			vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stageFlags, 0),
			vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_SAMPLER, stageFlags, 1),
			vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_SAMPLER, stageFlags, 2),
			vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_SAMPLER, stageFlags, 3)
		};

		VkDescriptorSetLayoutCreateInfo layoutCI = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutCI.bindingCount	= static_cast<uint32_t>(bindings.size());
		layoutCI.pBindings		= bindings.data();
		layoutCI.flags			= 0;

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &m_globalShaderVarsLayout));

		const std::vector<VkDescriptorPoolSize> poolSizes = {
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 3}
		};

		VkDescriptorPoolCreateInfo poolCI = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCI.maxSets = 1;
		poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolCI.pPoolSizes = poolSizes.data();

		for (int i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i)
		{
			auto& FRAME = m_framedata[i];

			VK_CHECK(vkCreateDescriptorPool(device, &poolCI, nullptr, &FRAME.globalShaderVarsPool));

			VkDescriptorSetAllocateInfo allocinf = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocinf.descriptorPool		= FRAME.globalShaderVarsPool;
			allocinf.descriptorSetCount = 1;
			allocinf.pSetLayouts		= &m_globalShaderVarsLayout;
			VK_CHECK(vkAllocateDescriptorSets(device, &allocinf, &FRAME.globalShaderVarsSet));

			std::vector<VkWriteDescriptorSet> writeset;
			VkDescriptorBufferInfo bufferInfo = {};
			bufferInfo = m_uniformBuffers[FRAME.globalVarsUbo.index]->descriptor;
			writeset.push_back(vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FRAME.globalShaderVarsSet, &bufferInfo, 0));

			VkDescriptorImageInfo samplerInfo[3] = {};
			samplerInfo[0].sampler = m_samplers[m_globalSamplers.anisotropeLinearRepeat.index].sampler;
			samplerInfo[1].sampler = m_samplers[m_globalSamplers.anisotropeLinearEdgeClamp.index].sampler;
			samplerInfo[2].sampler = m_samplers[m_globalSamplers.nearestEdgeClamp.index].sampler;
			writeset.push_back(vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLER, FRAME.globalShaderVarsSet, &samplerInfo[0], 1, 0));
			writeset.push_back(vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLER, FRAME.globalShaderVarsSet, &samplerInfo[1], 2, 0));
			writeset.push_back(vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLER, FRAME.globalShaderVarsSet, &samplerInfo[2], 3, 0));

			vkUpdateDescriptorSets(device, static_cast<unsigned>(writeset.size()), writeset.data(), 0, nullptr);
		}
	}

	void VulkanRenderer::init_global_texture_descriptor_set()
	{
		auto device = m_device->vkDevice;

		std::vector<VkDescriptorSetLayoutBinding> textureArrayBinding(1);
		textureArrayBinding[0].binding = 0;
		textureArrayBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		textureArrayBinding[0].descriptorCount = maxTextureCount;
		textureArrayBinding[0].stageFlags = VK_SHADER_STAGE_ALL;
		textureArrayBinding[0].pImmutableSamplers = nullptr;

		const VkDescriptorBindingFlags binding_bits[1] = { 
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | 
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | 
			VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
		};

		VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo{};
		flagInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		flagInfo.pNext = nullptr;
		flagInfo.bindingCount = 1;
		flagInfo.pBindingFlags = binding_bits;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = &flagInfo;
		layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		layoutInfo.bindingCount = (unsigned) textureArrayBinding.size();
		layoutInfo.pBindings = textureArrayBinding.data();

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_gloalTexturesLayout));

		VkDescriptorPoolSize poolSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		poolSize.descriptorCount = maxTextureCount;

		VkDescriptorPoolCreateInfo poolInfo;
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		poolInfo.maxSets = 1;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;

		VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_globalTexturesPool));

		VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo = {};
		variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
		variableDescriptorCountInfo.pNext = nullptr;
		variableDescriptorCountInfo.descriptorSetCount = 1;
		variableDescriptorCountInfo.pDescriptorCounts = &maxTextureCount;

		VkDescriptorSetAllocateInfo setInfo = {};
		setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		setInfo.pNext = &variableDescriptorCountInfo;
		setInfo.descriptorPool = m_globalTexturesPool;
		setInfo.descriptorSetCount = 1;
		setInfo.pSetLayouts = &m_gloalTexturesLayout;
		VK_CHECK(vkAllocateDescriptorSets(device, &setInfo, &m_globalTexturesArraySet));
	}

	void VulkanRenderer::init_default_images()
	{
		gpass_data.uFlatNormalIndex = getImageGlobalIndex(m_flatDepthImage);
	}

	void VulkanRenderer::init_descriptor_pool()
	{
		const uint32_t maxSets = 100;
		const std::vector<VkDescriptorPoolSize> poolSizes = {
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5 * maxSets},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * maxSets},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5 * maxSets},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * maxSets},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 5 * maxSets }
		};

		VkDescriptorPoolCreateInfo poolCI = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCI.maxSets = maxSets;
		poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolCI.pPoolSizes = poolSizes.data();

		VK_CHECK(vkCreateDescriptorPool(m_device->vkDevice, &poolCI, nullptr, &m_defaultDescriptorPool));
	}

	void VulkanRenderer::init_depth_image()
	{
#if 0
		VkImageCreateInfo zinf = {};
		zinf.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		zinf.arrayLayers = 1;
		zinf.format = VK_FORMAT_D24_UNORM_S8_UINT;
		zinf.imageType = VK_IMAGE_TYPE_2D;
		zinf.mipLevels = 1;
		zinf.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		zinf.tiling = VK_IMAGE_TILING_OPTIMAL;
		auto extent = m_swapchain->getSwapChainExtent();
		zinf.extent.depth = 1;
		zinf.extent.width = extent.width;
		zinf.extent.height = extent.height;
		zinf.flags = 0;
		zinf.samples = VK_SAMPLE_COUNT_1_BIT;
		zinf.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		if (m_depthImageView) {
			vkDestroyImageView(*m_device, m_depthImageView, nullptr);
		}

		if (m_depthImage) {
			vmaDestroyImage(m_device->allocator, m_depthImage, m_depthImageMemory);
		}

		VmaAllocationCreateInfo allocInf = {};
		allocInf.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		VK_CHECK(vmaCreateImage(m_device->allocator, &zinf, &allocInf, &m_depthImage, &m_depthImageMemory, nullptr));

		VkImageSubresourceRange subres = {};
		subres.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT| VK_IMAGE_ASPECT_STENCIL_BIT;
		subres.baseArrayLayer = 0;
		subres.baseMipLevel = 0;
		subres.layerCount = 1;
		subres.levelCount = 1;

		VkImageViewCreateInfo viewInf = {};
		viewInf.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInf.format = VK_FORMAT_D24_UNORM_S8_UINT;
		viewInf.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInf.image = m_depthImage;
		viewInf.subresourceRange = subres;
		viewInf.flags = 0;
		VK_CHECK(vkCreateImageView(*m_device, &viewInf, nullptr, &m_depthImageView));
#endif
		auto extent = m_swapchain->getSwapChainExtent();
		ImageDescription id = {};
		id.depth = 1;
		id.width = extent.width;
		id.height = extent.height;
		id.faces = 1;
		id.format = DEPTH_FORMAT;
		id.usageFlags = ImageUsageFlags::Attachment;
		id.layers = 1;
		id.levels = 1;
		id.name = "default_depth";
		m_zBuffer = createImage(id);

	}

	void VulkanRenderer::init_imgui()
	{
		VkAttachmentDescription color = {};
		color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		color.format = m_swapchain->getSwapchainImageFormat();
		color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color.samples = VK_SAMPLE_COUNT_1_BIT;
		VkAttachmentReference colorRef = {};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass = {};
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		VkSubpassDependency dep = {};
		dep.dependencyFlags = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;

		VkRenderPassCreateInfo imguiRenderPassCI = {};
		imguiRenderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		imguiRenderPassCI.subpassCount = 1;
		imguiRenderPassCI.attachmentCount = 1;
		imguiRenderPassCI.dependencyCount = 1;
		imguiRenderPassCI.pAttachments = &color;
		imguiRenderPassCI.pDependencies = &dep;
		imguiRenderPassCI.pSubpasses = &subpass;

		VK_CHECK(vkCreateRenderPass(m_device->vkDevice, &imguiRenderPassCI, nullptr, &m_imguiRenderPass));
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsLight();
#ifdef VKS_USE_VOLK
		ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance)
			{
				return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
			}, &m_device->instance);
#endif
		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForVulkan(m_device->window->getWindow());
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = m_device->instance;
		init_info.PhysicalDevice = m_device->physicalDevice;
		init_info.Device = m_device->vkDevice;
		init_info.QueueFamily = m_device->queueFamilyIndices.graphics;
		init_info.Queue = m_device->graphicsQueue;
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = m_defaultDescriptorPool;
		init_info.Subpass = 0;
		init_info.MinImageCount = 2;
		init_info.ImageCount = m_swapchain->getImageCount();
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.Allocator = nullptr;
		init_info.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&init_info, m_imguiRenderPass);

		m_device->executeCommands([](VkCommandBuffer cmd)
			{
				ImGui_ImplVulkan_CreateFontsTexture(cmd);
			});

		ImGui_ImplVulkan_DestroyFontUploadObjects();

		//ImGui_ImplVulkan_NewFrame();
		//ImGui_ImplSDL2_NewFrame();

	}

	void VulkanRenderer::reset()
	{
		shutdown();
		init();
	}

	void VulkanRenderer::shutdown()
	{
		VkDevice device = m_device->vkbDevice;

		for (size_t i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i) {
			VK_CHECK(vkWaitForFences(device, 1, &m_framedata[i].renderFence, true, 1000000000ULL));
		}

		m_device->waitIdle();
		m_device->waitIdle();

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		vkDestroyRenderPass(m_device->vkDevice, m_imguiRenderPass, nullptr);

		for (size_t i = 0; i < m_drawCommandPools.size(); ++i) {
			vkDestroyCommandPool(device, m_drawCommandPools[i], nullptr);
		}

		for (size_t i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i) {
			vkDestroySemaphore(device, m_framedata[i].presentSemaphore, nullptr);
			vkDestroySemaphore(device, m_framedata[i].renderSemaphore, nullptr);
			vkDestroyFence(device, m_framedata[i].renderFence, nullptr);
			cleanupTemporaryFrameBuffers(&m_framedata[i]);
			vkDestroyDescriptorPool(device, m_framedata[i].globalShaderVarsPool, nullptr);
			vkDestroyCommandPool(device, m_framedata[i].commandPool, nullptr);
		}
		vkDestroyCommandPool(device, m_defaultCommandPool, nullptr);

		for (auto& it : m_samplers) {
			vkDestroySampler(device, it.sampler, nullptr);
		}

		vkDestroyDescriptorSetLayout(device, m_globalShaderVarsLayout, nullptr);
		vkDestroyDescriptorPool(device, m_globalTexturesPool, nullptr);
		vkDestroyDescriptorPool(device, m_defaultDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, m_gloalTexturesLayout, nullptr);

		m_images.clear();
		m_descriptorSets.clear();
		m_meshes.clear();
		m_uniformBuffers.clear();
		m_storageBuffers.clear();
		m_swapchainImages.clear();

		m_globalSamplers = {};
		m_globalTextureArray.clear();
		m_globalTextureArrayFreeIndexes.clear();
		m_globalTexturesArraySet = {};
		m_globalTexturesPool = {};
		m_globalShaderVarsLayout = {};
		m_gloalTexturesLayout = {};
		m_defaultCommandPool = {};
	}

	void VulkanRenderer::updateUniforms(PerFrameData& FRAME)
	{
		//m_uniformBuffers[FRAME.globalVarsUbo.index]->fillBufferImmediate(sizeof(gvars), &gvars);
	}

	void VulkanRenderer::prepareDrawCommands()
	{
		submitGraphicsPasses(m_framedata[FrameCounter::getFrameIndex()]);
	}

	void VulkanRenderer::updateUniformBufferData(handle::UniformBuffer handle, uint32_t size, const void* data)
	{
		m_deferredUniformBufferFills.push_back(FillUniformBufferData{ handle, toCharArray(size,data) });
	}

	void VulkanRenderer::updateStorageBufferData(handle::StorageBuffer handle, uint32_t size, const void* data)
	{
		m_deferredStorageBufferFills.push_back(FillStorageBufferData{ handle, toCharArray(size,data) });
	}

	void VulkanRenderer::submitGraphicsPasses(PerFrameData& frame)
	{
		const auto currentFrame = FrameCounter::getFrameIndex();

		for (const auto& renderPass : m_activePasses) 
		{	
			if (renderPass.type != RenderPassType::Graphics) { continue; }

			GraphicsPassInfo& pass = m_activeGraphicsPasses[renderPass.index];

			const uint32_t firstBuffer = m_numThreads * currentFrame;

			VkExtent2D extent = m_swapchain->getSwapChainExtent();

			const auto& grPipeline = m_graphicsPipelines[pass.pipelineHandle.index];

			std::vector<VkImageView> attachments;
			for (auto& it : pass.targets) {
				assert(handle::is_valid(it));
				const auto& image = getImageRef(it);
				extent.width = image->extent.width;
				extent.height = image->extent.height;
				attachments.push_back(image->view);
			}

			VkViewport viewport = {};
			viewport.width = (float)extent.width;
			viewport.height = (float)extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			VkRect2D scissor = {};
			scissor.extent = extent;

			VkFramebufferCreateInfo fbCI = vkinit::framebuffer_create_info(grPipeline.vkPipeline->getRenderPass(), extent, attachments);
			
			VkFramebuffer fb = createTemporaryFrameBuffer(fbCI);
			pass.frameBuffer = fb;

			VkCommandBufferInheritanceInfo inheritance = {};
			inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			inheritance.renderPass = grPipeline.vkPipeline->getRenderPass();
			inheritance.framebuffer = fb;
			inheritance.subpass = 0;
			VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
			beginInfo.pInheritanceInfo = &inheritance;

			std::vector<VkDescriptorSet> sets = { frame.globalShaderVarsSet,m_globalTexturesArraySet };
			const VkDescriptorSet descriptorSet = grPipeline.vkPipeline->getDescriptorSet(currentFrame);

			if (descriptorSet) {
				sets.push_back(descriptorSet);
				updateDescriptorSet(descriptorSet, pass.resources);
			}

			for (size_t i = 0; i < m_numThreads; ++i)
			{
				VkCommandBuffer cmd = grPipeline.drawCommandBuffers[i + firstBuffer];
				vkResetCommandBuffer(cmd, 0);
				vkBeginCommandBuffer(cmd, &beginInfo);
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grPipeline.vkPipeline->getPipeline());
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grPipeline.vkPipeline->getPipelineLayout(), 0, uint32_t(sets.size()), sets.data(), 0, nullptr);
				vkCmdSetViewport(cmd, 0, 1, &viewport);
				vkCmdSetScissor(cmd, 0, 1, &scissor);
				if (handle::is_valid(m_mergedMesh))
				{
					VkBuffer vbuffer = m_meshes[m_mergedMesh.index].vertexBuffer->handle;
					VkBuffer ibuffer = m_meshes[m_mergedMesh.index].indexBuffer->handle;
					VkDeviceSize voffset = 0;
					vkCmdBindVertexBuffers(cmd, 0, 1, &vbuffer, &voffset);
					vkCmdBindIndexBuffer(cmd, ibuffer, 0, m_meshes[m_mergedMesh.index].indexType);
				}
			}

		}
	}

	void VulkanRenderer::updateDescriptorSet(VkDescriptorSet set, const RenderPassResources& resources)
	{
		size_t total_image_updates = resources.sampledImages.size() + resources.samplers.size() + resources.storageImages.size() + resources.combinedImageSamplers.size();
		size_t total_buffer_updates = resources.storageBuffers.size() + resources.uniformBuffers.size();

		std::vector<VkWriteDescriptorSet> wsets;
		std::vector<VkDescriptorImageInfo> imageInfos;
		std::vector<VkDescriptorBufferInfo> bufferInfos;

		imageInfos.reserve(total_image_updates);
		bufferInfos.reserve(total_buffer_updates);

		for (const auto& it : resources.sampledImages) {
			VkDescriptorImageInfo info = {};
			const auto& vkimage = getImageRef(it.image);
			info.imageLayout = vkimage->layout;
			info.imageView = vkimage->view;
			imageInfos.push_back(info);
			wsets.push_back(vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, set, &imageInfos.back(), it.binding, 0));
		}

		for (const auto& it : resources.combinedImageSamplers) {
			VkDescriptorImageInfo info = {};
			const auto& vkimage = getImageRef(it.image);
			const auto& sampler = m_samplers[it.sampler.index];

			info.imageLayout = vkimage->layout;
			info.imageView = vkimage->view;
			info.sampler = sampler.sampler;

			imageInfos.push_back(info);
			wsets.push_back(vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, set, &imageInfos.back(), it.binding, 0));
		}

		for (const auto& it : resources.storageImages) {
			VkDescriptorImageInfo info = {};
			const auto& vkimage = getImageRef(it.image);
			info.imageLayout = vkimage->layout;
			info.imageView = vkimage->view;
			imageInfos.push_back(info);
			wsets.push_back(vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, set, &imageInfos.back(), it.binding, 0));
		}

		for (const auto& it : resources.samplers) {
			VkDescriptorImageInfo info = {};
			const auto& vksampler = m_samplers[it.sampler.index];
			info.sampler = vksampler.sampler;
			imageInfos.push_back(info);
			wsets.push_back(vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLER, set, &imageInfos.back(), it.binding, 0));
		}

		for (const auto& it : resources.uniformBuffers) {
			VkDescriptorBufferInfo info = {};
			const auto& vkbuffer = m_uniformBuffers[it.buffer.index];
			info = vkbuffer->descriptor;
			bufferInfos.push_back(info);
			wsets.push_back(vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, set, &bufferInfos.back(), it.binding));
		}

		for (const auto& it : resources.storageBuffers) {
			VkDescriptorBufferInfo info = {};
			const auto& vkbuffer = m_storageBuffers[it.buffer.index];
			info = vkbuffer->descriptor;
			bufferInfos.push_back(info);
			wsets.push_back(vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, set, &bufferInfos.back(), it.binding));
		}

		vkUpdateDescriptorSets(m_device->vkDevice, uint32_t(wsets.size()), wsets.data(), 0, nullptr);
	}

	void VulkanRenderer::processDeferredBufferFills2()
	{
		VkCommandPool pool;
		VkCommandBuffer cmd;
		VkBufferCopy copyInfo;

		VK_CHECK(m_device->createCommandPool(m_device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &pool));
		VK_CHECK(m_device->createCommandBuffer(pool, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, false, &cmd, false));

		const uint32_t frameIndex = FrameCounter::getFrameIndex();
		PerFrameData& FRAME = m_framedata[frameIndex];

		m_stagingBuffer->map();
		m_stagingBuffer->copyTo(&gpass_data, sizeof(gpass_data));
		m_stagingBuffer->unmap();
		copyInfo.size = sizeof(gpass_data);
		copyInfo.srcOffset = 0;
		
		m_device->beginCommandBuffer(cmd);
		vkCmdCopyBuffer(cmd, m_stagingBuffer->handle, m_uniformBuffers[FRAME.globalVarsUbo.index]->handle, 1, &copyInfo);
		m_device->flushCommandBuffer(cmd, m_device->getGraphicsQueue(), pool, false);

		VkDeviceSize stageBufferOffset(0);
		VkDeviceSize copySize(0);
		for (const auto& it : m_deferredUniformBufferFills) {
			
			size_t copied(0);
			while (copied < it.data.size())
			{
				copySize = std::min(it.data.size() - copied, (size_t)m_stagingBuffer->size);
				m_stagingBuffer->map();
				m_stagingBuffer->copyTo(it.data.data() + copied, copySize);
				m_stagingBuffer->unmap();
				copyInfo.size = copySize;
				copyInfo.dstOffset = copied;
				m_device->beginCommandBuffer(cmd);
				vkCmdCopyBuffer(cmd, m_stagingBuffer->handle, m_uniformBuffers[it.buffer.index]->handle, 1, &copyInfo);
				m_device->flushCommandBuffer(cmd, m_device->getGraphicsQueue(), pool, false);
				copied += copySize;
			}
		}
		for (const auto& it : m_deferredStorageBufferFills) {

			size_t copied(0);
			while (copied < it.data.size())
			{
				copySize = std::min(it.data.size() - copied, (size_t)m_stagingBuffer->size);
				m_stagingBuffer->map();
				m_stagingBuffer->copyTo(it.data.data() + copied, copySize);
				m_stagingBuffer->unmap();
				copyInfo.size = copySize;
				copyInfo.dstOffset = copied;
				m_device->beginCommandBuffer(cmd);
				vkCmdCopyBuffer(cmd, m_stagingBuffer->handle, m_storageBuffers[it.buffer.index]->handle, 1, &copyInfo);
				m_device->flushCommandBuffer(cmd, m_device->getGraphicsQueue(), pool, false);
				copied += copySize;
			}
		}

		vkDestroyCommandPool(m_device->vkDevice, pool, nullptr);
	}

	void VulkanRenderer::processDeferredBufferFills(VkCommandBuffer cmd)
	{
		const uint32_t frameIndex = FrameCounter::getFrameIndex();
		PerFrameData& FRAME = m_framedata[frameIndex];

		uint32_t stagebufferMemoryReq = sizeof(gpass_data);
		PipelineBarrier barriers;
#ifdef USE_BUFFER_BARRIER
		BufferBarrier bb = {};
		bb.accessMasks(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
			.buffer(m_uniformBuffers[FRAME.globalVarsUbo.index]->handle)
			.range(0, sizeof(gpass_data));
		barriers.add(bb);
#endif
		for (const auto& it : m_deferredUniformBufferFills) {
			stagebufferMemoryReq += uint32_t(it.data.size());
#ifdef USE_BUFFER_BARRIER
			bb.accessMasks(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
				.buffer(m_uniformBuffers[it.buffer.index]->handle)
				.range(0, it.data.size());

			barriers.add(bb);
#endif
		}
		for (const auto& it : m_deferredStorageBufferFills) {
			//m_storageBuffers[it.buffer.index]->fillBufferImmediate((unsigned)it.data.size(), it.data.data());
			stagebufferMemoryReq += uint32_t(it.data.size());
#ifdef USE_BUFFER_BARRIER
			bb.accessMasks(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
				.buffer(m_storageBuffers[it.buffer.index]->handle)
				.range(0, it.data.size());

			barriers.add(bb);
#endif
		}
		
		if (!m_deferredStagingBuffer[frameIndex] || m_deferredStagingBuffer[frameIndex]->size < stagebufferMemoryReq) {
			m_deferredStagingBuffer[frameIndex].reset(
				new VulkanBuffer(m_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagebufferMemoryReq, VMA_MEMORY_USAGE_CPU_ONLY));
			jsrlib::Info("StageBuffer[%d] grows to %d", frameIndex, stagebufferMemoryReq);
		}
		
		const std::unique_ptr<VulkanBuffer>& stagingBuffer = m_deferredStagingBuffer[frameIndex];

		size_t offset = 0;
		VK_CHECK(stagingBuffer->map());
		{
			char* mapped = (char*)stagingBuffer->mapped;

			memcpy(mapped + offset, &gpass_data, sizeof(gpass_data));
			VkBufferCopy copyInfo{};
			copyInfo.size = sizeof(gpass_data);
			copyInfo.srcOffset = offset;
			vkCmdCopyBuffer(cmd, stagingBuffer->handle, m_uniformBuffers[FRAME.globalVarsUbo.index]->handle, 1, &copyInfo);

			offset += sizeof(gpass_data);

			for (const auto& it : m_deferredUniformBufferFills) {
				memcpy(mapped + offset, it.data.data(), it.data.size());

				copyInfo.size = it.data.size();
				copyInfo.srcOffset = offset;
				vkCmdCopyBuffer(cmd, stagingBuffer->handle, m_uniformBuffers[it.buffer.index]->handle, 1, &copyInfo);
				offset += it.data.size();
			}
			for (const auto& it : m_deferredStorageBufferFills) {
				memcpy(mapped + offset, it.data.data(), it.data.size());

				copyInfo.size = it.data.size();
				copyInfo.srcOffset = offset;
				vkCmdCopyBuffer(cmd, stagingBuffer->handle, m_storageBuffers[it.buffer.index]->handle, 1, &copyInfo);
				offset += it.data.size();
			}
		}
		stagingBuffer->unmap();

		MemoryBarrier mb = {};
		mb.accessMasks(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT| VK_ACCESS_SHADER_WRITE_BIT);
		barriers.add(mb);
//		barriers.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
		barriers.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	void VulkanRenderer::renderImgui(VkCommandBuffer cmd, VkExtent2D extent)
	{
		ImGui::Render();
		ImDrawData* draw_data = ImGui::GetDrawData();
		std::vector<VkImageView> schImage = { m_swapchain->getLastAcquiredImageView() };

		VkFramebufferCreateInfo fbCI = vkinit::framebuffer_create_info(m_imguiRenderPass, extent, schImage);
		VkFramebuffer fb = createTemporaryFrameBuffer(fbCI);

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.clearValueCount = 0;
		renderPassInfo.framebuffer = fb;
		renderPassInfo.renderArea.extent = extent;
		renderPassInfo.renderPass = m_imguiRenderPass;

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
		vkCmdEndRenderPass(cmd);

	}

	const std::vector<RenderPassBarriers> VulkanRenderer::createRenderPassBarriers()
	{
		std::vector<RenderPassBarriers> result;

		for (const auto& renderPass : m_activePasses)
		{
			const RenderPassResources* resources = nullptr;
			RenderPassBarriers passBarriers;
			if (renderPass.type == RenderPassType::Graphics)
			{
				const auto& graphicsPass = m_activeGraphicsPasses[renderPass.index];
				resources = &graphicsPass.resources;

				for (const auto& target : graphicsPass.targets) {

					if (!handle::is_valid(target)) continue;

					auto& imageRef = getImageRef(target);
					const bool isDepth = VulkanImage::isDepthFormat(imageRef->format);
					const VkImageLayout requiredLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

					const bool needsLayoutChange = imageRef->layout != requiredLayout;

					if (needsLayoutChange || imageRef->currentlyWriting) {
						ImageBarrier ib = {};
						VkImageSubresourceRange subres = {};
						subres.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
						if (VulkanImage::isStencilFormat(imageRef->format)) {
							subres.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
						}
						subres.baseArrayLayer = 0;
						subres.baseMipLevel = 0;
						subres.layerCount = imageRef->layers;
						subres.levelCount = imageRef->levels;

						const VkAccessFlags newAccess = isDepth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						ib.image(imageRef->image)
							.layout(imageRef->layout, requiredLayout)
							.accessMasks(imageRef->currentAccess, newAccess)
							.subresourceRange(subres)
							.build();

						passBarriers.imageBarriers.push_back(ib);
						imageRef->layout = requiredLayout;
						imageRef->currentAccess = newAccess;
						//imageRef->currentlyWriting = false;
					}
					imageRef->currentlyWriting = true;
				}

			}
			else {
				resources = &m_activeComputePasses[renderPass.index].resources;
			}

			for (const auto& sampledImage : resources->sampledImages)
			{
				if (!handle::is_valid(sampledImage.image)) { continue; }
				auto& image = getImageRef(sampledImage.image);
				const bool isDepth = VulkanImage::isDepthFormat(image->format);
				const VkImageLayout requiredLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				const bool needsLayoutChange = image->layout != requiredLayout;
				if (needsLayoutChange || image->currentlyWriting)
				{
					ImageBarrier ib = {};
					VkImageSubresourceRange subres = {};
					subres.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
					subres.baseArrayLayer = 0;
					subres.baseMipLevel = 0;
					subres.layerCount = image->levels;
					subres.levelCount = image->layers;

					const VkAccessFlags newAccess = VK_ACCESS_SHADER_READ_BIT;
					ib.image(image->image)
						.layout(image->layout, requiredLayout)
						.accessMasks(image->currentAccess, newAccess)
						.subresourceRange(subres)
						.build();

					passBarriers.imageBarriers.push_back(ib);
					image->layout = requiredLayout;
					image->currentAccess = newAccess;
				}
				image->currentlyWriting = false;
			}

			for (const auto& storageImage : resources->storageImages)
			{
				if (!handle::is_valid(storageImage.image)) { continue; }
				auto& image = getImageRef(storageImage.image);
				const VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_GENERAL;
				const bool needsLayoutChange = image->layout != requiredLayout;
				if (needsLayoutChange || image->currentlyWriting)
				{
					ImageBarrier ib = {};
					VkImageSubresourceRange subres = {};
					subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					subres.baseArrayLayer = 0;
					subres.baseMipLevel = storageImage.level;
					subres.layerCount = 1;
					subres.levelCount = 1;

					const VkAccessFlags newAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
					ib.image(image->image)
						.layout(image->layout, requiredLayout)
						.accessMasks(image->currentAccess, newAccess)
						.subresourceRange(subres)
						.build();

					passBarriers.imageBarriers.push_back(ib);
					image->layout = requiredLayout;
					image->currentAccess = newAccess;
				}
				image->currentlyWriting = storageImage.write;
			}

			for (const auto& storageBuffer : resources->storageBuffers)
			{
				if (!handle::is_valid(storageBuffer.buffer)) { continue; }
				auto& buffer = m_storageBuffers[storageBuffer.buffer.index];
				if (buffer->hasWritten && passBarriers.memoryBarriers.empty()) {
					MemoryBarrier mb;
					mb.accessMasks(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
					passBarriers.memoryBarriers.push_back(mb);
				}
				buffer->hasWritten = false;
			}
			result.push_back(passBarriers);
		}
		return result;
	}

	handle::UniformBuffer VulkanRenderer::createUniformBufferInternal(const UniformBufferDescription& desc)
	{
		handle::UniformBuffer handle = {};
		handle.index = (uint32_t) m_uniformBuffers.size();
		m_uniformBuffers.emplace_back(std::make_unique<VulkanBuffer>(
			m_device,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			uint32_t(desc.size),
			VMA_MEMORY_USAGE_GPU_ONLY));

		return handle;
	}

	handle::StorageBuffer VulkanRenderer::createStorageBufferInternal(const StorageBufferDescription& desc)
	{
		handle::StorageBuffer handle = {};

		auto buffer = std::make_unique<VulkanBuffer>(
			m_device,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			uint32_t(desc.size),
			VMA_MEMORY_USAGE_GPU_ONLY);

		if (m_storageBufferFreeIndexes.empty()) {
			handle.index = (uint32_t)m_storageBuffers.size();
			m_storageBuffers.push_back(std::move(buffer));
		}
		else {
			handle.index = m_storageBufferFreeIndexes.back();
			m_storageBufferFreeIndexes.pop_back();
			m_storageBuffers[handle.index] = std::move(buffer);
		}

		return handle;
	}

	handle::Image VulkanRenderer::createImageInternal(const ImageDescription& desc)
	{
		return allocateImage(std::make_unique<VulkanImage>(m_device, desc));
	}

	handle::Image VulkanRenderer::createImageInternal(const std::filesystem::path& filename, const std::string& name)
	{
		auto handle = allocateImage(std::make_unique<VulkanImage>(m_device, filename));
			
		getImageRef(handle)->name = name;

		return handle;
	}

	handle::Image VulkanRenderer::allocateImage(std::unique_ptr<VulkanImage>& param)
	{
		handle::Image handle = {};
		if (m_imagesFreeIndexes.empty())
		{
			handle.index = uint32_t(m_images.size());
			m_images.push_back(std::move(param));
		}
		else
		{
			handle.index = m_imagesFreeIndexes.back();
			m_imagesFreeIndexes.pop_back();
			m_images[handle.index] = std::move(param);
		}

		if (bool(ImageUsageFlags::Sampled & m_images[handle.index]->usageFlags))
		{
			uint32_t arrayIndex = ~0;
			if (m_globalTextureArrayFreeIndexes.empty())
			{
				arrayIndex = uint32_t(m_globalTextureArray.size());
				m_globalTextureArray.push_back(handle);
			}
			else
			{
				arrayIndex = m_globalTextureArrayFreeIndexes.back();
				m_globalTextureArrayFreeIndexes.pop_back();
				m_globalTextureArray[arrayIndex] = handle;
			}
			m_images[handle.index]->globalIndex = arrayIndex;
			updateGlobalTextureArray(arrayIndex, handle);
		}

		return handle;
	}

	void VulkanRenderer::updateGlobalTextureArray(uint32_t arrayIndex, handle::Image imageHandle)
	{
		const auto& IMG = getImageRef(imageHandle);

		VkDescriptorImageInfo info{};
		info.imageLayout = IMG->layout;
		info.imageView = IMG->view;
		VkWriteDescriptorSet wset = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_globalTexturesArraySet, &info, 0, arrayIndex);
		//vkDeviceWaitIdle(m_device->vkDevice);
		vkUpdateDescriptorSets(m_device->vkDevice, 1, &wset, 0, nullptr);
	}

	uint32_t VulkanRenderer::getImageGlobalIndex(handle::Image handle) const
	{
		const auto& img = getImageRef(handle);
		return img->globalIndex;
	}

	void VulkanRenderer::fillBufferImmediate(handle::UniformBuffer handle, uint32_t size, const void* data)
	{
		m_uniformBuffers[handle.index]->fillBufferImmediate(size, data);
	}

	void VulkanRenderer::fillBufferImmediate(handle::StorageBuffer handle, uint32_t size, const void* data)
	{
		m_storageBuffers[handle.index]->fillBufferImmediate(size, data);
	}

	handle::Image VulkanRenderer::getZBuffer() const
	{
		return m_zBuffer;
	}

	handle::Image VulkanRenderer::getWhiteImage() const
	{
		return m_whiteImage;
	}

	handle::Image VulkanRenderer::getBlackImage() const
	{
		return m_blackImage;
	}

	handle::Image VulkanRenderer::getFlatDepthImage() const
	{
		return m_flatDepthImage;
	}

	handle::Sampler VulkanRenderer::getDefaultAnisotropeRepeatSampler() const
	{
		return m_globalSamplers.anisotropeLinearRepeat;
	}

	void VulkanRenderer::renderFrame(bool present)
	{
		VkDevice device = m_device->vkDevice;

		const uint32_t frameNumber = FrameCounter::getCurrentFrame();
		const uint32_t currentFrame = FrameCounter::getFrameIndex();
		const uint32_t commandBuffersOffset = m_numThreads * currentFrame;
		auto& frame = m_framedata[currentFrame];

		VK_CHECK(vkResetFences(device, 1, &frame.renderFence));
//		VK_CHECK(vkResetCommandBuffer(frame.commandBuffer, 0));
		VK_CHECK(vkResetCommandPool(device, frame.commandPool, 0));

		VkCommandBuffer cmd = frame.commandBuffer;
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;

		VkExtent2D extent = m_swapchain->getSwapChainExtent();
		const auto barriers = createRenderPassBarriers();

		//processDeferredBufferFills2();

		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
		{
			processDeferredBufferFills(cmd);
			//defaultPass(swapchain_image.imageView);
			for (int passId = 0; passId < m_activePasses.size(); ++passId) {

				const auto& renderPass = m_activePasses[passId];
				
				if (renderPass.type == RenderPassType::Compute) 
				{ 
					const auto& computePass = m_activeComputePasses[renderPass.index];
					const ComputePipeline& pipeline = m_computePipelines[computePass.pipelineHandle.index];
					const auto& passBarriers = barriers[passId];
					PipelineBarrier pipelineBarrier;
					pipelineBarrier.add(passBarriers.imageBarriers);
					pipelineBarrier.add(passBarriers.memoryBarriers);
					pipelineBarrier.issuePipelineBarrierCommand(cmd);
					VkDescriptorSet descSet = VK_NULL_HANDLE;
					if (handle::is_valid(computePass.descriptorSet)) {
						descSet = m_descriptorSets[computePass.descriptorSet.index].set;
					}
					else {
						descSet = pipeline.vkPipeline->getDescriptorSet(currentFrame);
						updateDescriptorSet(descSet, computePass.resources);
					}
					const std::vector<VkDescriptorSet> sets = { frame.globalShaderVarsSet, m_globalTexturesArraySet, descSet };
					vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.vkPipeline->getPipeline());
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.vkPipeline->getPipelineLayout(), 0, asuint(sets.size()), sets.data(), 0, nullptr);
					vkCmdDispatch(cmd, 
						computePass.invocationCount[0] / pipeline.groupSize[0],
						computePass.invocationCount[1] / pipeline.groupSize[1],
						computePass.invocationCount[2] / pipeline.groupSize[2]);

					continue; 
				}

				auto& pass = m_activeGraphicsPasses[renderPass.index];

				const GraphicsPipeline& pipeline = m_graphicsPipelines[pass.pipelineHandle.index];
				const auto clearValues = pipeline.vkPipeline->getClearValues();
				const auto& passBarriers = barriers[passId];

				PipelineBarrier pipelineBarrier;
				pipelineBarrier.add(passBarriers.imageBarriers);
				pipelineBarrier.add(passBarriers.memoryBarriers);
				pipelineBarrier.issuePipelineBarrierCommand(cmd);

				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.clearValueCount		= (uint32_t) clearValues.size();
				renderPassInfo.pClearValues			= clearValues.data();
				renderPassInfo.framebuffer			= pass.frameBuffer;
				renderPassInfo.renderArea.extent	= extent;
				renderPassInfo.renderPass			= pipeline.vkPipeline->getRenderPass();
				
				vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

				for (size_t i = 0; i < m_numThreads; ++i) {
					vkEndCommandBuffer(pipeline.drawCommandBuffers[i + commandBuffersOffset]);
				}

				vkCmdExecuteCommands(cmd, m_numThreads, &pipeline.drawCommandBuffers[commandBuffersOffset]);
				vkCmdEndRenderPass(cmd);
			}
		}

		renderImgui(cmd, extent);

		VK_CHECK(vkEndCommandBuffer(cmd));

		//--- submit commands ---
		VkPipelineStageFlags psf = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo si{};
		si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		si.commandBufferCount = 1;
		si.pCommandBuffers = &frame.commandBuffer;
		si.signalSemaphoreCount = 1;
		si.pSignalSemaphores = &frame.renderSemaphore;
		si.waitSemaphoreCount = 1;
		si.pWaitSemaphores = &frame.presentSemaphore;
		si.pWaitDstStageMask = &psf;

		VK_CHECK(vkQueueSubmit(m_graphicQueue, 1, &si, frame.renderFence));


		if (present) {
			getImageRef(getSwapchainImage())->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			m_swapchain->presentImage(m_graphicQueue, frame.renderSemaphore, m_swapchain->getLastAcquiredImageIndex());
		}
	}

	void VulkanRenderer::setWindowResized()
	{
		m_windowResized = true;
	}

	VulkanSwapchain* VulkanRenderer::getSwapchain()
	{
		return m_swapchain;
	}

	VkFramebuffer VulkanRenderer::createTemporaryFrameBuffer(const VkFramebufferCreateInfo& framebufferInfo)
	{
		VkFramebuffer fb;
		VK_CHECK(vkCreateFramebuffer(m_device->vkDevice, &framebufferInfo, nullptr, &fb));

		m_framedata[FrameCounter::getFrameIndex()].temporaryFramebuffers.push_back(fb);

		return fb;
	}

	handle::UniformBuffer VulkanRenderer::createUniformBuffer(const UniformBufferDescription& desc)
	{
		handle::UniformBuffer handle = createUniformBufferInternal(desc);
		auto& buffer = m_uniformBuffers[handle.index];
		if (desc.initialData) {
			buffer->fillBufferImmediate(desc.size, desc.initialData);
		}

		return handle;
	}

	handle::StorageBuffer VulkanRenderer::createStorageBuffer(const StorageBufferDescription& desc)
	{
		handle::StorageBuffer handle = createStorageBufferInternal(desc);
		auto& buffer = m_storageBuffers[handle.index];
		if (desc.initialData) {
			buffer->fillBufferImmediate(desc.size, desc.initialData);
		}

		return handle;
	}

	void VulkanRenderer::freeStorageBuffer(handle::StorageBuffer handle)
	{
		auto& data = m_storageBuffers[handle.index];
		data->unmap();

		m_storageBufferFreeIndexes.push_back(handle.index);
		m_storageBuffers[handle.index].reset();
	}

	handle::Image VulkanRenderer::createImage(const ImageDescription& desc)
	{
		return createImageInternal(desc);
	}

	handle::Image VulkanRenderer::createImage(const std::filesystem::path& filename, const std::string& name)
	{
		return createImageInternal(filename, name);
	}

	handle::Image VulkanRenderer::getSwapchainImage() const
	{
		return handle::Image{ m_swapchain->getLastAcquiredImageIndex(), ImageHandleType::Swapchain };
	}

	handle::Sampler VulkanRenderer::createSampler(const VkSamplerCreateInfo& samplerInfo)
	{
		handle::Sampler handle = {};
		handle.index = static_cast<uint32_t>(m_samplers.size());
		Sampler sampler = {};
		VK_CHECK(vkCreateSampler(m_device->vkDevice, &samplerInfo, nullptr, &sampler.sampler));
		m_samplers.push_back(sampler);

		return handle;
	}

	handle::DescriptorSet VulkanRenderer::createDescriptorSet(handle::GraphicsPipeline pipeline, const RenderPassResources& resources)
	{
		handle::DescriptorSet result{};
		
		const GraphicsPipeline& pData = m_graphicsPipelines[pipeline.index];
		VkDescriptorSetLayout layout = pData.vkPipeline->getPerObjectLayout();

		if (layout == VK_NULL_HANDLE) {
			return result;
		}

		if (m_descriptorSetsFreeIndexes.empty()) {
			result.index = static_cast<uint32_t>(m_descriptorSets.size());
			m_descriptorSets.push_back({});
		}
		else {
			result.index = m_descriptorSetsFreeIndexes.back();
			m_descriptorSetsFreeIndexes.pop_back();
		}

		descriptorSet_t& data = m_descriptorSets[result.index];

		VkDescriptorSetAllocateInfo inf = {};
		inf.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		inf.descriptorPool = m_defaultDescriptorPool;
		inf.descriptorSetCount = 1;
		inf.pSetLayouts = &layout;
		VK_CHECK(vkAllocateDescriptorSets(m_device->vkDevice, &inf, &data.set));
		updateDescriptorSet(data.set, resources);

		return result;
	}

	handle::DescriptorSet VulkanRenderer::createDescriptorSet(handle::ComputePipeline pipeline, const RenderPassResources& resources)
	{
		handle::DescriptorSet result{};

		const ComputePipeline& pData = m_computePipelines[pipeline.index];
		VkDescriptorSetLayout layout = pData.vkPipeline->getPerObjectLayout();

		if (layout == VK_NULL_HANDLE) {
			return result;
		}

		if (m_descriptorSetsFreeIndexes.empty()) {
			result.index = static_cast<uint32_t>(m_descriptorSets.size());
			m_descriptorSets.push_back({});
		}
		else {
			result.index = m_descriptorSetsFreeIndexes.back();
			m_descriptorSetsFreeIndexes.pop_back();
		}

		descriptorSet_t& data = m_descriptorSets[result.index];

		VkDescriptorSetAllocateInfo inf = {};
		inf.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		inf.descriptorPool = m_defaultDescriptorPool;
		inf.descriptorSetCount = 1;
		inf.pSetLayouts = &layout;
		VK_CHECK(vkAllocateDescriptorSets(m_device->vkDevice, &inf, &data.set));
		updateDescriptorSet(data.set, resources);

		return result;
	}

	std::vector<handle::Mesh> VulkanRenderer::createMeshes(const std::vector<MeshDescription>& descriptions)
	{
		std::vector<handle::Mesh> out;

		for (const auto& ci : descriptions) {

			assert(ci.vertexCount > 0);
			assert(ci.vertexBuffer.size() > 0);

			Mesh mesh(ci.vertexCount, ci.indexCount);
			handle::Mesh handle;

			//if (ci.indexCount > std::numeric_limits<uint16_t>::max()) {
			//	mesh.indexType = VK_INDEX_TYPE_UINT32;
			//}

			mesh.indexType = ci.indexType == INDEX_TYPE_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

			VulkanBuffer* vertexBuffer = 
				new VulkanBuffer(m_device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, (unsigned)ci.vertexBuffer.size(), VMA_MEMORY_USAGE_GPU_ONLY);
			VulkanBuffer* indexBuffer = 
				new VulkanBuffer(m_device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, (unsigned)(ci.indexBuffer.size() * sizeof(uint16_t)), VMA_MEMORY_USAGE_GPU_ONLY);

			vertexBuffer->fillBufferImmediate((unsigned)ci.vertexBuffer.size(), ci.vertexBuffer.data());
			indexBuffer->fillBufferImmediate((unsigned)(ci.indexBuffer.size() * sizeof(uint16_t)), ci.indexBuffer.data());
			
			handle.index = (unsigned) m_meshes.size();
			mesh.indexBuffer.reset(indexBuffer);
			mesh.vertexBuffer.reset(vertexBuffer);

			m_meshes.push_back(std::move(mesh));
			out.push_back(handle);
		}

		return out;
	}

	void VulkanRenderer::setMergedMesh(handle::Mesh hMesh)
	{
		m_mergedMesh = hMesh;
	}

	void VulkanRenderer::freeImage(handle::Image handle)
	{
		if (handle.type != ImageHandleType::Swapchain)
		{
			auto& a = m_images[handle.index];
			if (a->globalIndex != ~0)
			{
				m_globalTextureArray[a->globalIndex] = {};
				m_globalTextureArrayFreeIndexes.push_back(a->globalIndex);
			}
			m_imagesFreeIndexes.push_back(handle.index);
			m_images[handle.index].reset();
		}
	}

	void VulkanRenderer::cleanupTemporaryFrameBuffers(PerFrameData* frame)
	{		
		for (auto& it : frame->temporaryFramebuffers) {
			vkDestroyFramebuffer(m_device->vkDevice, it, nullptr);
		}
		frame->temporaryFramebuffers.clear();
	}

	std::unique_ptr<VulkanImage>& VulkanRenderer::getImageRef(const handle::Image handle)
	{
		if (handle.type == ImageHandleType::Default) {
			assert(handle.index < m_images.size());
			return m_images[handle.index];
		}
		else if (handle.type == ImageHandleType::Swapchain) {
			assert(handle.index < m_swapchainImages.size());
			return m_swapchainImages[handle.index];
		}
		else {
			throw std::runtime_error("Not implemented");
		}
	}

	const std::unique_ptr<VulkanImage>& VulkanRenderer::getImageRef(const handle::Image handle) const
	{
		if (handle.type == ImageHandleType::Default || handle.type == ImageHandleType::Transient) {
			assert(handle.index < m_images.size());
			return m_images[handle.index];
		}
		else if (handle.type == ImageHandleType::Swapchain) {
			assert(handle.index < m_swapchainImages.size());
			return m_swapchainImages[handle.index];
		}
	}

}
