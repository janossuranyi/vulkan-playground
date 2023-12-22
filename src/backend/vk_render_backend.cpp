#pragma once
#include "pch.h"
#include "vk_render_backend.h"
#include "vk_command_pool.h"
#include "vk_command_buffer.h"
#include "vk_pipeline.h"
#include "vk_buffer.h"
#include "vk_initializers.h"
#include "gli_utils.h"

#include <jsrlib/jsr_resources.h>
#include <jsrlib/jsr_logger.h>
#include <stb_image.h>

const uint64_t ONE_SEC = 1000000000ULL;

namespace fs = std::filesystem;

namespace vks {

RenderBackend::~RenderBackend()
{
	quit();
}

bool RenderBackend::init(int concurrency)
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	m_vkctx.window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_vkctx.windowExtent.width,
		m_vkctx.windowExtent.height,
		window_flags
	);

	m_triangles[0].xyz = glm::vec3( 1.0f, -1.0f,  0.0f);
	m_triangles[1].xyz = glm::vec3(-1.0f, -1.0f,  0.0f);
	m_triangles[2].xyz = glm::vec3(-1.0f,  1.0f, 0.0f);
	
	m_triangles[3].xyz = glm::vec3(-1.0f,  1.0f, 0.0f);
	m_triangles[4].xyz = glm::vec3( 1.0f,  1.0f, 0.0f);
	m_triangles[5].xyz = glm::vec3( 1.0f, -1.0f, 0.0f);

	m_triangles[0].uv = glm::vec2(1.0f, 0.0f);
	m_triangles[1].uv = glm::vec2(0.0f, 0.0f);
	m_triangles[2].uv = glm::vec2(0.0f, 1.0f);

	m_triangles[3].uv = glm::vec2(0.0f, 1.0f);
	m_triangles[4].uv = glm::vec2(1.0f, 1.0f);
	m_triangles[5].uv = glm::vec2(1.0f, 0.0f);

	m_triangles[0].pack_color(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	m_triangles[1].pack_color(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
	m_triangles[2].pack_color(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
	m_triangles[3].pack_color(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
	m_triangles[4].pack_color(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
	m_triangles[5].pack_color(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

	m_triangles[0].index = 1+1;
	m_triangles[3].index = 1+5;

	for (int i = 0; i < VERT_COUNT; ++i) {
		m_triangles[i].uv.y = 1.0f - m_triangles[i].uv.y;
		m_triangles[i].xyz.y = -m_triangles[i].xyz.y;
	}

	init_vulkan();
	init_swapchain();
	init_commands();
	init_default_renderpass();
	init_framebuffers();
	init_sync_structures();
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

void RenderBackend::renderFrame(uint32_t frameNumber, bool present)
{
	static bool first = true;

	using namespace glm;
	const auto currentFrame = frameNumber % INFLIGHT_FRAMES;
	const auto& frameData = m_frameData[currentFrame];
		
	const uint64_t TimeOut = 32 * 1000000;
	VK_CHECK(vkWaitForFences(*device, 1, &frameData.renderFence, true, ONE_SEC));
	VK_CHECK(vkResetFences(*device, 1, &frameData.renderFence));

	if (m_windowResized) {
		m_windowResized = false;
		recreate_swapchain();
	}

	uint32_t nextImage;
	VK_CHECK(vkAcquireNextImageKHR(*device, m_vkctx.swapchain, ONE_SEC, frameData.presentSemaphore, nullptr, &nextImage));
	VK_CHECK(vkResetCommandBuffer(frameData.commandBuffer, 0));

	auto cmd = frameData.commandBuffer;
	const float w = float(m_vkctx.windowExtent.width);
	const float h = float(m_vkctx.windowExtent.height);

	m_globalShaderData.g_modelMatrix = glm::rotate(mat4(1.0f), float(frameNumber) / 128.f, vec3(0.0f, 0.0f, 1.0f));
	m_globalShaderData.g_viewMatrix = glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, -2.5f + 2.0f * glm::sin(float(frameNumber) / 256.f)));
	m_globalShaderData.g_frameCount = float(frameNumber);
	m_globalShaderData.g_windowExtent = vec4(w, h, 1.0f / w, 1.0f / h);
	m_globalShaderData.g_projectionMatrix = glm::perspective(glm::radians(45.f), (float)m_vkctx.windowExtent.width / (float)m_vkctx.windowExtent.height, 0.1f, 10.f);


	void* ptr = nullptr;
	vmaMapMemory(*device, frameData.globalUbo.memory, &ptr);
	assert(ptr);
	memcpy(ptr, &m_globalShaderData, sizeof(m_globalShaderData));
	vmaUnmapMemory(*device, frameData.globalUbo.memory);

	beginCommandBuffer(cmd);

	{
		VkClearValue clearVal{};
		float flash = glm::abs(glm::sin(frameNumber / 64.f));
		clearVal.color = { {flash, flash, flash, 1.0f} };
		VkRenderPassBeginInfo rpbi{};
		rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpbi.renderPass = m_defaultRenderPass;
		rpbi.renderArea.extent = m_vkctx.windowExtent;
		rpbi.framebuffer = m_framebuffers[nextImage];
		rpbi.clearValueCount = 1;
		rpbi.pClearValues = &clearVal;
		vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
	}
	//--- draw fancy objects ---
	
	VkDescriptorSet dsets[] = { frameData.globalDescriptor,m_globalTextureArrayDescriptorSet };
	VkViewport vp{ 0.0f,0.0f,w,h,0.0f,1.0f };
	VkRect2D scissor{};
	scissor.extent = { m_vkctx.windowExtent.width, m_vkctx.windowExtent.height };

	vkCmdSetViewport(cmd, 0, 1, &vp);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_defaultPipelineLayout, 0, 2, dsets, 0, nullptr);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_defaultPipeline);
	VkDeviceSize offset = 0;

	//--- draw fancy objects ---
	for (const auto& it : m_renderQueue) {
		const MeshResource& mesh = m_meshes[it.handle.index];

		FragmentPushConstants pc;
		TextureResource tres; 
		get_texture(it.textures[0], &tres);
		pc.texture0 = tres.globalIndex;

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
		presentInfo.pSwapchains = &m_vkctx.swapchain;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &frameData.renderSemaphore;
		presentInfo.pImageIndices = &nextImage;

		VK_CHECK(vkQueuePresentKHR(m_graphicQueue, &presentInfo));
	}
}

TextureHandle RenderBackend::createTexture(const std::filesystem::path& filename, const std::string& name)
{
	TextureHandle result;
	if (get_texture(filename.u8string(), &result, nullptr)) return result;

	gli::texture texture = gli::load(filename.u8string());
	if (texture.empty()) {
		return TextureHandle();
	}

	return createTexture(name, texture);
}

TextureHandle RenderBackend::createTexture(const std::string& name, const gli::texture& gliTexture)
{
	auto result = TextureHandle();
	TextureResource newTexture;
	newTexture.name = name;

	if (VK_SUCCESS == createTextureInternal(gliTexture, newTexture)) {
		std::unique_lock(m_textures.mutex);
		result.index = m_textures.list.size();
		m_textures.list.push_back(newTexture);
		m_textures.namehash.emplace(name, result.index);
	}

	return result;
}

std::vector<MeshHandle> RenderBackend::createMeshes(const std::vector<MeshResourceCreateInfo>& descriptions)
{
	std::vector<MeshHandle> result;

	VkCommandPool pool;
	VK_CHECK(device->createCommandPool(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &pool));

	for (const auto& ci : descriptions) {

		assert(ci.vertexCount > 0);
		assert(ci.vertexBuffer.size() > 0);

		MeshResource mesh(ci.vertexCount, ci.indexCount);
		MeshHandle handle;

		if (ci.indexCount > std::numeric_limits<uint16_t>::max()) {
			mesh.indexType = VK_INDEX_TYPE_UINT32;
		}

		VkDeviceSize size = ci.vertexBuffer.size();
		BufferResource vertexBuffer{};
		BufferResource indexBuffer{};
		
		VK_CHECK(create_buffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			size,
			&vertexBuffer,
			nullptr));
		
		
		m_buffers.list.push_back(vertexBuffer);
		mesh.vertexBuffer = vertexBuffer;
		

		BufferResource vertexStageBuffer{};
		BufferResource indexStageBuffer{};
		VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, size, &vertexStageBuffer, ci.vertexBuffer.data()));
		
		VkCommandBuffer cmd;
		VK_CHECK(device->createCommandBuffers(pool, 1, false, &cmd, true));

		VkBufferCopy copyInfo{};
		copyInfo.size = size;
		vkCmdCopyBuffer(cmd, vertexStageBuffer.handle, vertexBuffer.handle, 1, &copyInfo);
#if 0
		VkBufferMemoryBarrier to_graphicsBarrier{};
		to_graphicsBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		to_graphicsBarrier.buffer = vertexBuffer.handle;
		to_graphicsBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_graphicsBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		to_graphicsBarrier.offset = 0;
		to_graphicsBarrier.size = size;
		to_graphicsBarrier.srcQueueFamilyIndex = device->queueFamilyIndices.transfer;
		to_graphicsBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
#endif
		VkBufferMemoryBarrier to_transerBarrier{};
		to_transerBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		to_transerBarrier.buffer = vertexBuffer.handle;
		to_transerBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_transerBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		to_transerBarrier.offset = 0;
		to_transerBarrier.size = size;
		to_transerBarrier.srcQueueFamilyIndex = device->queueFamilyIndices.graphics;
		to_transerBarrier.dstQueueFamilyIndex = device->queueFamilyIndices.graphics;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &to_transerBarrier, 0, nullptr);

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

			to_transerBarrier.buffer = indexBuffer.handle;
			to_transerBarrier.size = size;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &to_transerBarrier, 0, nullptr);

			m_buffers.list.push_back(indexBuffer);
			mesh.indexBuffer = indexBuffer;
		}

		device->flushCommandBuffer(cmd, m_graphicQueue, pool, true);
		vertexStageBuffer.destroy();
		if (indexStageBuffer.handle) {
			indexStageBuffer.destroy();
		}

		handle.index = static_cast<uint32_t>(m_meshes.size());
		m_meshes.push_back(mesh);
		result.push_back(handle);
	}

	vkDestroyCommandPool(*device, pool, nullptr);

	return result;
}

