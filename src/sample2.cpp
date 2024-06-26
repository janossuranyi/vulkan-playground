#include "sample2.h"

#include "vkjs/vkcheck.h"
#include "vkjs/pipeline.h"
#include "vkjs/shader_module.h"
#include "jsrlib/jsr_logger.h"

#include <nvrhi/utils.h>
#include <nvrhi/validation.h>

#include "ktx.h"
#include "ktxvulkan.h"

namespace fs = std::filesystem;
using namespace glm;
using namespace nvrhi;

void Sample2App::init_pipelines()
{
	jvk::ShaderModule vert_module(*pDevice);
	jvk::ShaderModule frag_module(*pDevice);
	const fs::path vert_spirv_filename = basePath / "shaders/bin/debug.vert.spv";
	const fs::path frag_spirv_filename = basePath / "shaders/bin/debug.frag.spv";
	VK_CHECK(vert_module.create(vert_spirv_filename));
	VK_CHECK(frag_module.create(frag_spirv_filename));

	ShaderDesc shaderDesc(ShaderType::Vertex);
	shaderDesc.debugName = "Vertex Shader";
	m_vertexShader = m_nvrhiDevice->createShader(
		shaderDesc,
		vert_module.data(),
		vert_module.size());

	shaderDesc.debugName = "Fragment Shader";
	shaderDesc.shaderType = ShaderType::Pixel;
	m_fragmentShader = m_nvrhiDevice->createShader(
		shaderDesc,
		frag_module.data(),
		frag_module.size());

	auto layoutDesc = BindingLayoutDesc()
		.setVisibility(ShaderType::AllGraphics)
		.addItem(BindingLayoutItem::VolatileConstantBuffer(0))
		.addItem(BindingLayoutItem::Texture_SRV(0))
		.addItem(BindingLayoutItem::Sampler(0));

	m_bindingLayout = m_nvrhiDevice->createBindingLayout(layoutDesc);

	RenderState renderState = {};
	renderState.depthStencilState.depthTestEnable = false;
	renderState.rasterState.cullMode = RasterCullMode::None;

	auto pipelineDesc = GraphicsPipelineDesc()
		.setVertexShader(m_vertexShader)
		.setPixelShader(m_fragmentShader)
		.setRenderState(renderState)
		.addBindingLayout(m_bindingLayout);

	m_graphicsPipeline = m_nvrhiDevice->createGraphicsPipeline(pipelineDesc, m_fbs[0]);


	auto constantBufferDesc = BufferDesc()
		.setByteSize(sizeof(globals)) // stores one matrix
		.setIsConstantBuffer(true)
		.setIsVolatile(true)
		.setMaxVersions(16); // number of automatic versions, only necessary on Vulkan

	m_constantBuffer = m_nvrhiDevice->createBuffer(constantBufferDesc);

}

void Sample2App::on_update_gui()
{
	if (settings.overlay == false) return;
	ImGui::DragFloat3("Color Bias", &globals.rpColorBias[0], 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat3("Color Scale", &globals.rpColorMultiplier[0], 0.01f, 0.0f, 2000.0f, "%.2f");
	ImGui::DragFloat3("Vertex scale", &globals.rpScale[0], 0.01f, 0.0f, 300.0f);
	ImGui::DragFloat3("Vertex Bias", &globals.rpBias[0], 0.01f, -3.0f, 3.0f);
}

Sample2App::~Sample2App()
{
	m_nvrhiDevice->waitForIdle();
}

void Sample2App::create_framebuffers()
{
	if ( ! m_fbs.empty() ) {
		m_fbs.clear();
	}
	
	auto& views = swapchain.images;
	jsrlib::Info("itt1");

	for (int i(0); i < m_swapchainImages.size(); ++i) {
		auto framebufferDesc = FramebufferDesc()
			.addColorAttachment(m_swapchainImages[i]); // you can specify a particular subresource if necessary

		jsrlib::Info("itt2");
		FramebufferHandle framebuffer = m_nvrhiDevice->createFramebuffer(framebufferDesc);
		if (!framebuffer) { jsrlib::Error("Create framebuffer failed !!!"); }

		jsrlib::Info("itt3");

		m_fbs.push_back(framebuffer);
	}
}

void Sample2App::prepare()
{
	jvk::AppBase::prepare();


	basePath = fs::path("../..");

	vulkan::DeviceDesc vkDesc = {};
	vkDesc.graphicsQueue = pDevice->graphics_queue;
	vkDesc.device = pDevice->logicalDevice;
	vkDesc.graphicsQueueIndex = pDevice->queue_family_indices.graphics;
	vkDesc.physicalDevice = pDevice->vkbPhysicalDevice.physical_device;
	vkDesc.numDeviceExtensions = enabled_device_extensions.size();
	vkDesc.deviceExtensions = enabled_device_extensions.data();
	vkDesc.numInstanceExtensions = enabled_instance_extensions.size();
	vkDesc.instanceExtensions = enabled_instance_extensions.data();
	vkDesc.instance = instance;

	if (pDevice->vkbPhysicalDevice.has_separate_compute_queue()) {
		vkDesc.computeQueueIndex = pDevice->queue_family_indices.compute;
		vkDesc.computeQueue = pDevice->get_compute_queue();
	}
	if (pDevice->vkbPhysicalDevice.has_separate_transfer_queue()) {
		vkDesc.transferQueueIndex = pDevice->queue_family_indices.transfer;
		vkDesc.transferQueue = pDevice->get_transfer_queue();
	}
	m_nvrhiDevice = vulkan::createDevice(vkDesc);

	nvrhi::DeviceHandle nvrhiValidationLayer = nvrhi::validation::createValidationLayer(m_nvrhiDevice);
	m_nvrhiDevice = nvrhiValidationLayer; // make the rest of the application go through the validation layer

	on_window_resized();

	init_pipelines();

	auto sd = SamplerDesc()
		.setAllAddressModes(SamplerAddressMode::Repeat);

	m_sampColor = m_nvrhiDevice->createSampler(sd);

	m_commandList = m_nvrhiDevice->createCommandList();
	init_images();

	auto bindingSetDesc = nvrhi::BindingSetDesc()
		.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer))
		.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_tex0))
		.addItem(nvrhi::BindingSetItem::Sampler(0, m_sampColor));


	m_bindingSet = m_nvrhiDevice->createBindingSet(bindingSetDesc, m_bindingLayout);


	prepared = true;
}

