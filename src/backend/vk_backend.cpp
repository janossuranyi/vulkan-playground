#pragma once

#include "pch.h"
#include "vk.h"
#include "vk_backend.h"
#include "vk_command_pool.h"
#include "vk_command_buffer.h"
#include "vk_pipeline.h"
#include "vk_buffer.h"
#include "vk_initializers.h"
#include "vk_barrier.h"
#include "renderer/gli_utils.h"

#include <jsrlib/jsr_resources.h>
#include <jsrlib/jsr_logger.h>

#include <SDL_vulkan.h>
#include <random>
#include <spirv_reflect.h>

const uint64_t ONE_SEC = 1000000000ULL;

namespace fs = std::filesystem;

namespace vks {

VulkanBackend::~VulkanBackend()
{
	quit();
}

bool VulkanBackend::init(SDL_Window* window, int concurrency)
{

	m_window = window;
	m_drawcallCommandPools.resize(concurrency);

	int x, y;
	SDL_GetWindowSize(window, &x, &y);
	m_vulkanContext.windowExtent.width = (unsigned)x;
	m_vulkanContext.windowExtent.height = (unsigned)y;

	init_vulkan();
	init_swapchain();
	init_commands();
	init_sync_structures();
	init_default_renderpass();
	init_framebuffers();
	init_buffers();
	init_samplers();

	init_global_texture_array_layout();
	init_global_texture_array_descriptor();
	init_descriptors();
	init_pipelines();

	onStartup();

	init_images();


	return true;
}

void VulkanBackend::renderFrame(uint32_t frame,bool present)
{
	static bool first = true;

	using namespace glm;
	const auto currentFrame = m_currentFrame % INFLIGHT_FRAMES;
	const auto frameNumber = m_currentFrame.fetch_add(1);
	auto& frameData = m_frameData[currentFrame];

	const uint64_t TimeOut = 32 * 1000000;
	VK_CHECK(vkWaitForFences(*device, 1, &frameData.renderFence, true, ONE_SEC));

	if (!frameData.deferredCommands.empty())
	{
		for (auto& it : frameData.deferredCommands)
		{
			std::visit(*this, it);
		}
		frameData.deferredCommands.clear();
	}

	uint32_t nextImage;
	auto acquaireResult = vkAcquireNextImageKHR(*device, m_vulkanContext.swapchain, ONE_SEC, frameData.presentSemaphore, nullptr, &nextImage);

	if (acquaireResult == VK_ERROR_OUT_OF_DATE_KHR || acquaireResult == VK_SUBOPTIMAL_KHR || m_windowResized) {
		m_windowResized = false;
		recreate_swapchain();
		return;
	}
	else if (acquaireResult != VK_SUCCESS) {
		jsrlib::Error("vkAcquireNextImageKHR failed");
		return;
	}

	VK_CHECK(vkResetFences(*device, 1, &frameData.renderFence));
	VK_CHECK(vkResetCommandBuffer(frameData.commandBuffer, 0));

	auto cmd = frameData.commandBuffer;
	const float w = float(m_vulkanContext.windowExtent.width);
	const float h = float(m_vulkanContext.windowExtent.height);

	m_globalShaderData.g_modelMatrix = glm::rotate(mat4(1.0f), float(frameNumber) / 128.f, vec3(0.0f, 0.0f, 1.0f));
	m_globalShaderData.g_viewMatrix = glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -2.5f + 2.0f * glm::sin(float(frameNumber) / 256.f)));
	m_globalShaderData.g_frameCount = float(frameNumber);
	m_globalShaderData.g_windowExtent = vec4(w, h, 1.0f / w, 1.0f / h);
	m_globalShaderData.g_projectionMatrix = glm::perspective(glm::radians(45.f), (float)m_vulkanContext.windowExtent.width / (float)m_vulkanContext.windowExtent.height, 0.1f, 10.f);


	void* ptr = nullptr;
	vmaMapMemory(*device, frameData.globalUbo.memory, &ptr);
	assert(ptr);
	memcpy(ptr, &m_globalShaderData, sizeof(m_globalShaderData));
	vmaUnmapMemory(*device, frameData.globalUbo.memory);

	beginCommandBuffer(cmd);

	{
		VkClearValue clearVal{};
		VkClearValue clearDepth{};

		float flash = glm::abs(glm::sin(frameNumber / 64.f));
		clearVal.color = { {flash, flash, flash, 1.0f} };
		clearDepth.depthStencil.depth = 1.0f;
		VkClearValue clears[] = { clearVal,clearDepth };
		
		VkRenderPassBeginInfo rpbi{};
		rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpbi.renderPass = m_defaultRenderPass;
		rpbi.renderArea.extent = m_vulkanContext.windowExtent;
		rpbi.framebuffer = m_framebuffers[nextImage];
		rpbi.clearValueCount = 2;
		rpbi.pClearValues = clears;
		vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
	}
	//--- draw fancy objects ---
	
	VkDescriptorSet dsets[] = { m_globalShaderDataDescriptors.sets[currentFrame],m_globalTextureArrayDescriptors.sets[0] };
	VkViewport vp{ 0.0f,0.0f,w,h,0.0f,1.0f };
	VkRect2D scissor{};
	scissor.extent = { m_vulkanContext.windowExtent.width, m_vulkanContext.windowExtent.height };

	vkCmdSetViewport(cmd, 0, 1, &vp);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_defaultPipelineLayout, 0, 2, dsets, 0, nullptr);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_defaultPipeline);
	VkDeviceSize offset = 0;

	//--- draw fancy objects ---

	for (const auto& it : m_renderQueue) {
		const Mesh& mesh = m_meshes[it.handle.index];

		FragmentPushConstants pc;
		pc.texture0 = it.textures[0];
		pc.texture1 = m_globalImages["default_white"].globalArrayIndex;
		vkCmdPushConstants(cmd, m_defaultPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(FragmentPushConstants), &pc);
		vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer.handle, &offset);
		vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.handle, offset, mesh.indexType);
		vkCmdDrawIndexed(cmd, it.indexCount, 1,it.firstIndex, it.vertexOffset, 0);
	}
	m_renderQueue.clear();

	vkCmdEndRenderPass(cmd);
	VK_CHECK(vkEndCommandBuffer(cmd));

	//--- submit commands ---
	VkPipelineStageFlags psf = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &frameData.renderSemaphore;
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &frameData.presentSemaphore;
	si.pWaitDstStageMask = &psf;

	VK_CHECK(vkQueueSubmit(m_graphicQueue, 1, &si, frameData.renderFence));

	if (present) {
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_vulkanContext.swapchain;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &frameData.renderSemaphore;
		presentInfo.pImageIndices = &nextImage;

		VK_CHECK(vkQueuePresentKHR(m_graphicQueue, &presentInfo));
	}
}

ImageHandle VulkanBackend::createImageFromFile(const std::filesystem::path& filename, const std::string& name)
{
	ImageHandle result;
	if (get_image_by_name(filename.u8string(), &result, nullptr)) return result;

	gli::texture texture = gli::load(filename.u8string());
	if (texture.empty()) {
		return ImageHandle();
	}

	return createImageFromGliTexture(name, texture);
}

ImageHandle VulkanBackend::createImageFromGliTexture(const std::string& name, const gli::texture& gliTexture)
{
	auto result = ImageHandle();
	Image newTexture;
	newTexture.name = name;

	if (VK_SUCCESS == create_texture_internal(gliTexture, newTexture)) {
		result.index = insert_image_to_list(newTexture);
	}

	return result;
}