void RenderBackend::addRenderMesh(const MeshRenderInfo& h)
{
	m_renderQueue.push_back(h);
}

void RenderBackend::windowResized(int width, int height)
{
	m_vkctx.windowExtent.width = std::max(1, width);
	m_vkctx.windowExtent.height = std::max(1, height);
	m_windowResized = true;
}

VkSubresourceLayout RenderBackend::get_subresource_layout(VkImage image, const VkImageSubresource& subresource) {

	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(*device, image, &subresource, &layout);

	return layout;
}

void RenderBackend::quit()
{
	std::cout << "Exiting... Cleaning resources" << std::endl;

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(vkWaitForFences(*device, 1, &m_frameData[i].renderFence, true, ONE_SEC));
	}

	vkDeviceWaitIdle(*device);

	m_deletions.flush();

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
		vmaDestroyImage(device->allocator, tex.image.handle, tex.image.memory);
	}

	m_textures.list.clear();

	vkDestroyCommandPool(*device, m_commandPool, nullptr);
	vkDestroyCommandPool(*device, m_uploadContext.commandPool, nullptr);

	cleanup_swapchain();

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		vkDestroyFence(*device, m_frameData[i].renderFence, nullptr);
		vkDestroySemaphore(*device, m_frameData[i].presentSemaphore, nullptr);
		vkDestroySemaphore(*device, m_frameData[i].renderSemaphore, nullptr);
	}
	vkDestroyFence(*device, m_uploadContext.uploadFence, nullptr);

	vmaDestroyAllocator(*device);

	delete device;

	vkDestroySurfaceKHR(m_vkctx.instance, m_vkctx.surface, nullptr);
	vkb::destroy_debug_utils_messenger(m_vkctx.instance, m_vkctx.debug_messenger);
	vkDestroyInstance(m_vkctx.instance, nullptr);


	SDL_DestroyWindow(m_vkctx.window);
	SDL_Quit();
}

