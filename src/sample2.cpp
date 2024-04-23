#include "sample2.h"

#include "vkjs/vkcheck.h"
#include "vkjs/pipeline.h"
#include "vkjs/shader_module.h"

#include "nvrhi/utils.h"

namespace fs = std::filesystem;

const glm::mat3 from709to2020 = glm::mat3(
	glm::vec3(0.6274040, 0.3292820, 0.0433136),
	glm::vec3(0.0690970, 0.9195400, 0.0113612),
	glm::vec3(0.0163916, 0.0880132, 0.8955950)
);

/* Fd is the displayed luminance in cd/m2 */
float PQinverseEOTF(float Fd)
{
	const float Y = Fd / 10000.0f;
	const float m1 = pow(Y, 0.1593017578125f);
	float res = (0.8359375f + (18.8515625f * m1)) / (1.0f + (18.6875f * m1));
	res = pow(res, 78.84375f);
	return res;
}

glm::vec3 PQinverseEOTF(glm::vec3 c)
{
	return glm::vec3(
		PQinverseEOTF(c.x),
		PQinverseEOTF(c.y),
		PQinverseEOTF(c.z));
}

void Sample2App::init_pipelines()
{
	jvk::ShaderModule vert_module(*pDevice);
	jvk::ShaderModule frag_module(*pDevice);
	const fs::path vert_spirv_filename = basePath / "shaders/bin/debug.vert.spv";
	const fs::path frag_spirv_filename = basePath / "shaders/bin/debug.frag.spv";
	VK_CHECK(vert_module.create(vert_spirv_filename));
	VK_CHECK(frag_module.create(frag_spirv_filename));

	nvrhi::ShaderDesc shaderDesc(nvrhi::ShaderType::Vertex);
	shaderDesc.debugName = "Vertex Shader";
	m_vertexShader = m_nvrhiDevice->createShader(
		shaderDesc,
		vert_module.data(),
		vert_module.size());

	shaderDesc.debugName = "Fragment Shader";
	shaderDesc.shaderType = nvrhi::ShaderType::Pixel;
	m_fragmentShader = m_nvrhiDevice->createShader(
		shaderDesc,
		frag_module.data(),
		frag_module.size());

	auto layoutDesc = nvrhi::BindingLayoutDesc()
		.setVisibility(nvrhi::ShaderType::Vertex)
		.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
	m_bindingLayout = m_nvrhiDevice->createBindingLayout(layoutDesc);

	nvrhi::RenderState renderState = {};
	renderState.depthStencilState.depthTestEnable = false;
	renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
		.setVertexShader(m_vertexShader)
		.setPixelShader(m_fragmentShader)
		.setRenderState(renderState)
		.addBindingLayout(m_bindingLayout);

	m_graphicsPipeline = m_nvrhiDevice->createGraphicsPipeline(pipelineDesc, m_fbs[0]);


	auto constantBufferDesc = nvrhi::BufferDesc()
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

	for (int i(0); i < views.size(); ++i) {
		auto framebufferDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(m_swapchainImages[i]); // you can specify a particular subresource if necessary

		nvrhi::FramebufferHandle framebuffer = m_nvrhiDevice->createFramebuffer(framebufferDesc);
		m_fbs.push_back(framebuffer);
	}
}

void Sample2App::prepare()
{
	jvk::AppBase::prepare();

	basePath = fs::path("../..");

	nvrhi::vulkan::DeviceDesc vkDesc = {};
	vkDesc.graphicsQueue = pDevice->graphics_queue;
	vkDesc.device = pDevice->logicalDevice;
	vkDesc.graphicsQueueIndex = pDevice->queue_family_indices.graphics;
	vkDesc.physicalDevice = pDevice->vkbPhysicalDevice.physical_device;
	vkDesc.numDeviceExtensions = enabled_device_extensions.size();
	vkDesc.deviceExtensions = enabled_device_extensions.data();
	vkDesc.numInstanceExtensions = enabled_instance_extensions.size();
	vkDesc.instanceExtensions = enabled_instance_extensions.data();
	vkDesc.instance = instance;

	if (pDevice->has_dedicated_compute_queue()) {
		vkDesc.computeQueueIndex = pDevice->queue_family_indices.compute;
		vkDesc.computeQueue = pDevice->get_compute_queue();
	}
	if (pDevice->has_dedicated_transfer_queue()) {
		vkDesc.transferQueueIndex = pDevice->queue_family_indices.transfer;
		vkDesc.transferQueue = pDevice->get_transfer_queue();
	}
	m_nvrhiDevice = nvrhi::vulkan::createDevice(vkDesc);

	on_window_resized();

	init_pipelines();

	m_commandList = m_nvrhiDevice->createCommandList();

	auto bindingSetDesc = nvrhi::BindingSetDesc()
		.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_constantBuffer));

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
	m_commandList->beginTrackingTextureState(image, imgSub, ResourceStates::Unknown);

	nvrhi::utils::ClearColorAttachment(m_commandList, currentFramebuffer, 0, nvrhi::Color(0.f));
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
	m_commandList->commitBarriers();

	m_commandList->close();
	m_nvrhiDevice->executeCommandList(m_commandList);
	m_nvrhiDevice->runGarbageCollection();

}

void Sample2App::render()
{
	build_command_buffers();
	//swapchain_images[currentBuffer].layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

}

void Sample2App::get_enabled_extensions()
{
	enabled_instance_extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
	enabled_device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
}

void Sample2App::get_enabled_features()
{
	enabled_features12.timelineSemaphore = true;
	VkPhysicalDeviceSynchronization2Features sync2 = {};
	sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	sync2.synchronization2 = true;

	required_generic_features.push_back(jvk::GenericFeature(sync2));
}

void Sample2App::on_window_resized()
{
	nvrhi::Format fmt = nvrhi::Format::BGRA8_UNORM;

	m_swapchainImages.clear();
	//pDevice->wait_idle();
	if (swapchain.vkb_swapchain.image_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
		fmt = nvrhi::Format::R10G10B10A2_UNORM;
	}
	for (size_t it = 0; it < swapchain_images.size(); ++it) {
		auto textureDesc = nvrhi::TextureDesc()
			.setDimension(nvrhi::TextureDimension::Texture2D)
			.setFormat(fmt)
			.setWidth(width)
			.setHeight(height)
			.setIsRenderTarget(true)
			.setDebugName("Swap Chain Image");

		// In this line, <type> depends on the GAPI and should be one of: D3D11_Resource, D3D12_Resource, VK_Image.
		nvrhi::TextureHandle swapChainTexture = m_nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, swapchain_images[it].image, textureDesc);

		m_swapchainImages.push_back(swapChainTexture);
	}
	create_framebuffers();
}