ImageHandle VulkanBackend::createImage(const ImageDescription& desc, VkImageLayout initialLayout)
{
	VkImageUsageFlags usage = {};
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	Image res = {};
	ImageHandle result = {};
	const bool isDepth = isDepthFormat(desc.format);
	const bool isStencil = isStencilFormat(desc.format);

	if (bool(desc.usageFlags & ImageUsageFlags::Sampled)) {
		usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (bool(desc.usageFlags & ImageUsageFlags::Storage)) {
		usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		layout = VK_IMAGE_LAYOUT_GENERAL;
	}
	if (bool(desc.usageFlags & ImageUsageFlags::Attachment)) {
		usage |= (isDepth||isStencil) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	uint32_t mipCount = 1;
	if (desc.mipCount == MipCount::Manual) {
		mipCount = desc.manualMipCount;
	}
	else if (desc.mipCount == MipCount::FullChain) {
		mipCount = (uint32_t)(std::floor(std::log2(std::max(desc.width, desc.height))));
	}
	VkExtent3D extent;
	extent.width = desc.width;
	extent.height = desc.height;
	extent.depth = desc.depth;

	auto imageCI = vkinitializer::image_create_info(desc.format, usage, extent);
	imageCI.arrayLayers = desc.cubeMap ? 6 : 1;
	imageCI.imageType = desc.type;
	imageCI.mipLevels = mipCount;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (desc.cubeMap) {
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	VkResult vkResult = vmaCreateImage(*device, &imageCI, &dimg_allocinfo, &res.vulkanImage, &res.memory, nullptr);
	VK_CHECK(vkResult);

	if (vkResult) {
		jsrlib::Error("vmaCreateImage failed!");
		return result;
	}

	res.format = desc.format;
	res.extent = extent;
	res.layers = imageCI.arrayLayers;
	res.levels = mipCount;
	res.device = device;
	res.isCubemap = desc.cubeMap;

	VkImageAspectFlags aspect = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	if (isStencil) { aspect |= VK_IMAGE_ASPECT_STENCIL_BIT; }

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
	switch (desc.type) {
	case VK_IMAGE_TYPE_1D:
		viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case VK_IMAGE_TYPE_2D:
		viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case VK_IMAGE_TYPE_3D:
		viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	default:
		assert(0);
	}

	if (mipCount > 1)
	{
		for (uint32_t layer = 0; layer < res.layers; ++layer)
		{
			for (uint32_t i = 0; i < mipCount; ++i)
			{
				VkImageViewCreateInfo info{};
				info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				info.viewType = viewType;
				info.format = desc.format;
				info.subresourceRange.aspectMask = aspect;
				info.subresourceRange.baseArrayLayer = layer;
				info.subresourceRange.layerCount = 1;
				info.subresourceRange.baseMipLevel = i;
				info.subresourceRange.levelCount = 1;
				info.image = res.vulkanImage;
				res.layerViews.push_back({});
				VK_CHECK(vkCreateImageView(*device, &info, nullptr, &res.layerViews.back()));
			}
		}
	}

	VkAccessFlags dstAccessMask = {};
	VkPipelineStageFlags dstStage = {};
	if (initialLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstStage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if (initialLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
		dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dstStage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	}

	if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		immediate_submit([&](VkCommandBuffer cmd) {
			VkImageSubresourceRange range = {};
			range.aspectMask = aspect;
			range.baseArrayLayer = 0;
			range.baseMipLevel = 0;
			range.layerCount = 1;
			range.levelCount = mipCount;

			ImageBarrier ibarrier;
			ibarrier.image(res.vulkanImage)
				.oldLayout(VK_IMAGE_LAYOUT_UNDEFINED)
				.newLayout(initialLayout)
				.subresourceRange(range)
				.accessMasks(VK_ACCESS_NONE, dstAccessMask);

			PipelineBarrier barrier(ibarrier);
			barrier.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, dstStage);
			});
	}

	auto view_create_info = vkinitializer::imageview_create_info(res.format, res.vulkanImage, res.levels, aspect);

	view_create_info.subresourceRange.layerCount = res.layers;
	view_create_info.viewType = res.isCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : viewType;
	VK_CHECK(vkCreateImageView(device->vkDevice, &view_create_info, nullptr, &res.view));

	if (bool(desc.usageFlags & ImageUsageFlags::Sampled)) {
		add_texture_to_global_array(res);
	}

	result.index = insert_image_to_list(res);

	return result;
}

SamplerHandle VulkanBackend::createSampler(const VkSamplerCreateInfo& samplerCI)
{
	SamplerHandle res = {};
	Sampler samp;
	samp.info = samplerCI;
	VK_CHECK(vkCreateSampler(*device, &samplerCI, nullptr, &samp.vulkanSampler));
	res.index = static_cast<unsigned>( m_samplers.size() );
	m_samplers.push_back(samp);

	return res;
}

void VulkanBackend::uploadImage(ImageHandle hImage, uint32_t layer, uint32_t level, const void* data, size_t size)
{
	assert(hImage.index != invalidIndex);
	assert(hImage.index < m_textures.list.size());
	Image& image = m_textures.list[hImage.index];


	Buffer staging_buffer;

	VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, size, &staging_buffer, nullptr));

	VK_CHECK(staging_buffer.map());
	staging_buffer.copyTo(data, VkDeviceSize(size));
	staging_buffer.unmap();

	VkCommandBuffer cmd;
	//VK_CHECK(device->createCommandPool(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &pool));
	VK_CHECK(device->createCommandBuffer(m_uploadContext.commandPool, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, false, &cmd, true /* begin command buffer */));
	{
		if (image.layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			ImageBarrier img_barrier;

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.levelCount = image.levels;
			subresourceRange.layerCount = image.layers;

			img_barrier.image(image.vulkanImage)
				.layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
				.subresourceRange(subresourceRange)
				.accessMasks(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT);

			PipelineBarrier to_transfer(img_barrier);
			to_transfer.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			image.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = level;
		bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent = image.extent;
		bufferCopyRegion.bufferOffset = 0;

		vkCmdCopyBufferToImage(
			cmd,
			staging_buffer.handle,
			image.vulkanImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion
		);

		device->flushCommandBuffer(cmd, m_graphicQueue, m_uploadContext.commandPool, true /* destroy cmdBuffer */);
	}
	staging_buffer.destroy();
	
}

void VulkanBackend::updateGraphicPassDescriptorSet(GraphicsPassHandle hPass, const RenderPassResources& resources)
{
	GraphicsPass& pass = m_graphicsPasses.at(hPass.index);
	for (unsigned i = 0; i < INFLIGHT_FRAMES; ++i) {
		updateDescriptorSet(pass.descriptorSetLayaout, pass.descriptorSets[i], resources);
	}
}

void VulkanBackend::updateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet set, const RenderPassResources& resources)
{
	std::vector<VkWriteDescriptorSet> wds;

	for (unsigned i = 0; i < (unsigned)resources.storageImages.size(); ++i) {
		const auto& it = resources.storageImages[i];
		const Image& img = get_imageref(it.image);

		VkDescriptorImageInfo imgInfo = {};
		imgInfo.imageLayout = img.layout;
		imgInfo.imageView = it.level ? img.layerViews[it.level] : img.view;
		VkWriteDescriptorSet w = vkinitializer::write_descriptor_image(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, set, &imgInfo, it.binding, 0);
		wds.push_back(w);
	}
	for (unsigned i = 0; i < (unsigned)resources.sampledImages.size(); ++i) {
		const auto& it = resources.sampledImages[i];
		const Image& img = get_imageref(it.image);

		VkDescriptorImageInfo imgInfo = {};
		imgInfo.imageLayout = img.layout;
		imgInfo.imageView = it.level ? img.layerViews[it.level] : img.view;
		VkWriteDescriptorSet w = vkinitializer::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, set, &imgInfo, it.binding, 0);
		wds.push_back(w);
	}
	for (unsigned i = 0; i < (unsigned)resources.samplers.size(); ++i) {
		const auto& it = resources.samplers[i];
		VkDescriptorImageInfo imgInfo = {};
		const Sampler& samp = m_samplers.at(it.sampler.index);
		imgInfo.sampler = samp.vulkanSampler;

		VkWriteDescriptorSet w = {};
		w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		w.dstBinding = it.binding;
		w.dstSet = set;
		w.pImageInfo = &imgInfo;
		wds.push_back(w);
	}
	for (unsigned i = 0; i < (unsigned)resources.storageBuffers.size(); ++i) {
		const auto& it = resources.storageBuffers[i];
		const Buffer& buf = m_buffers.list.at(it.buffer.index);
		VkDescriptorBufferInfo bufInfo = {};
		bufInfo.buffer = buf.handle;
		bufInfo.offset = 0;
		bufInfo.range = buf.size;
		VkWriteDescriptorSet w = vkinitializer::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, set, &bufInfo, it.binding);
		wds.push_back(w);
	}
	for (unsigned i = 0; i < (unsigned)resources.uniformBuffers.size(); ++i) {
		const auto& it = resources.uniformBuffers[i];
		const Buffer& buf = m_buffers.list.at(it.buffer.index);
		VkDescriptorBufferInfo bufInfo = {};
		bufInfo.buffer = buf.handle;
		bufInfo.offset = 0;
		bufInfo.range = buf.size;
		VkWriteDescriptorSet w = vkinitializer::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, set, &bufInfo, it.binding);
		wds.push_back(w);
	}

	vkUpdateDescriptorSets(*device, static_cast<unsigned>(wds.size()), wds.data(), 0, nullptr);
}

GraphicsPassHandle VulkanBackend::createGraphicsPass(const GraphicsPassDescription& desc)
{
	GraphicsPass pass;
	GraphicsPassHandle res = {};
	if (create_graphics_pass_internal(desc, &pass))
	{
		res.index = (uint32_t)m_graphicsPasses.size();
		m_graphicsPasses.push_back(pass);
	}

	return res;
}