bool RenderBackend::get_texture(const std::string& name, TextureHandle* outhandle, TextureResource* out)
{
	std::unique_lock(m_textures.mutex);
	auto it = m_textures.namehash.find(name);
	if (it != std::end(m_textures.namehash)) {
		if (out) *out = m_textures.list[it->second];
		if (outhandle) outhandle->index = it->second;
		return true;
	}

	return false;
}

void RenderBackend::get_texture(TextureHandle handle, TextureResource* out)
{
	assert(handle.index < m_textures.list.size());

	std::unique_lock(m_textures.mutex);
	*out = m_textures.list[handle.index];
}

VkResult RenderBackend::createTextureInternal(const gli::texture& texture, TextureResource& dest)
{
	VkFormat format = gliutils::convert_format(texture.format());

	VkExtent3D extent = gliutils::convert_extent(texture.extent());
	const uint32_t face_total(static_cast<uint32_t>(texture.layers() * texture.faces()));

	auto create_image_info = vkinit::image_create_info(format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);
	create_image_info.arrayLayers = face_total;
	create_image_info.mipLevels = texture.levels();
	create_image_info.imageType = gliutils::convert_type(texture.target());
	create_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	if (texture.target() == gli::TARGET_CUBE || texture.target() == gli::TARGET_CUBE_ARRAY) {
		create_image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	//allocate and create the image
	VK_CHECK(vmaCreateImage(*device, &create_image_info, &dimg_allocinfo, &dest.image.handle, &dest.image.memory, nullptr));

	VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;

	BufferResource staging_buffer;

	VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, texture.size(), &staging_buffer, nullptr));

	const void* data = texture.data();
	VK_CHECK(staging_buffer.map());
	staging_buffer.copyTo(data, VkDeviceSize(texture.size()));
	staging_buffer.unmap();

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	std::vector<VkImageViewCreateInfo> viewCreateInfos;
	for (std::size_t layer = 0; layer < texture.layers(); ++layer) {
		for (std::size_t face = 0; face < texture.faces(); ++face) {

			if (face_total > 1) {
				VkImageViewCreateInfo info{};
				info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				info.format = format;
				info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				info.subresourceRange.baseArrayLayer = layer * texture.faces() + face;
				info.subresourceRange.layerCount = 1;
				info.subresourceRange.baseMipLevel = 0;
				info.subresourceRange.levelCount = texture.levels();
				info.image = dest.image.handle;
				info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				dest.layerViews.push_back({});
				VK_CHECK(vkCreateImageView(*device, &info, nullptr, &dest.layerViews.back()));
			}

			for (std::size_t level = 0; level < texture.levels(); ++level) {
				for (uint32_t z = 0; z < extent.depth; ++z) {

					const std::size_t layer_index(layer * texture.faces() + face);
					const glm::ivec3 extent(glm::ivec3(texture.extent(level)));

					size_t offset = size_t(texture.data(layer, face, level)) - size_t(data);
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
	}

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = texture.levels();
	subresourceRange.layerCount = face_total;

	//VkCommandPool pool;
	VkCommandBuffer cmd;
	//VK_CHECK(device->createCommandPool(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &pool));
	VK_CHECK(device->createCommandBuffers(m_uploadContext.commandPool, 1, false, &cmd, true /* begin command buffer */));
	{
		VkImageMemoryBarrier imageBarrier_toTransfer{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageBarrier_toTransfer.image = dest.image.handle;
		imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.subresourceRange = subresourceRange;
		imageBarrier_toTransfer.srcAccessMask = VK_ACCESS_NONE;
		imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imageBarrier_toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier_toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

		vkCmdCopyBufferToImage(
			cmd,
			staging_buffer.handle,
			dest.image.handle,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

		device->flushCommandBuffer(cmd, m_graphicQueue, m_uploadContext.commandPool, true /* destroy cmdBuffer */);
	}
	//vkDestroyCommandPool(*device, pool, nullptr);

	staging_buffer.destroy();

	auto imageType = gliutils::convert_type(texture.target());
	auto view_create_info = vkinit::imageview_create_info(format, dest.image.handle, texture.levels(), VK_IMAGE_ASPECT_COLOR_BIT);

	view_create_info.subresourceRange.layerCount = face_total;
	view_create_info.viewType = gliutils::convert_view_type(texture.target());
	VK_CHECK(vkCreateImageView(*device, &view_create_info, nullptr, &dest.view));

	dest.width = extent.width;
	dest.height = extent.height;
	dest.depth = extent.depth;
	dest.levels = texture.levels();
	dest.layers = texture.layers();
	dest.isCubemap = create_image_info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	
	add_texture_to_global_array(dest);


	return VK_SUCCESS;
}

void RenderBackend::upload_image(
	const ImageResource& image,
	const VkExtent3D& extent,
	uint32_t mipLevel,
	const void* data,
	VkDeviceSize size)
{
	BufferResource stagingBuffer;

	VkBufferCreateInfo staging{};
	staging.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	staging.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	staging.size = size;
	VmaAllocationCreateInfo stageMem{};
	stageMem.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	VK_CHECK(vmaCreateBuffer(*device, &staging, &stageMem, &stagingBuffer.handle, &stagingBuffer.memory, nullptr));
	void* ptr = nullptr;
	vmaMapMemory(*device, stagingBuffer.memory, &ptr);
	assert(ptr);
	memcpy(ptr, data, size);
	vmaUnmapMemory(*device, stagingBuffer.memory);

	immediate_submit([&](VkCommandBuffer cmd)
		{

			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;

			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = mipLevel;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = extent;

			//copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, stagingBuffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		});


	vmaDestroyBuffer(*device, stagingBuffer.handle, stagingBuffer.memory);

}

bool RenderBackend::init_vulkan()
{
	auto system_info_ret = vkb::SystemInfo::get_system_info();
	if (!system_info_ret) {
		printf("%s\n", system_info_ret.error().message().c_str());
		return false;
	}

	auto system_info = system_info_ret.value();
	printf("Available extensions: \n");
	for (auto& ext : system_info.available_extensions)
	{
		printf("%s ", ext.extensionName);
	} 
	printf("\n");

	vkb::InstanceBuilder builder;
	// init vulkan instance with debug features
	builder.set_app_name("JSR_Vulkan_driver")
		.set_app_version(VK_MAKE_VERSION(1, 0, 0))
		.require_api_version(1, 2, 0);

#if _DEBUG
	if (system_info.validation_layers_available) {
		builder.enable_validation_layers()
		.use_default_debug_messenger();
	}
#endif
	auto inst_ret = builder.build();

	if (!inst_ret) {
		std::cerr << "Failed to create Vulkan instance!" << std::endl;
		return false;
	}

	vkb::Instance vkb_inst = inst_ret.value();
	m_vkctx.instance = vkb_inst.instance;
	m_vkctx.debug_messenger = vkb_inst.debug_messenger;

	// get the surface of the window we opened with SDL
	if (!SDL_Vulkan_CreateSurface(m_vkctx.window, m_vkctx.instance, &m_vkctx.surface)) {
		std::cerr << "SDL failed to create Vulkan surface !" << std::endl;
		return false;
	}

	VkPhysicalDeviceFeatures features = {};
	features.samplerAnisotropy = VK_TRUE;
	features.fragmentStoresAndAtomics = VK_TRUE;
	features.fillModeNonSolid = VK_TRUE;
	features.depthClamp = VK_TRUE;
	features.geometryShader = VK_TRUE;
	features.textureCompressionBC = VK_TRUE;
	features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.2
	vkb::PhysicalDeviceSelector selector(vkb_inst);
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 2)
		.set_surface(m_vkctx.surface)
		.set_required_features(features)
		.select()
		.value();

	if (!physicalDevice) {
		throw std::runtime_error("No suitable physical device found !");
	}

	jsrlib::Info("Device name: %s", physicalDevice.name.c_str());

	device = new VulkanDevice(physicalDevice, m_vkctx.instance);

	m_vkctx.gpu = device->physicalDevice;

	m_graphicQueue = device->getGraphicsQueue();
	m_computeQueue = device->getComputeQueue();
	m_transferQueue = device->getTransferQueue();

	return true;
}
bool RenderBackend::init_swapchain()
{
	VkSurfaceFormatKHR format{};
	format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	//format.format = VK_FORMAT_R8G8B8A8_UNORM;
	format.format = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::SwapchainBuilder builder(m_vkctx.gpu, *device, m_vkctx.surface);
	auto swapchain = builder
		//.use_default_format_selection()
		.set_desired_format(format)
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(m_vkctx.windowExtent.width, m_vkctx.windowExtent.height)
		.build()
		.value();

	if (!swapchain) {
		std::cerr << "Swapchain initialization failed !" << std::endl;
		return false;
	}

	m_vkctx.swapchain = swapchain.swapchain;
	m_vkctx.swapchainImageFormat = swapchain.image_format;
	m_vkctx.swapchainImages = swapchain.get_images().value();
	m_vkctx.swapchainImageViews = swapchain.get_image_views().value();

	return true;
}

bool RenderBackend::init_commands()
{
	const auto cpci1 = createCommandPoolInfo(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	const auto cpci2 = createCommandPoolInfo(device->queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(*device, &cpci1, nullptr, &m_commandPool));
	VK_CHECK(vkCreateCommandPool(*device, &cpci2, nullptr, &m_uploadContext.commandPool));


	const auto acbi = commandBufferAllocateInfo(m_commandPool, 1);

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(vkAllocateCommandBuffers(*device, &acbi, &m_frameData[i].commandBuffer));
	}

	const auto acbi2 = commandBufferAllocateInfo(m_uploadContext.commandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(*device, &acbi2, &m_uploadContext.commandBuffer));

	return true;
}
bool RenderBackend::init_default_renderpass()
{

	VkAttachmentDescription color_attachment {};
	color_attachment.format			= m_vkctx.swapchainImageFormat;
	color_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout	= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_ref{};
	color_ref.attachment			= 0;
	color_ref.layout				= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS; 
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &color_ref;

	VkRenderPassCreateInfo rpci {};
	rpci.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount			= 1;
	rpci.pAttachments				= &color_attachment;
	rpci.subpassCount				= 1;
	rpci.pSubpasses					= &subpass;

	VK_CHECK(vkCreateRenderPass(*device, &rpci, nullptr, &m_defaultRenderPass));

	m_deletions.push_function([=] { vkDestroyRenderPass(*device, m_defaultRenderPass, nullptr); });

	return true;
}
bool RenderBackend::init_framebuffers()
{
	VkFramebufferCreateInfo fbci{};
	fbci.sType				= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbci.renderPass			= m_defaultRenderPass;
	fbci.attachmentCount	= 1;
	fbci.width				= m_vkctx.windowExtent.width;
	fbci.height				= m_vkctx.windowExtent.height;
	fbci.layers				= 1;

	const auto image_count = m_vkctx.swapchainImages.size();
	m_framebuffers.resize(image_count);
		
	size_t i = 0;
	for (const auto& it : m_vkctx.swapchainImageViews) {
		fbci.pAttachments = &it;
		VK_CHECK(vkCreateFramebuffer(*device, &fbci, nullptr, &m_framebuffers[i++]));
	}

	return true;
}
bool RenderBackend::init_sync_structures()
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
bool RenderBackend::init_descriptors()
{
	const std::vector<VkDescriptorPoolSize> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = 100;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes = sizes.data();

	VK_CHECK(vkCreateDescriptorPool(*device, &pool_info, nullptr, &m_defaultDescriptorPool));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorPool(*device, m_defaultDescriptorPool, nullptr);
		});

	const auto globalBufferBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	const auto samp0 = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1);

	VkDescriptorSetLayoutBinding bindings[] = {globalBufferBinding,samp0};

	VkDescriptorSetLayoutCreateInfo globalinfo = {};
	globalinfo.flags = 0;
	globalinfo.pNext = nullptr;
	globalinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	globalinfo.bindingCount = 2;
	globalinfo.pBindings = bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(*device, &globalinfo, nullptr, &m_defaultSetLayout));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorSetLayout(*device, m_defaultSetLayout, nullptr);
		});

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		//allocate one descriptor set for each frame
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		//using the pool we just set
		allocInfo.descriptorPool = m_defaultDescriptorPool;
		//only 1 descriptor
		allocInfo.descriptorSetCount = 1;
		//using the global data layout
		allocInfo.pSetLayouts = &m_defaultSetLayout;

		VK_CHECK( vkAllocateDescriptorSets(*device, &allocInfo, &m_frameData[i].globalDescriptor) );

		m_frameData[i].globalUbo.setupDescriptor(sizeof(GlobalShaderDescription));

		VkWriteDescriptorSet write = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frameData[i].globalDescriptor, &m_frameData[i].globalUbo.descriptor, 0);
		vkUpdateDescriptorSets(*device, 1, &write, 0, nullptr);

		VkDescriptorImageInfo iinf{};
		iinf.sampler = m_samplers.anisotropeLinearRepeat;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		write.descriptorCount = 1;
		write.dstBinding = 1;
		write.dstSet = m_frameData[i].globalDescriptor;
		write.pImageInfo = &iinf;
		vkUpdateDescriptorSets(*device, 1, &write, 0, nullptr);
	}
	return false;
}
bool RenderBackend::init_pipelines()
{
	//--- default pipeline ---

	// define layout
	VkPipelineLayoutCreateInfo plci = pipeline_layout_create_info();
	VkDescriptorSetLayout setLayouts[] = { m_defaultSetLayout, m_globalTextureArrayLayout };
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
	b._depthStencil = depth_stencil_create_info(false, false, VK_COMPARE_OP_ALWAYS);
	b._inputAssembly = input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	b._multisampling = multisampling_state_create_info();
	b._pipelineLayout = m_defaultPipelineLayout;
	b._rasterizer = rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	b._colorBlendAttachment.blendEnable = VK_TRUE;
	b._colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MAX;
	b._colorBlendAttachment.colorBlendOp = VK_BLEND_OP_MIN;
	b._colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	b._colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

	auto vert = jsrlib::Filesystem::root.ReadFile("../../resources/shaders/triangle.vert.spv");
	auto frag = jsrlib::Filesystem::root.ReadFile("../../resources/shaders/triangle.frag.spv");
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
	b._vertexInputInfo.vertexBindingDescriptionCount = vertexDesc.bindings.size();
	b._vertexInputInfo.pVertexBindingDescriptions = vertexDesc.bindings.data();
	b._vertexInputInfo.vertexAttributeDescriptionCount = vertexDesc.attributes.size();
	b._vertexInputInfo.pVertexAttributeDescriptions = vertexDesc.attributes.data();

	//b._viewport = { 0.0f, 0.0f, float(m_vkctx.windowExtent.width), float(m_vkctx.windowExtent.height), 0.0f, 1.0f };
	//b._scissor = { 0,0,m_vkctx.windowExtent.width,m_vkctx.windowExtent.height };
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


void RenderBackend::immediate_submit(const std::function<void(VkCommandBuffer)>&& function)
{
	
	VkCommandBuffer cmd = m_uploadContext.commandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//execute the function
	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = vkinit::submit_info(&cmd);


	//submit command buffer to the queue and execute it.
	// _uploadFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(m_graphicQueue, 1, &submit, m_uploadContext.uploadFence));

	vkWaitForFences(*device, 1, &m_uploadContext.uploadFence, true, 9999999999);
	vkResetFences(*device, 1, &m_uploadContext.uploadFence);

	// reset the command buffers inside the command pool
	vkResetCommandPool(*device, m_uploadContext.commandPool, 0);
	
}

void RenderBackend::add_texture_to_global_array(TextureResource& texture)
{
	assert(m_globalTextureUsed < maxTextureCount);

	// 3d textures not implemented
	if (texture.depth != 1) return;

	VkDescriptorImageInfo imginfo{};
	imginfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imginfo.imageView = texture.view;

	std::vector<VkWriteDescriptorSet> writes;

	texture.globalIndex = static_cast<uint32_t>( m_globalTextureUsed++ );
	const auto face_total = texture.faces() * texture.layers;

	std::vector<VkDescriptorImageInfo> imginfos(face_total);

	if (face_total == 1) {
		auto imgWrite = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_globalTextureArrayDescriptorSet, &imginfo, 0, texture.globalIndex);
		writes.push_back(imgWrite);
	} else {
		// if texture is an array or cube map then create one slot for every face/layer

		assert(texture.layerViews.size() == face_total);
		for (int layer = 0; layer < texture.layers; ++layer) {
			for (int face = 0; face < texture.faces(); ++face) {

				uint32_t index = static_cast<uint32_t>( m_globalTextureUsed - 1 );
				const size_t face_index = layer * texture.faces() + face;
				
				imginfos[face_index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imginfos[face_index].imageView = texture.layerViews[face_index];

				auto info = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_globalTextureArrayDescriptorSet, &imginfos[face_index], 0, index);
				writes.push_back(info);
				++m_globalTextureUsed;
			}
		}
	}

	vkUpdateDescriptorSets(*device, writes.size(), writes.data(), 0, nullptr);

}