void Sample2App::build_command_buffers()
{
	using namespace glm;
	using namespace nvrhi;

	IFramebuffer* currentFramebuffer = m_fbs[currentBuffer];
	ITexture* image = m_swapchainImages[currentBuffer];
	TextureSubresourceSet imgSub = {};

	m_commandList->open();
	m_commandList->beginMarker("Main render pass");
//	m_commandList->beginTrackingTextureState(image, imgSub, ResourceStates::Unknown);
	m_commandList->setPermanentTextureState(m_tex0, ResourceStates::ShaderResource);

	utils::ClearColorAttachment(m_commandList, currentFramebuffer, 0, Color(0.f));
	m_commandList->writeBuffer(m_constantBuffer, &globals, sizeof(globals), 0);

	auto graphicsState = GraphicsState()
		.setPipeline(m_graphicsPipeline)
		.setFramebuffer(currentFramebuffer)
		.setViewport(ViewportState().addViewportAndScissorRect(Viewport(width, height)))
		.addBindingSet(m_bindingSet);

	m_commandList->setGraphicsState(graphicsState);

	auto drawArguments = DrawArguments()
		.setVertexCount(3);

	m_commandList->draw(drawArguments);
	m_commandList->setTextureState(image, imgSub, ResourceStates::RenderTarget);
//	m_commandList->commitBarriers();
	m_commandList->endMarker();
	m_commandList->close();
	m_nvrhiDevice->executeCommandList(m_commandList);

}