bool VulkanBackend::create_graphics_pass_internal(const GraphicsPassDescription& desc, GraphicsPass* out)
{
	VkGraphicsPipelineCreateInfo pipelineCI = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	GraphicPassShaderDescriptions shaderDescr = {};
	shaderDescr.vertex.path = desc.shaderDescriptions.vertex.path;
	shaderDescr.fragment.path = desc.shaderDescriptions.fragment.path;
	shaderDescr.geometry = desc.shaderDescriptions.geometry;

	GraphicShadersHandle shadersHandle = m_shaders.createGraphicsShader(shaderDescr);
	if (shadersHandle.index == invalidIndex)
	{
		return false;
	}

	out->shaderHandle = shadersHandle;
	GraphichPipelineShaderBinary shaders = {};
	m_shaders.getGraphicsShaders(shadersHandle, &shaders);
	VkShaderModuleCreateInfo smCI = {};
	VkShaderModule vertSM,fragSM;
	smCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smCI.codeSize = shaders.vertexSPIRV.size();
	smCI.pCode = (uint32_t*)shaders.vertexSPIRV.data();
	VK_CHECK(vkCreateShaderModule(*device, &smCI, nullptr, &vertSM));
	smCI.codeSize = shaders.fragmentSPIRV.size();
	smCI.pCode = (uint32_t*)shaders.fragmentSPIRV.data();
	VK_CHECK(vkCreateShaderModule(*device, &smCI, nullptr, &fragSM));

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	VkPipelineShaderStageCreateInfo stageCI = {};
	stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCI.module = vertSM;
	stageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
	stageCI.pName = "main";
	stages.push_back(stageCI);
	stageCI.module = fragSM;
	stageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages.push_back(stageCI);

	pipelineCI.stageCount = (uint32_t)stages.size();
	pipelineCI.pStages = stages.data();
	
	VkPipelineVertexInputStateCreateInfo vertexInputCI = vkinitializer::vertex_input_state_create_info();
	VertexInputDescription vertexInputDesc;
	if (desc.vertexAttribFormat == VertexAttribFormat::Full)
	{
		vertexInputDesc = Vertex::vertex_input_description();
	}
	else
	{
		vertexInputDesc = Vertex::vertex_input_description_position_only();
	}

	vertexInputCI.vertexBindingDescriptionCount = (uint32_t) vertexInputDesc.bindings.size();
	vertexInputCI.pVertexBindingDescriptions = vertexInputDesc.bindings.data();
	vertexInputCI.vertexAttributeDescriptionCount = (uint32_t)vertexInputDesc.attributes.size();
	vertexInputCI.pVertexAttributeDescriptions = vertexInputDesc.attributes.data();
	pipelineCI.pVertexInputState = &vertexInputCI;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = vkinitializer::input_assembly_create_info(convertTopology(desc.inputAssembly.topology));
	pipelineCI.pInputAssemblyState = &inputAssemblyCI;

	VkPipelineRasterizationStateCreateInfo rasterizationCI = vkinitializer::rasterization_state_create_info(convertPolygonMode(desc.rasterization.mode));
	rasterizationCI.cullMode = convertCullMode(desc.rasterization.cullMode);
	rasterizationCI.frontFace = convertFrontFace(desc.rasterization.frontFace);
	rasterizationCI.lineWidth = 1.0f;
	pipelineCI.pRasterizationState = &rasterizationCI;

	VkPipelineMultisampleStateCreateInfo multisampleCI = vkinitializer::multisampling_state_create_info();
	pipelineCI.pMultisampleState = &multisampleCI;

	VkPipelineDepthStencilStateCreateInfo depthStencilCI = vkinitializer::depth_stencil_create_info(false, false, VK_COMPARE_OP_ALWAYS);
	depthStencilCI.depthCompareOp = convertCompareOp(desc.depthtest.depthFunction);
	depthStencilCI.depthTestEnable = desc.depthtest.depthFunction != CompareOp::None;
	depthStencilCI.depthWriteEnable = desc.depthtest.depthWrite;
	pipelineCI.pDepthStencilState = &depthStencilCI;

	VkPipelineColorBlendStateCreateInfo blendCI = {};
	VkPipelineColorBlendAttachmentState attachment = vkinitializer::color_blend_attachment_state();
	std::vector<VkPipelineColorBlendAttachmentState> attachments;

	blendCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendCI.attachmentCount = (uint32_t) desc.attachments.size();
	for (size_t i = 0; i < desc.attachments.size(); ++i)
	{
		if (isDepthFormat(desc.attachments[i].format)) continue;

		attachment.blendEnable = desc.blend.blendOp != BlendOp::None;
		attachment.alphaBlendOp = convertBlendOp(desc.blend.blendOp);
		attachment.colorBlendOp = convertBlendOp(desc.blend.blendOp);
		VkColorComponentFlags writeMask = {};
		if (desc.blend.writeMask[0])	writeMask |= VK_COLOR_COMPONENT_R_BIT;
		if (desc.blend.writeMask[1])	writeMask |= VK_COLOR_COMPONENT_G_BIT;
		if (desc.blend.writeMask[2])	writeMask |= VK_COLOR_COMPONENT_B_BIT;
		if (desc.blend.writeMask[3])	writeMask |= VK_COLOR_COMPONENT_A_BIT;
		attachment.colorWriteMask = writeMask;
		attachment.srcColorBlendFactor = convertBlendFactor(desc.blend.srcBlendFactor);
		attachment.srcAlphaBlendFactor = convertBlendFactor(desc.blend.srcBlendFactor);
		attachment.dstColorBlendFactor = convertBlendFactor(desc.blend.dstBlendFactor);
		attachment.dstAlphaBlendFactor = convertBlendFactor(desc.blend.dstBlendFactor);
		attachments.push_back(attachment);
	}
	blendCI.pAttachments = attachments.data();
	blendCI.logicOp = VK_LOGIC_OP_COPY;
	blendCI.logicOpEnable = VK_FALSE;
	pipelineCI.pColorBlendState = &blendCI;

	VkPipelineDynamicStateCreateInfo dynStateCI = {};
	const VkDynamicState dynState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	dynStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynStateCI.dynamicStateCount = 2;
	dynStateCI.pDynamicStates = dynState;
	pipelineCI.pDynamicState = &dynStateCI;
	
	VkPipelineLayoutCreateInfo layoutCI = vkinitializer::pipeline_layout_create_info();
	std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
	VkDescriptorSetLayoutBinding binding = {};
	VkDescriptorSetLayoutCreateInfo setCI = {};
	setCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

	for (uint32_t index = 0; index < (uint32_t) desc.layout.uniformBufferBindings.size(); ++index)
	{
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		binding.binding = desc.layout.uniformBufferBindings[index];
		binding.descriptorCount = 1;
		descriptorBindings.push_back(binding);
	}
	for (uint32_t index = 0; index < (uint32_t)desc.layout.storageBufferBindings.size(); ++index)
	{
		binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		binding.binding = desc.layout.storageBufferBindings[index];
		binding.descriptorCount = 1;
		descriptorBindings.push_back(binding);
	}
	for (uint32_t index = 0; index < (uint32_t)desc.layout.sampledImageBindings.size(); ++index)
	{
		binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		binding.binding = desc.layout.sampledImageBindings[index];
		binding.descriptorCount = 1;
		descriptorBindings.push_back(binding);
	}
	for (uint32_t index = 0; index < (uint32_t)desc.layout.samplerBindings.size(); ++index)
	{
		binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		binding.binding = desc.layout.samplerBindings[index];
		binding.descriptorCount = 1;
		descriptorBindings.push_back(binding);
	}
	for (uint32_t index = 0; index < (uint32_t)desc.layout.storageImageBindings.size(); ++index)
	{
		binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
		binding.binding = desc.layout.storageImageBindings[index];
		binding.descriptorCount = 1;
		descriptorBindings.push_back(binding);
	}
	setCI.bindingCount = (uint32_t) descriptorBindings.size();
	setCI.pBindings = descriptorBindings.data();
	VK_CHECK(vkCreateDescriptorSetLayout(*device, &setCI, nullptr, &out->descriptorSetLayaout));

	std::array<VkDescriptorSetLayout, 3> layout = 
		{ m_globalShaderDataDescriptors.layout,out->descriptorSetLayaout,m_globalTextureArrayDescriptors.layout };

	layoutCI.setLayoutCount = 3;
	layoutCI.pSetLayouts = layout.data();

	VkPushConstantRange pcRanges;
	pcRanges.offset = 0;
	pcRanges.size = desc.pushConstantsSize;
	pcRanges.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	if (desc.shaderDescriptions.geometry.has_value())
	{
		pcRanges.stageFlags != VK_SHADER_STAGE_GEOMETRY_BIT;
	}
	if (pcRanges.size > 0)
	{
		layoutCI.pushConstantRangeCount = 1;
		layoutCI.pPushConstantRanges = &pcRanges;
	}

	VK_CHECK(vkCreatePipelineLayout(*device, &layoutCI, nullptr, &out->pipelineLayout));

	std::vector<VkAttachmentDescription> colorAttachments;
	std::vector<VkAttachmentReference> colorRefs;
	VkAttachmentReference depthRef = {};

	for (uint32_t i = 0; i < (uint32_t) desc.attachments.size(); ++i)
	{
		const auto& it = desc.attachments[i];
		const bool isDepth = isDepthFormat(it.format);

		VkAttachmentDescription attachment = {};
		attachment.format = it.format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = it.loadOp;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachments.push_back(attachment);
		if (isDepth)
		{
			depthRef.attachment = i;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		}
		else
		{
			VkAttachmentReference color_ref{};
			color_ref.attachment = i;
			color_ref.layout = attachment.finalLayout;
			colorRefs.push_back(color_ref);
		}
	}

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = (uint32_t) colorRefs.size();
	subpass.pColorAttachments = colorRefs.data();
	if (depthRef.layout)
	{
		subpass.pDepthStencilAttachment = &depthRef;
	}
	VkSubpassDependency color_dependency{};
	color_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	color_dependency.dstSubpass = 0;
	color_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	color_dependency.srcAccessMask = VK_ACCESS_NONE;
	color_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	color_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency depth_dependency{};
	depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depth_dependency.dstSubpass = 0;
	depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.srcAccessMask = VK_ACCESS_NONE;
	depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	const VkSubpassDependency deps[]{ color_dependency,depth_dependency };
	VkRenderPassCreateInfo rpci{};
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = (uint32_t)colorAttachments.size();
	rpci.pAttachments = colorAttachments.data();
	rpci.subpassCount = 1;
	rpci.pSubpasses = &subpass;
	rpci.dependencyCount = 2;
	rpci.pDependencies = deps;

	if (colorAttachments.empty() == false || depthRef.layout) {
		VK_CHECK(vkCreateRenderPass(*device, &rpci, nullptr, &out->renderPass));
	}

	pipelineCI.renderPass = out->renderPass;
	pipelineCI.layout = out->pipelineLayout;

	VkViewport viewport = {};
	viewport.width = m_vulkanContext.windowExtent.width;
	viewport.height = m_vulkanContext.windowExtent.height;
	VkRect2D scissor = {};
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.pScissors = &scissor;
	pipelineCI.pViewportState = &viewportState;

	VK_CHECK(vkCreateGraphicsPipelines(*device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &out->pipeline));

	for (const auto& it : stages)
	{
		vkDestroyShaderModule(*device, it.module, nullptr);
	}

	return true;
}