VkResult RenderBackend::create_buffer(VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VkDeviceSize size, BufferResource* buffer, const void* data)
{
	VkBufferCreateInfo info = {};
	VmaAllocationCreateInfo vma_allocinfo = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.size = size;
	info.usage = usage;
	vma_allocinfo.usage = memUsage;


	VkResult result = vmaCreateBuffer(device->allocator, &info, &vma_allocinfo, &buffer->handle, &buffer->memory, nullptr);

	buffer->device = device;
	buffer->setupDescriptor();
	buffer->valid = true;
	buffer->usageFlags = usage;

	if (data && (memUsage & VMA_MEMORY_USAGE_GPU_ONLY)==0) {
		VK_CHECK(buffer->map());
		buffer->copyTo(data, size);
		buffer->unmap();
	}

	return result;
}

bool RenderBackend::init_buffers()
{
	const VkDeviceSize SIZE = 16 * 1024;

	BufferResource stagingBuffer;

	VK_CHECK(create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, SIZE, &stagingBuffer, nullptr));


	const VkDeviceSize vertcache_size = 1024 * 1024;
	
	VK_CHECK(create_buffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vertcache_size,
		&m_vertexBuffer,
		nullptr));

	stagingBuffer.map();
	stagingBuffer.copyTo(m_triangles, VERT_COUNT * sizeof(Vertex));
	stagingBuffer.unmap();

	immediate_submit([=](VkCommandBuffer cmd)
		{
			VkBufferCopy copy = { 0,0,VERT_COUNT * sizeof(Vertex) };
			vkCmdCopyBuffer(cmd, stagingBuffer.handle, m_vertexBuffer.handle, 1, &copy);
		});


	stagingBuffer.destroy();

	m_globalShaderData.g_modelMatrix = glm::mat4(1.0f);
	m_globalShaderData.g_viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.5f));
	m_globalShaderData.g_projectionMatrix = glm::perspective(glm::radians(45.f), (float)m_vkctx.windowExtent.width / (float)m_vkctx.windowExtent.height, 0.1f, 10.f);

	for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
		VK_CHECK(create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 1024, &m_frameData[i].globalUbo, nullptr));

		m_frameData[i].globalUbo.map();
		m_frameData[i].globalUbo.copyTo(&m_globalShaderData, sizeof(m_globalShaderData));
		m_frameData[i].globalUbo.unmap();
	}

	m_deletions.push_function([=]()
		{
			m_vertexBuffer.destroy();

			for (int i = 0; i < INFLIGHT_FRAMES; ++i) {
				m_frameData[i].globalUbo.destroy();
			}
		});

	return true;
}