void Sample2App::render()
{
	if (currentFrame == 1) {
		m_nvrhiDevice->runGarbageCollection();
	}

	build_command_buffers();
	//swapchain_images[currentBuffer].layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

void Sample2App::get_enabled_extensions()
{
	enabled_instance_extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
	enabled_instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	/**/
	enabled_device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	desired_device_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
	
}

void Sample2App::get_enabled_features()
{
	enabled_features.textureCompressionBC = VK_TRUE;
	enabled_features12.timelineSemaphore = VK_TRUE;

	VkPhysicalDeviceSynchronization2Features sync2 = {};
	sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	sync2.synchronization2 = VK_TRUE;
	required_generic_features.push_back(jvk::GenericFeature(sync2));
}

void Sample2App::init_images()
{
	ktxTexture* kTexture;
	KTX_error_code result;
	auto ktx = basePath / "resources/models/sponza/ktx/6772804448157695701.ktx2";

	result = ktxTexture_CreateFromNamedFile(ktx.u8string().c_str(),
		KTX_TEXTURE_CREATE_NO_FLAGS,
		&kTexture);

	auto fmt = Format::BC7_UNORM;

	if (result == KTX_SUCCESS)
	{
		if (ktxTexture2_NeedsTranscoding((ktxTexture2*)kTexture)) {
			ktx_texture_transcode_fmt_e tf;

			// Using VkGetPhysicalDeviceFeatures or GL_COMPRESSED_TEXTURE_FORMATS or
			// extension queries, determine what compressed texture formats are
			// supported and pick a format. For example
			VkPhysicalDeviceFeatures deviceFeatures = pDevice->vkbPhysicalDevice.features;
			khr_df_model_e colorModel = ktxTexture2_GetColorModel_e((ktxTexture2*)kTexture);
			if (deviceFeatures.textureCompressionBC)
				tf = KTX_TTF_BC7_RGBA;
			else {
				const std::string message = "Vulkan implementation does not support any available transcode target.";
				throw std::runtime_error(message.c_str());
			}

			result = ktxTexture2_TranscodeBasis((ktxTexture2*)kTexture, tf, 0);
			if (result != KTX_SUCCESS) {
				jsrlib::Info("Texture load error");
				return;
			}
			// Then use VkUpload or GLUpload to create a texture object on the GPU.
		}
		else {
			auto format = ktxTexture_GetVkFormat(kTexture);
			switch (format)
			{
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
			case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
				fmt = Format::BC1_UNORM_SRGB;
				break;
			case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
			case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
				fmt = Format::BC1_UNORM;
				break;
			case VK_FORMAT_BC3_SRGB_BLOCK:
				fmt = Format::BC3_UNORM_SRGB;
				break;
			case VK_FORMAT_BC3_UNORM_BLOCK:
				fmt = Format::BC3_UNORM;
				break;
			case VK_FORMAT_R8G8B8A8_UNORM:
				fmt = Format::RGBA8_UNORM;
				break;
			default:
				throw std::runtime_error("Invalid format");
			}
		}

		// Retrieve information about the texture from fields in the ktxTexture
		// such as:
		ktx_uint32_t numLevels = kTexture->numLevels;
		ktx_uint32_t baseWidth = kTexture->baseWidth;
		ktx_uint32_t baseHeight = kTexture->baseHeight;
		ktx_uint32_t numFaces = kTexture->numFaces;
		ktx_bool_t isArray = kTexture->isArray;

		TextureDesc td = TextureDesc()
			.setArraySize(kTexture->numLayers)
			.setDebugName("Texture0")
			.setDimension(TextureDimension::Texture2D)
			.setWidth(baseWidth)
			.setHeight(baseHeight)
			.setInitialState(ResourceStates::ShaderResource)
			.setFormat(fmt)
			.setMipLevels(numLevels);

		m_tex0 = m_nvrhiDevice->createTexture(td);
		
		TextureSubresourceSet subset = {};
		subset.numMipLevels = numLevels;

		const char* data = (const char*)ktxTexture_GetData(kTexture);

		auto cmd = m_nvrhiDevice->createCommandList();
		cmd->open();
		//cmd->beginTrackingTextureState(m_tex0, subset, ResourceStates::Unknown);
		cmd->setTextureState(m_tex0, subset, ResourceStates::CopyDest);


		for (int layer = 0; layer < 1; layer++)
		{
			for (int face = 0; face < numFaces; face++)
			{
				for (int level = 0; level < numLevels; level++)
				{
					const uint32_t layer_index(layer * numFaces + face);
					size_t offset{};
					ktxResult res = ktxTexture_GetImageOffset(kTexture, level, layer, face, &offset);
					assert(res == KTX_SUCCESS);
					const size_t rowpitch = ktxTexture_GetRowPitch(kTexture, level);
					cmd->writeTexture(m_tex0, layer, level, (data + offset), rowpitch);
				}
			}
		}
		cmd->close();
		m_nvrhiDevice->executeCommandList(cmd);
	}
}

void Sample2App::on_window_resized()
{
	auto fmt = Format::BGRA8_UNORM;

	//m_nvrhiDevice->waitForIdle();


	m_swapchainImages.clear();
	//pDevice->wait_idle();
	if (swapchain.vkb_swapchain.image_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
		fmt = Format::R10G10B10A2_UNORM;
	}
	for (size_t it = 0; it < swapchain_images.size(); ++it) {
		auto textureDesc = nvrhi::TextureDesc()
			.setDimension(nvrhi::TextureDimension::Texture2D)
			.setFormat(fmt)
			.setWidth(width)
			.setHeight(height)
			.setIsRenderTarget(true)
			.setDebugName("Swap Chain Image #" + std::to_string(it));

		// In this line, <type> depends on the GAPI and should be one of: D3D11_Resource, D3D12_Resource, VK_Image.
		nvrhi::TextureHandle swapChainTexture = 
			m_nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, swapchain_images[it].image, textureDesc);

		if (!swapChainTexture) { jsrlib::Error("Cannot create swapchain texture !!!"); }

		m_swapchainImages.push_back(swapChainTexture);

	}
	create_framebuffers();
}