void VulkanBackend::freeImage(ImageHandle textureHandle)
{
	Image& tex = get_imageref(textureHandle);
	free_global_texture(tex.globalIndex);
	{
		std::lock_guard<std::mutex> lck(m_textures.mutex);
		m_textures.list[textureHandle.index] = {};
		m_textures.freeSlots.push_back(textureHandle.index);
		m_textures.name_to_index.erase(tex.name);
	}
	vkDestroyImageView(device->vkDevice, tex.view, nullptr);
	for (const auto& it : tex.layerViews)
	{
		vkDestroyImageView(device->vkDevice, it, nullptr);
	}
	vmaDestroyImage(device->allocator, tex.vulkanImage, tex.memory);
}

uint32_t VulkanBackend::insert_image_to_list(const Image& newTexture)
{
	std::lock_guard<std::mutex> lck(m_textures.mutex);
	uint32_t index;
	if (m_textures.freeSlots.empty() == false) {
		index = m_textures.freeSlots.back();
		m_textures.freeSlots.pop_back();
		m_textures.list[index] = newTexture;
	}
	else {
		index = static_cast<unsigned>(m_textures.list.size());
		m_textures.list.push_back(newTexture);
	}

	m_textures.name_to_index.emplace(newTexture.name, index);

	return index;
}

BufferHandle VulkanBackend::createUniformBuffer(const UniformBufferDescription& ci)
{
	Buffer newBuffer{};
	BufferHandle handle{};

	VK_CHECK(create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, ci.size, &newBuffer, nullptr));
	handle.index = static_cast<unsigned>(m_buffers.list.size());
	m_buffers.list.push_back(newBuffer);

	if (ci.initialData) {
		transfer_buffer_immediate(newBuffer, ci.size, ci.initialData);
	}

	return handle;
}

BufferHandle VulkanBackend::createStorageBuffer(const StorageBufferDescription& ci)
{
	Buffer result = {};
	BufferHandle handle = {};
	VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, ci.size, &result, nullptr));

	handle.index = static_cast<unsigned>(m_buffers.list.size());
	m_buffers.list.push_back(result);

	if (ci.initialData) {
		transfer_buffer_immediate(result, ci.size, ci.initialData);
	}

	return handle;

}

std::vector<MeshHandle> VulkanBackend::createMeshes(const std::vector<MeshDescription>& descriptions)
{
	std::vector<MeshHandle> result;

	VkCommandPool pool;
	VK_CHECK(device->createCommandPool(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &pool));
	VkCommandBuffer cmd;
	VK_CHECK(device->createCommandBuffer(pool, 0, false, &cmd, true));

	for (const auto& ci : descriptions) {

		assert(ci.vertexCount > 0);
		assert(ci.vertexBuffer.size() > 0);

		Mesh mesh(ci.vertexCount, ci.indexCount);
		MeshHandle handle;

		if (ci.indexCount > std::numeric_limits<uint16_t>::max()) {
			mesh.indexType = VK_INDEX_TYPE_UINT32;
		}

		VkDeviceSize size = ci.vertexBuffer.size();
		Buffer vertexBuffer{};
		Buffer indexBuffer{};
		
		VK_CHECK(create_buffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			size,
			&vertexBuffer,
			nullptr));
		
		
		m_buffers.list.push_back(vertexBuffer);
		mesh.vertexBuffer = vertexBuffer;
		

		Buffer vertexStageBuffer{};
		Buffer indexStageBuffer{};
		VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, size, &vertexStageBuffer, ci.vertexBuffer.data()));
		
		VkBufferCopy copyInfo{};
		copyInfo.size = size;
		vkCmdCopyBuffer(cmd, vertexStageBuffer.handle, vertexBuffer.handle, 1, &copyInfo);
		if (ci.indexCount > 0) {
			size = ci.indexBuffer.size() * sizeof(uint16_t);
			VK_CHECK(create_buffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VMA_MEMORY_USAGE_GPU_ONLY,
				size,
				&indexBuffer,
				nullptr));

			VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, size, &indexStageBuffer, ci.indexBuffer.data()));
			copyInfo.size = size;

			vkCmdCopyBuffer(cmd, indexStageBuffer.handle, indexBuffer.handle, 1, &copyInfo);
#if 0
			VkBufferMemoryBarrier to_transerBarrier{};
			to_transerBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			to_transerBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			to_transerBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			to_transerBarrier.offset = 0;
			to_transerBarrier.srcQueueFamilyIndex = vkDevice->queueFamilyIndices.graphics;
			to_transerBarrier.dstQueueFamilyIndex = vkDevice->queueFamilyIndices.graphics;
			to_transerBarrier.buffer = indexBuffer.vulkanImage;
			to_transerBarrier.size = size;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &to_transerBarrier, 0, nullptr);
#endif

			m_buffers.list.push_back(indexBuffer);
			mesh.indexBuffer = indexBuffer;
		}

		device->flushCommandBuffer(cmd, m_graphicQueue, pool, false);
		vkResetCommandBuffer(cmd, 0);

		vertexStageBuffer.destroy();
		if (indexStageBuffer.handle) {
			indexStageBuffer.destroy();
		}

		handle.index = static_cast<uint32_t>(m_meshes.size());
		m_meshes.push_back(mesh);
		result.push_back(handle);
	}

	vkFreeCommandBuffers(*device, pool, 1, &cmd);
	vkDestroyCommandPool(*device, pool, nullptr);

	return result;
}

uint32_t VulkanBackend::getTextureGlobalIndex(ImageHandle textureHandle)
{
	return get_imageref(textureHandle).globalIndex;
}

void VulkanBackend::setTestBuffer(BufferHandle handle)
{
	testBuffer = handle;
}

void VulkanBackend::postDestroyTexture(ImageHandle hTexture)
{
	DeferredCommand cmd = DeferredDestroyTexture{ hTexture };
	const uint32_t frame = getFlightFrame();
	m_frameData[frame].deferredCommands.push_back(cmd);
}

void VulkanBackend::addRenderMesh(const MeshRenderInfo& h)
{
	m_renderQueue.push_back(h);
}

void VulkanBackend::windowResized(void)
{
	m_windowResized = true;
}

VkSubresourceLayout VulkanBackend::get_subresource_layout(VkImage image, const VkImageSubresource& subresource) {

	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(*device, image, &subresource, &layout);

	return layout;
}

void VulkanBackend::operator()(const DeferredDestroyTexture& cmd)
{
	jsrlib::Info("Texture %d destroyed", cmd.handle);
}