bool RenderBackend::init_global_texture_array_layout()
{
	VkDescriptorSetLayoutBinding textureArrayBinding{};
	textureArrayBinding.binding = 0;
	textureArrayBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	textureArrayBinding.descriptorCount = maxTextureCount;
	textureArrayBinding.stageFlags = VK_SHADER_STAGE_ALL;
	textureArrayBinding.pImmutableSamplers = nullptr;

	const VkDescriptorBindingFlags flags =
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo{};
	flagInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flagInfo.pNext = nullptr;
	flagInfo.bindingCount = 1;
	flagInfo.pBindingFlags = &flags;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &flagInfo;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &textureArrayBinding;

	VK_CHECK(vkCreateDescriptorSetLayout(*device, &layoutInfo, nullptr, &m_globalTextureArrayLayout));

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorSetLayout(*device, m_globalTextureArrayLayout, nullptr);
		});

	return true;
}

bool RenderBackend::init_global_texture_array_descriptor()
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

	VK_CHECK( vkCreateDescriptorPool(*device, &poolInfo, nullptr, &m_globalTextureArrayPool) );
	
	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo{};
	variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableDescriptorCountInfo.pNext = nullptr;
	variableDescriptorCountInfo.descriptorSetCount = 1;
	variableDescriptorCountInfo.pDescriptorCounts = &maxTextureCount;

	VkDescriptorSetAllocateInfo setInfo{};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setInfo.pNext = &variableDescriptorCountInfo;
	setInfo.descriptorPool = m_globalTextureArrayPool;
	setInfo.descriptorSetCount = 1;
	setInfo.pSetLayouts = &m_globalTextureArrayLayout;

	VK_CHECK( vkAllocateDescriptorSets(*device, &setInfo, &m_globalTextureArrayDescriptorSet) );

	m_deletions.push_function([=]()
		{
			vkDestroyDescriptorPool(*device, m_globalTextureArrayPool, nullptr);
		});

	return true;
}