void VulkanBackend::quit()
{
	std::cout << "Exiting... Cleaning resources" << std::endl;

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(vkWaitForFences(*device, 1, &m_frameData[i].renderFence, true, ONE_SEC));
	}

	vkDeviceWaitIdle(*device);

	m_deletions.flush();

	for (auto& it : m_graphicsPasses) {
		vkDestroyPipeline(*device, it.pipeline, nullptr);
		vkDestroyPipelineLayout(*device, it.pipelineLayout, nullptr);
		vkDestroyRenderPass(*device, it.renderPass, nullptr);
		vkDestroyDescriptorSetLayout(*device, it.descriptorSetLayaout, nullptr);
	}
	m_graphicsPasses.clear();

	for (auto& it : m_buffers.list) {
		vmaDestroyBuffer(device->allocator, it.handle, it.memory);
	}
	m_buffers.list.clear();

	for (auto& tex : m_textures.list) {
		if (tex.sampler) {
			vkDestroySampler(*device, tex.sampler, nullptr);
		}
		if (tex.view) {
			vkDestroyImageView(*device, tex.view, nullptr);
		}
		for (auto view : tex.layerViews) {
			vkDestroyImageView(*device, view, nullptr);
		}
		vmaDestroyImage(device->allocator, tex.vulkanImage, tex.memory);
	}

	m_textures.list.clear();

	for (auto& it : m_samplers) {
		vkDestroySampler(*device, it.vulkanSampler, nullptr);
	}
	m_samplers.clear();

	vkDestroyCommandPool(*device, m_commandPool, nullptr);
	vkDestroyCommandPool(*device, m_uploadContext.commandPool, nullptr);
	for (uint32_t i = 0; i < m_drawcallCommandPools.size(); ++i)
	{
		vkDestroyCommandPool(*device, m_drawcallCommandPools[i], nullptr);
	}

	cleanup_swapchain();

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		vkDestroyFence(*device, m_frameData[i].renderFence, nullptr);
		vkDestroySemaphore(*device, m_frameData[i].presentSemaphore, nullptr);
		vkDestroySemaphore(*device, m_frameData[i].renderSemaphore, nullptr);
	}
	vkDestroyFence(*device, m_uploadContext.uploadFence, nullptr);

	vmaDestroyAllocator(*device);
	
	delete device;

	m_vulkanContext.cleanup();

}

uint32_t VulkanBackend::getUniformBlockOffsetAlignment() const
{
	return static_cast<uint32_t>(device->properties.limits.minUniformBufferOffsetAlignment);
}

uint32_t VulkanBackend::getAlignedUniformBlockOffset(uint32_t offset) const
{
	const uint32_t aligment = getUniformBlockOffsetAlignment() - 1;
	return (offset + aligment) & ~aligment;
}

bool VulkanBackend::get_image_by_name(const std::string& name, ImageHandle* outhandle, Image* out)
{
	std::lock_guard<std::mutex> lck(m_textures.mutex);
	auto it = m_textures.name_to_index.find(name);
	if (it != std::end(m_textures.name_to_index)) {
		if (out) *out = m_textures.list[it->second];
		if (outhandle) outhandle->index = it->second;
		return true;
	}

	return false;
}

Image& VulkanBackend::get_imageref(ImageHandle handle)
{
	assert(handle.index < m_textures.list.size());

	return m_textures.list[handle.index];
}

VkResult VulkanBackend::create_texture_internal(const gli::texture& texture, Image& dest)
{
	VkFormat format = gliutils::convert_format(texture.format());

	VkExtent3D extent = gliutils::convert_extent(texture.extent());
	const uint32_t face_total(static_cast<uint32_t>(texture.layers() * texture.faces()));

	auto create_image_info = vkinitializer::image_create_info(format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);
	create_image_info.arrayLayers = face_total;
	create_image_info.mipLevels = static_cast<unsigned>(texture.levels());
	create_image_info.imageType = gliutils::convert_type(texture.target());
	create_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	if (texture.target() == gli::TARGET_CUBE || texture.target() == gli::TARGET_CUBE_ARRAY) {
		create_image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	//allocate and create the image
	VK_CHECK(vmaCreateImage(*device, &create_image_info, &dimg_allocinfo, &dest.vulkanImage, &dest.memory, nullptr));

	VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;

	Buffer staging_buffer;

	VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, texture.size(), &staging_buffer, nullptr));

	const void* data = texture.data();
	VK_CHECK(staging_buffer.map());
	staging_buffer.copyTo(data, VkDeviceSize(texture.size()));
	staging_buffer.unmap();

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	std::vector<VkImageViewCreateInfo> viewCreateInfos;
	for (uint32_t layer = 0; layer < texture.layers(); ++layer) {
		for (uint32_t face = 0; face < texture.faces(); ++face) {

			if (face_total > 1) {
				VkImageViewCreateInfo info{};
				info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				info.format = format;
				info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				info.subresourceRange.baseArrayLayer = static_cast<unsigned>(texture.faces()) * layer + face;
				info.subresourceRange.layerCount = 1;
				info.subresourceRange.baseMipLevel = 0;
				info.subresourceRange.levelCount = static_cast<unsigned>(texture.levels());
				info.image = dest.vulkanImage;
				info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				dest.layerViews.push_back({});
				VK_CHECK(vkCreateImageView(*device, &info, nullptr, &dest.layerViews.back()));
			}

			for (uint32_t level = 0; level < texture.levels(); ++level) {

				const uint32_t layer_index(layer * static_cast<uint32_t>(texture.faces()) + face);
				const glm::ivec3 extent( glm::ivec3( texture.extent( level ) ) );

				const size_t offset = size_t( texture.data( layer, face, level ) ) - size_t( data );
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = layer_index;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent = gliutils::convert_extent(extent);
				bufferCopyRegion.bufferOffset = offset;
				bufferCopyRegions.push_back(bufferCopyRegion);
			}
		}
	}

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = static_cast<unsigned>(texture.levels());
	subresourceRange.layerCount = face_total;

	//VkCommandPool pool;
	VkCommandBuffer cmd;
	//VK_CHECK(device->createCommandPool(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &pool));
	VK_CHECK(device->createCommandBuffer(m_uploadContext.commandPool, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, false, &cmd, true /* begin command buffer */));
	{
		ImageBarrier img_barrier;
		
		img_barrier.image(dest.vulkanImage)
			.layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			.subresourceRange(subresourceRange)
			.accessMasks(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT);

		{
			PipelineBarrier to_transfer(img_barrier);
			to_transfer.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		}

		vkCmdCopyBufferToImage(
			cmd,
			staging_buffer.handle,
			dest.vulkanImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		img_barrier
			.layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.accessMasks(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE);

		PipelineBarrier to_shader(img_barrier);
		to_shader.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

		device->flushCommandBuffer(cmd, m_graphicQueue, m_uploadContext.commandPool, true /* destroy cmdBuffer */);
	}
	//vkDestroyCommandPool(*device, pool, nullptr);

	staging_buffer.destroy();

	auto imageType = gliutils::convert_type(texture.target());
	auto view_create_info = vkinitializer::imageview_create_info(format, dest.vulkanImage, static_cast<unsigned>(texture.levels()), VK_IMAGE_ASPECT_COLOR_BIT);

	view_create_info.subresourceRange.layerCount = face_total;
	view_create_info.viewType = gliutils::convert_view_type(texture.target());
	VK_CHECK(vkCreateImageView(*device, &view_create_info, nullptr, &dest.view));

	dest.extent = extent;
	dest.levels = static_cast<unsigned>(texture.levels());
	dest.layers = static_cast<unsigned>(texture.layers());
	dest.isCubemap = create_image_info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	
	add_texture_to_global_array(dest);


	return VK_SUCCESS;
}

bool VulkanBackend::init_vulkan()
{
#ifdef VKS_USE_VOLK
	VK_CHECK(volkInitialize());
#endif

	vkb::InstanceBuilder builder;
	// init vulkan instance with debug features
	builder.set_app_name("JSR_Vulkan_driver")
		.set_app_version(VK_MAKE_VERSION(1, 0, 0))
		.require_api_version(1, 2, 0);

#if _DEBUG
	builder
		.enable_validation_layers()
		.use_default_debug_messenger();
#endif

	auto inst_ret = builder.build();

	if (!inst_ret) {
		std::cerr << "Failed to create Vulkan instance!" << std::endl;
		return false;
	}

	vkb::Instance vkb_inst = inst_ret.value();
	m_vulkanContext.instance = vkb_inst.instance;
	m_vulkanContext.vkbInstance = vkb_inst;
	m_vulkanContext.debug_messenger = vkb_inst.debug_messenger;

#ifdef VKS_USE_VOLK
	volkLoadInstanceOnly(vkb_inst.instance);
#endif
	VkPhysicalDeviceFeatures features = {};
	features.samplerAnisotropy = VK_TRUE;
	features.fragmentStoresAndAtomics = VK_TRUE;
	features.fillModeNonSolid = VK_TRUE;
	features.depthClamp = VK_TRUE;
	features.geometryShader = VK_TRUE;
	features.textureCompressionBC = VK_TRUE;
	features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	VkPhysicalDeviceVulkan12Features features12 = {}; //vulkan 1.2 features
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.pNext = nullptr;
	features12.hostQueryReset = VK_TRUE;
	features12.runtimeDescriptorArray = VK_TRUE;
	features12.descriptorBindingPartiallyBound = VK_TRUE;
	features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features = {};
	shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	shader_draw_parameters_features.pNext = nullptr;
	shader_draw_parameters_features.shaderDrawParameters = VK_TRUE;

	VkSurfaceKHR surface;
	SDL_bool surf_res = SDL_Vulkan_CreateSurface(m_window, m_vulkanContext.instance, &surface);
	// get the surface of the window we opened with SDL
	if (SDL_TRUE != surf_res) {
		std::cerr << "SDL failed to create Vulkan surface !" << std::endl;
		return false;
	}

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.2
	vkb::PhysicalDeviceSelector selector(vkb_inst);
	auto selector_result = selector
		.set_minimum_version(1, 2)
		.set_surface(surface)
		.set_required_features(features)
		.set_required_features_12(features12)
		.add_required_extension("VK_KHR_synchronization2")
		.add_required_extension_features(shader_draw_parameters_features)
		.select();

	if (!selector_result) {
		throw std::runtime_error("No suitable physical device found !");
	}

	vkb::PhysicalDevice physicalDevice = selector_result.value();

	jsrlib::Info("Device name: %s", physicalDevice.name.c_str());
	jsrlib::Info("API version: %d.%d.%d",
		VK_API_VERSION_MAJOR(physicalDevice.properties.apiVersion),
		VK_API_VERSION_MINOR(physicalDevice.properties.apiVersion),
		VK_API_VERSION_PATCH(physicalDevice.properties.apiVersion));

	device = new VulkanDevice();
	device->init(physicalDevice, m_vulkanContext.instance);

	m_vulkanContext.gpu = device->physicalDevice;
	m_vulkanContext.surface = surface;

	m_graphicQueue = device->getGraphicsQueue();
	m_computeQueue = device->getComputeQueue();
	m_transferQueue = device->getTransferQueue();

	return true;
}
bool VulkanBackend::init_swapchain()
{
	VkSurfaceFormatKHR format{};
	format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	//format.format = VK_FORMAT_R8G8B8A8_UNORM;
	format.format = VK_FORMAT_B8G8R8A8_UNORM;

	VkSurfaceKHR& surface = m_vulkanContext.surface;

	uint32_t sfc{};
	std::vector<VkSurfaceFormatKHR> formats;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkanContext.gpu, surface, &sfc, nullptr);	formats.resize(sfc);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkanContext.gpu, surface, &sfc, formats.data());

	for (auto& it : formats) {
		if (it.format == VK_FORMAT_B8G8R8A8_UNORM || it.format == VK_FORMAT_R8G8B8A8_UNORM) {
			format = it;
			break;
		}
	}

	int x = 0, y = 0;
	SDL_Vulkan_GetDrawableSize(m_window, &x, &y);

	m_vulkanContext.windowExtent.width = static_cast<uint32_t>(x);
	m_vulkanContext.windowExtent.height = static_cast<uint32_t>(y);

	vkb::SwapchainBuilder builder(m_vulkanContext.gpu, *device, surface);
	if (m_vulkanContext.swapchain) {
		builder.set_old_swapchain(m_vulkanContext.vkbSwapchain);
	}


	auto build_res = builder
		.set_desired_format(format)
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.build();

	if (!build_res) {
		std::cerr << "Swapchain initialization failed !" << std::endl;
		return false;
	}

	auto swapchain = build_res.value();

	if (m_vulkanContext.swapchain) {
		cleanup_swapchain();
		m_vulkanContext.vkbSwapchain = swapchain;
	}

	m_vulkanContext.swapchain = swapchain.swapchain;
	m_vulkanContext.swapchainImageFormat = swapchain.image_format;
	m_vulkanContext.swapchainImages = swapchain.get_images().value();
	m_vulkanContext.swapchainImageViews = swapchain.get_image_views().value();
	m_vulkanContext.vkbSwapchain = swapchain;

	return true;
}