bool RenderBackend::init_samplers()
{
	vkinit::sampler_builder builder;
	builder
		.addressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.minFilter(VK_FILTER_LINEAR)
		.magFilter(VK_FILTER_LINEAR)
		.maxLod(1000.f)
		.anisotropyEnable(VK_TRUE)
		.maxAnisotropy(8.0f)
		.mipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);

	auto sci = builder.build();
	VK_CHECK(vkCreateSampler(*device, &sci, nullptr, &m_samplers.anisotropeLinearRepeat));

	m_deletions.push_function([=]()
		{
			vkDestroySampler(*device, m_samplers.anisotropeLinearRepeat, nullptr);
		});

	return true;
}

void RenderBackend::cleanup_swapchain()
{
	vkDestroySwapchainKHR(*device, m_vkctx.swapchain, nullptr);

	for (const auto& it : m_framebuffers) {
		vkDestroyFramebuffer(*device, it, nullptr);
	}

	for (auto it : m_vkctx.swapchainImageViews) {
		vkDestroyImageView(*device, it, nullptr);
	}

	m_framebuffers.clear();
	m_vkctx.swapchainImageViews.clear();
}

void RenderBackend::recreate_swapchain()
{
	vkDeviceWaitIdle(*device);

	int x, y;

	cleanup_swapchain();

	SDL_Vulkan_GetDrawableSize(m_vkctx.window, &x, &y);
	m_vkctx.windowExtent.width = static_cast<uint32_t>(x);
	m_vkctx.windowExtent.height = static_cast<uint32_t>(y);

	init_swapchain();
	init_framebuffers();
}

bool RenderBackend::init_images() {

	return true;
}

}