bool VulkanBackend::init_commands()
{
	const auto cpci1 = createCommandPoolInfo(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(*device, &cpci1, nullptr, &m_commandPool));
	VK_CHECK(vkCreateCommandPool(*device, &cpci1, nullptr, &m_uploadContext.commandPool));
	for (uint32_t i = 0; i < m_drawcallCommandPools.size(); ++i)
	{
		VK_CHECK(vkCreateCommandPool(*device, &cpci1, nullptr, &m_drawcallCommandPools[i]));
	}

	const auto acbi = commandBufferAllocateInfo(m_commandPool, 1);

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(vkAllocateCommandBuffers(*device, &acbi, &m_frameData[i].commandBuffer));
	}

	const auto acbi2 = commandBufferAllocateInfo(m_uploadContext.commandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(*device, &acbi2, &m_uploadContext.commandBuffer));

	return true;
}
bool VulkanBackend::init_default_renderpass()
{

	VkAttachmentDescription color_attachment {};
	color_attachment.format			= m_vulkanContext.swapchainImageFormat;
	color_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout	= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depth_attachment{};
	depth_attachment.format			= m_depthFormat;
	depth_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentDescription, 2> attachments = { color_attachment ,depth_attachment };

	VkAttachmentReference color_ref{};
	color_ref.attachment			= 0;
	color_ref.layout				= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depth_ref{};
	depth_ref.attachment			= 1;
	depth_ref.layout				= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	VkSubpassDescription subpass {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS; 
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &color_ref;
	subpass.pDepthStencilAttachment = &depth_ref;

	// Setup subpass dependencies
	// These will add the implicit attachment layout transitions specified by the attachment descriptions
	// The actual usage layout is preserved through the layout specified in the attachment reference
	// Each subpass dependency will introduce a memory and execution dependency between the source and dest subpass described by
	// srcStageMask, dstStageMask, srcAccessMask, dstAccessMask (and dependencyFlags is set)
	// Note: VK_SUBPASS_EXTERNAL is a special constant that refers to all commands executed outside of the actual renderpass)
	std::array<VkSubpassDependency, 2> dependencies;

	// Does the transition from final to initial layout for the depth an color attachments
	// Depth attachment
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = 0; // VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = 0;
	// Color attachment
	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = 0;


	VkRenderPassCreateInfo rpci {};
	rpci.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount			= static_cast<uint32_t>(attachments.size());
	rpci.pAttachments				= attachments.data();
	rpci.subpassCount				= 1;
	rpci.pSubpasses					= &subpass;
	rpci.dependencyCount			= static_cast<uint32_t>(dependencies.size());
	rpci.pDependencies				= dependencies.data();

	VK_CHECK(vkCreateRenderPass(*device, &rpci, nullptr, &m_defaultRenderPass));

	m_deletions.push_function([=] { vkDestroyRenderPass(*device, m_defaultRenderPass, nullptr); });

	return true;
}
bool VulkanBackend::init_framebuffers()
{
	VkExtent3D depthExtent = { m_vulkanContext.windowExtent.width, m_vulkanContext.windowExtent.height,1 };
	ImageDescription depthID = {};
	depthID.width		= depthExtent.width;
	depthID.height		= depthExtent.height;
	depthID.depth		= 1;
	depthID.usageFlags	= ImageUsageFlags::Attachment;
	depthID.format		= m_depthFormat;
	depthID.autoCreateMips = false;
	depthID.cubeMap		= false;
	depthID.mipCount	= MipCount::One;
	depthID.type		= VK_IMAGE_TYPE_2D;
	depthID.name		= "swapchain_depth_image";
	m_depthImageHandle = createImage(depthID/*, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL*/);	
	
	const Image& iDepth = get_imageref(m_depthImageHandle);
	
	VkFramebufferCreateInfo fbci{};
	fbci.sType				= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbci.renderPass			= m_defaultRenderPass;
	fbci.attachmentCount	= 2;
	fbci.width				= m_vulkanContext.windowExtent.width;
	fbci.height				= m_vulkanContext.windowExtent.height;
	fbci.layers				= 1;

	const auto image_count	= m_vulkanContext.swapchainImages.size();
	m_framebuffers.resize(image_count);
		
	for (size_t i = 0; i < m_vulkanContext.swapchainImageViews.size(); ++i) {
		const auto& it = m_vulkanContext.swapchainImageViews[i];
		VkImageView attachments[] = { it,iDepth.view };
		fbci.pAttachments = attachments;
		VK_CHECK(vkCreateFramebuffer(*device, &fbci, nullptr, &m_framebuffers[i]));
	}

	return true;
}
bool VulkanBackend::init_sync_structures()
{
	VkFenceCreateInfo fci{};
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(vkCreateFence(*device, &fci, nullptr, &m_frameData[i].renderFence));
	}

	VkSemaphoreCreateInfo sci{};
	sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(vkCreateSemaphore(*device, &sci, nullptr, &m_frameData[i].renderSemaphore));
		VK_CHECK(vkCreateSemaphore(*device, &sci, nullptr, &m_frameData[i].presentSemaphore));
	}

	fci.flags = 0;
	VK_CHECK(vkCreateFence(*device, &fci, nullptr, &m_uploadContext.uploadFence));

	return true;
}
bool VulkanBackend::init_descriptors()
{
	const std::vector<VkDescriptorPoolSize> sizes_1 =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 10 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = INFLIGHT_FRAMES;
	pool_info.poolSizeCount = static_cast<unsigned>(sizes_1.size());
	pool_info.pPoolSizes = sizes_1.data();

	VK_CHECK(vkCreateDescriptorPool(*device, &pool_info, nullptr, &m_globalShaderDataDescriptors.pool));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorPool(*device, m_globalShaderDataDescriptors.pool, nullptr);
		});


	std::vector<VkDescriptorSetLayoutBinding> bindings;
	bindings.push_back(vkinitializer::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 0));
	bindings.push_back(vkinitializer::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 1));
	bindings.push_back(vkinitializer::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 2));
	bindings.push_back(vkinitializer::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 3));

	VkDescriptorSetLayoutCreateInfo globalinfo = {};
	globalinfo.flags = 0;
	globalinfo.pNext = nullptr;
	globalinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	globalinfo.bindingCount = static_cast<unsigned>(bindings.size());
	globalinfo.pBindings = bindings.data();
	VK_CHECK(vkCreateDescriptorSetLayout(*device, &globalinfo, nullptr, &m_globalShaderDataDescriptors.layout));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorSetLayout(*device, m_globalShaderDataDescriptors.layout, nullptr);
		});

	assert(testBuffer.index != invalidIndex);
	Buffer& testBufferInst = m_buffers.list[testBuffer.index];
	testBufferInst.setupDescriptor();
	m_globalShaderDataDescriptors.sets.resize(INFLIGHT_FRAMES);

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		//allocate one descriptor set for each frame
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		//using the pool we just set
		allocInfo.descriptorPool = m_globalShaderDataDescriptors.pool;
		//only 1 descriptor
		allocInfo.descriptorSetCount = 1;
		//using the global data layout
		allocInfo.pSetLayouts = &m_globalShaderDataDescriptors.layout;

		VK_CHECK( vkAllocateDescriptorSets(*device, &allocInfo, &m_globalShaderDataDescriptors.sets[i]) );

		m_frameData[i].globalUbo.setupDescriptor(sizeof(GlobalShaderDescription));

		std::vector<VkWriteDescriptorSet> write_sets;

		write_sets.push_back( vkinitializer::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_globalShaderDataDescriptors.sets[i], &m_frameData[i].globalUbo.descriptor, 0) );

		VkWriteDescriptorSet write_sampler = {};
		std::array<VkDescriptorImageInfo, 3> iinf = {};
		iinf[0].sampler = m_defaultSamplers.anisotropeLinearRepeat;
		iinf[1].sampler = m_defaultSamplers.anisotropeLinearEdgeClamp;
		iinf[2].sampler = m_defaultSamplers.nearestEdgeClamp;
		write_sampler.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_sampler.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		write_sampler.descriptorCount = 1;
		write_sampler.dstBinding = 1;
		write_sampler.dstSet = m_globalShaderDataDescriptors.sets[i];
		write_sampler.pImageInfo = &iinf[0];
		write_sets.push_back(write_sampler);
		write_sampler.dstBinding = 2;
		write_sampler.pImageInfo = &iinf[1];
		write_sets.push_back(write_sampler);
		write_sampler.dstBinding = 3;
		write_sampler.pImageInfo = &iinf[2];
		write_sets.push_back(write_sampler);

		vkUpdateDescriptorSets(*device, static_cast<unsigned>(write_sets.size()), write_sets.data(), 0, nullptr);
	}

	const std::vector<VkDescriptorPoolSize> sizes_2 =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 450 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 450 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 50 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50 }
	};

	VkDescriptorPoolCreateInfo pool_info2 = {};
	pool_info2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info2.flags = 0;
	pool_info2.maxSets = 1000;
	pool_info2.poolSizeCount = static_cast<unsigned>(sizes_2.size());
	pool_info2.pPoolSizes = sizes_2.data();

	VK_CHECK(vkCreateDescriptorPool(*device, &pool_info2, nullptr, &m_descriptorPool));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorPool(*device, m_descriptorPool, nullptr);
		});

	return true;
}
bool VulkanBackend::init_pipelines()
{
	//--- default pipeline ---

	// define layout
	VkPipelineLayoutCreateInfo plci = pipeline_layout_create_info();
	VkDescriptorSetLayout setLayouts[] = { m_globalShaderDataDescriptors.layout, m_globalTextureArrayDescriptors.layout };
	VkPushConstantRange pcRanges;
	pcRanges.offset = 0;
	pcRanges.size = sizeof(FragmentPushConstants);
	pcRanges.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	plci.setLayoutCount = 2;
	plci.pSetLayouts = setLayouts;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcRanges;
	VK_CHECK(vkCreatePipelineLayout(*device, &plci, nullptr, &m_defaultPipelineLayout));
	m_deletions.push_function([=]() {vkDestroyPipelineLayout(*device, m_defaultPipelineLayout, nullptr); });	

	PipelineBuilder b{};
	b._colorBlendAttachment = color_blend_attachment_state();
	b._depthStencil = depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	b._inputAssembly = input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	b._multisampling = multisampling_state_create_info();
	b._pipelineLayout = m_defaultPipelineLayout;
	b._rasterizer = rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	b._colorBlendAttachment.blendEnable = VK_FALSE;
	b._colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MIN;
	b._colorBlendAttachment.colorBlendOp = VK_BLEND_OP_MAX;
	b._colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	b._colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

	auto vert = jsrlib::Filesystem::root.ReadFile("../../resources/shaders/triangle.vert.spv");
	auto frag = jsrlib::Filesystem::root.ReadFile("../../resources/shaders/triangle.frag.spv");

	SpvReflectShaderModule module = {};
	SpvReflectResult result =
	spvReflectCreateShaderModule(vert.size(), vert.data(), &module);
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	uint32_t count = 0;
	result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	std::vector<SpvReflectDescriptorSet*> sets(count);
	result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
	assert(result == SPV_REFLECT_RESULT_SUCCESS);

	spvReflectDestroyShaderModule(&module);


	VkShaderModule modules[2];
	auto vertexDesc = Vertex::vertex_input_description();

	VkShaderModuleCreateInfo smci{};
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.pCode = (uint32_t*)vert.data();
	smci.codeSize = vert.size();
	VK_CHECK(vkCreateShaderModule(*device, &smci, nullptr, &modules[0]));
	smci.pCode = (uint32_t*)frag.data();
	smci.codeSize = frag.size();
	VK_CHECK(vkCreateShaderModule(*device, &smci, nullptr, &modules[1]));
	b._shaderStages.push_back(pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, modules[0]));
	b._shaderStages.push_back(pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, modules[1]));
	b._vertexInputInfo = vertex_input_state_create_info();
	b._vertexInputInfo.vertexBindingDescriptionCount = static_cast<unsigned>(vertexDesc.bindings.size());
	b._vertexInputInfo.pVertexBindingDescriptions = vertexDesc.bindings.data();
	b._vertexInputInfo.vertexAttributeDescriptionCount = static_cast<unsigned>(vertexDesc.attributes.size());
	b._vertexInputInfo.pVertexAttributeDescriptions = vertexDesc.attributes.data();

	//b._viewport = { 0.0f, 0.0f, float(m_vulkanContext.windowExtent.width), float(m_vulkanContext.windowExtent.height), 0.0f, 1.0f };
	//b._scissor = { 0,0,m_vulkanContext.windowExtent.width,m_vulkanContext.windowExtent.height };
	VkDynamicState dynState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	b._dynamicStates = {};
	b._dynamicStates.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	b._dynamicStates.dynamicStateCount = 2;
	b._dynamicStates.pDynamicStates = dynState;

	m_defaultPipeline = b.build_pipeline(*device, m_defaultRenderPass);

	vkDestroyShaderModule(*device, modules[0], nullptr);
	vkDestroyShaderModule(*device, modules[1], nullptr);

	m_deletions.push_function([=]() {vkDestroyPipeline(*device, m_defaultPipeline, nullptr); });

	return false;
}


void VulkanBackend::immediate_submit(const std::function<void(VkCommandBuffer)>&& function)
{
	
	VkCommandBuffer cmd = m_uploadContext.commandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinitializer::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//execute the function
	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = vkinitializer::submit_info(&cmd);


	//submit command buffer to the queue and execute it.
	// _uploadFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(m_graphicQueue, 1, &submit, m_uploadContext.uploadFence));

	vkWaitForFences(*device, 1, &m_uploadContext.uploadFence, true, 9999999999);
	vkResetFences(*device, 1, &m_uploadContext.uploadFence);

	// reset the command buffers inside the command pool
	vkResetCommandPool(*device, m_uploadContext.commandPool, 0);
	
}

void VulkanBackend::transfer_buffer_immediate(const Buffer& dest, uint32_t size, const void* data)
{
	VkCommandPool pool;
	VkCommandBuffer cmd;
	VK_CHECK(device->createCommandPool(device->queueFamilyIndices.transfer, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &pool));
	VK_CHECK(device->createCommandBuffer(pool, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, false, &cmd, true));

	Buffer stagingBuffer = {};
	VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, size, &stagingBuffer, data));

	VkBufferCopy copy = {};
	copy.size = size;
	vkCmdCopyBuffer(cmd, stagingBuffer.handle, dest.handle, 1, &copy);

	device->flushCommandBuffer(cmd, m_transferQueue, pool, true);

	vkDestroyCommandPool(*device, pool, nullptr);
	stagingBuffer.destroy();
}

void VulkanBackend::add_texture_to_global_array(Image& texture)
{
	assert(static_cast<unsigned>(m_globalTextureUsed) < maxTextureCount);

	VkDescriptorImageInfo imginfo{};
	imginfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imginfo.imageView = texture.view;

	m_globalTextureArrayMutex.lock();
	if (m_freeTextureIndexes.empty() == false)
	{
		texture.globalIndex = m_freeTextureIndexes.back();
		m_freeTextureIndexes.pop_back();
	}
	else
	{
		texture.globalIndex = m_globalTextureUsed.fetch_add(1);
	}
	m_globalTextureArrayMutex.unlock();

	auto imgWrite = vkinitializer::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_globalTextureArrayDescriptors.sets[0], &imginfo, 0, texture.globalIndex);
	vkUpdateDescriptorSets(*device, 1, &imgWrite, 0, nullptr);
}

void VulkanBackend::free_global_texture(uint32_t index)
{
	if (index != invalidIndex) {
		m_globalTextureArrayMutex.lock();
		m_freeTextureIndexes.push_back(index);
		m_globalTextureArrayMutex.unlock();
	}
}

VkResult VulkanBackend::create_buffer(VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VkDeviceSize size, Buffer* buffer, const void* data)
{
	VkBufferCreateInfo info{};
	VmaAllocationCreateInfo vma_allocinfo{};
	info.sType			= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;
	info.size			= size;
	info.usage			= usage;
	vma_allocinfo.usage	= memUsage;


	VkResult result = vmaCreateBuffer(device->allocator, &info, &vma_allocinfo, &buffer->handle, &buffer->memory, nullptr);

	buffer->device = device;
	buffer->setupDescriptor();
	buffer->valid = true;
	buffer->usageFlags = usage;

	if (data && (memUsage & (VMA_MEMORY_USAGE_CPU_ONLY|VMA_MEMORY_USAGE_CPU_TO_GPU))) {
		VK_CHECK(buffer->map());
		buffer->copyTo(data, size);
		buffer->unmap();
	}

	return result;
}

bool VulkanBackend::init_buffers()
{
	const VkDeviceSize SIZE = 16 * 1024;

	m_globalShaderData.g_modelMatrix = glm::mat4(1.0f);
	m_globalShaderData.g_viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.5f));
	m_globalShaderData.g_projectionMatrix = glm::perspective(glm::radians(45.f), (float)m_vulkanContext.windowExtent.width / (float)m_vulkanContext.windowExtent.height, 0.1f, 10.f);

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 1024, &m_frameData[i].globalUbo, nullptr));

		m_frameData[i].globalUbo.map();
		m_frameData[i].globalUbo.copyTo(&m_globalShaderData, sizeof(m_globalShaderData));
		m_frameData[i].globalUbo.unmap();
	}

	m_deletions.push_function([=]()
		{
			for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
				m_frameData[i].globalUbo.destroy();
			}
		});


	std::random_device r;
	std::default_random_engine e(r());
	std::uniform_int_distribution<int> uniform_dist(1,100);
	
	uint32_t test_uniform_buffer[256];
	for (int i = 0; i < 256; ++i) test_uniform_buffer[i] = uniform_dist(e);
	StorageBufferDescription ubo_ci(256 * 4, test_uniform_buffer);
	auto test_buffer = this->createStorageBuffer(ubo_ci);
	testBuffer = test_buffer;

	assert(test_buffer.index != invalidIndex);

	return true;
}

bool VulkanBackend::init_global_texture_array_layout()
{
	VkDescriptorSetLayoutBinding textureArrayBinding{};
	textureArrayBinding.binding = 0;
	textureArrayBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	textureArrayBinding.descriptorCount = maxTextureCount;
	textureArrayBinding.stageFlags = VK_SHADER_STAGE_ALL;
	textureArrayBinding.pImmutableSamplers = nullptr;

	const VkDescriptorBindingFlags bits = 
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo{};
	flagInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flagInfo.pNext = nullptr;
	flagInfo.bindingCount = 1;
	flagInfo.pBindingFlags = &bits;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &flagInfo;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &textureArrayBinding;

	VK_CHECK(vkCreateDescriptorSetLayout(*device, &layoutInfo, nullptr, &m_globalTextureArrayDescriptors.layout));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorSetLayout(*device, m_globalTextureArrayDescriptors.layout, nullptr);
		});

	return true;
}

bool VulkanBackend::init_global_texture_array_descriptor()
{
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

	VK_CHECK( vkCreateDescriptorPool(*device, &poolInfo, nullptr, &m_globalTextureArrayDescriptors.pool) );
	
	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo = {};
	variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableDescriptorCountInfo.pNext = nullptr;
	variableDescriptorCountInfo.descriptorSetCount = 1;
	variableDescriptorCountInfo.pDescriptorCounts = &maxTextureCount;

	VkDescriptorSetAllocateInfo setInfo = {};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setInfo.pNext = &variableDescriptorCountInfo;
	setInfo.descriptorPool = m_globalTextureArrayDescriptors.pool;
	setInfo.descriptorSetCount = 1;
	setInfo.pSetLayouts = &m_globalTextureArrayDescriptors.layout;
	m_globalTextureArrayDescriptors.sets.push_back({});
	VK_CHECK( vkAllocateDescriptorSets(*device, &setInfo, &m_globalTextureArrayDescriptors.sets.back()));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorPool(*device, m_globalTextureArrayDescriptors.pool, nullptr);
		});

	return true;
}

bool VulkanBackend::init_samplers()
{
	vkinitializer::sampler_builder builder;
	builder
		.addressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.filter(VK_FILTER_LINEAR)
		.maxLod(1000.f)
		.anisotropyEnable(VK_TRUE)
		.maxAnisotropy(8.0f)
		.mipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);


	auto sci = builder.build();
	SamplerHandle hSamp;
	hSamp = createSampler(sci);
	m_defaultSamplers.anisotropeLinearRepeat = m_samplers[hSamp.index].vulkanSampler;

	builder.addressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	sci = builder.build();
	hSamp = createSampler(sci);
	m_defaultSamplers.anisotropeLinearEdgeClamp = m_samplers[hSamp.index].vulkanSampler;

	builder.magFilter(VK_FILTER_NEAREST)
		.filter(VK_FILTER_NEAREST)
		.anisotropyEnable(VK_FALSE)
		.mipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);

	sci = builder.build();
	hSamp = createSampler(sci);
	m_defaultSamplers.nearestEdgeClamp = m_samplers[hSamp.index].vulkanSampler;

	return true;
}

void VulkanBackend::cleanup_swapchain()
{

	for (const auto& it : m_framebuffers) {
		vkDestroyFramebuffer(*device, it, nullptr);
	}

	m_vulkanContext.vkbSwapchain.destroy_image_views(m_vulkanContext.swapchainImageViews);
	vkb::destroy_swapchain(m_vulkanContext.vkbSwapchain);

	m_framebuffers.clear();
	m_vulkanContext.swapchainImageViews.clear();

}

void VulkanBackend::recreate_swapchain()
{
	vkDeviceWaitIdle(*device);
	
	VkSurfaceCapabilitiesKHR capabilities{};
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vulkanContext.gpu, m_vulkanContext.surface, &capabilities));
	SDL_Event e;
	while (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0)
	{
		SDL_WaitEvent(&e);
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vulkanContext.gpu, m_vulkanContext.surface, &capabilities));
	}

	freeImage(m_depthImageHandle);
	
	if (init_swapchain())
	{
		init_framebuffers();
	}
}

bool VulkanBackend::init_images() {

	// create white image
	gli::extent2d extent{ 1,1 };
	gli::texture2d white_texture(gli::texture::format_type::FORMAT_RGBA8_UNORM_PACK8, extent, 1);
	glm::vec<4,uint8_t> color(255);
	white_texture.clear(color);
	
	ImageHandle hTex = createImageFromGliTexture("default_white", white_texture);
	Image& res = get_imageref(hTex);

	GlobalImage white_image{};
	white_image.globalArrayIndex = res.globalIndex;
	white_image.name = res.name;
	m_globalImages.insert(std::make_pair(white_image.name, white_image));
	m_globalShaderData.g_whiteImageIndex = white_image.globalArrayIndex;
	return true;
}

}
