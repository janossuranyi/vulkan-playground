#include "renderer/VulkanImage.h"
#include "renderer/VulkanInitializers.h"
#include "renderer/VulkanCheck.h"
#include "renderer/VulkanBuffer.h"
#include "renderer/VulkanBarrier.h"
#include <stb_image.h>

namespace jsr {
	
	VulkanImage::VulkanImage(VulkanDevice* device, const ImageDescription& desc) : 
		device(device),
		format(desc.format),
		layers(desc.layers),
		faces(desc.faces),
		name(desc.name),
		usageFlags(desc.usageFlags)
	{
		currentlyWriting = false;

		create_image(desc);
	}

	VulkanImage::VulkanImage(VulkanDevice* device, const std::filesystem::path filename) : device(device)
	{
		this->name = filename.u8string();
	}
	
	VulkanImage::VulkanImage(VulkanDevice* device, const VulkanSwapchain* swapchain, int index) : device(device),swapchainImage(true)
	{
		this->extent.width = swapchain->getSwapChainExtent().width;
		this->extent.height = swapchain->getSwapChainExtent().height;
		this->extent.depth = 1;
		this->format = swapchain->getSwapchainImageFormat();
		this->image = swapchain->getImage(index);
		this->view = swapchain->getImageView(index);
		this->currentlyWriting = false;
		this->faces = 1;
		this->globalIndex = -1;
		this->isCubemap = false;
		this->type = VK_IMAGE_TYPE_2D;
		this->usageFlags = ImageUsageFlags::Attachment;
		this->layers = 1;
		this->levels = 1;
		this->layout = VK_IMAGE_LAYOUT_UNDEFINED;
		this->sampler = VK_NULL_HANDLE;
		this->memory = VK_NULL_HANDLE;
	}

	VulkanImage::~VulkanImage()
	{
		destroy();
	}

	gli::texture VulkanImage::loadImageHelper(const std::filesystem::path filename, bool autoMipmap)
	{
		gli::texture texture = gli::load(filename.u8string());
		if (!texture.empty()) {
			return texture;
		}

		int width, height, chn;
		stbi_uc* data = stbi_load(filename.u8string().c_str(), &width, &height, &chn, STBI_rgb_alpha);

		if (!data)
		{
			return gli::texture();
		}

		gli::texture2d t2d;

		if (autoMipmap) {
			t2d = gli::texture2d(gli::texture::format_type::FORMAT_RGBA8_UNORM_PACK8, { width, height });
		}
		else {
			t2d = gli::texture2d(gli::texture::format_type::FORMAT_RGBA8_UNORM_PACK8, { width, height }, 1);
		}

		memcpy(t2d.data(0, 0, 0), data, width * height * 4);
		stbi_image_free(data);

		if (autoMipmap) {
			return (gli::generate_mipmaps(t2d, gli::FILTER_LINEAR));
		}
		else {
			return t2d;
		}

	}

	void VulkanImage::create_image(const ImageDescription& desc)
	{
		VkImageUsageFlags usage = {};
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		const bool isDepth = isDepthFormat(desc.format);
		const bool isStencil = isStencilFormat(desc.format);

		if (bool(desc.usageFlags & ImageUsageFlags::Sampled)) {
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		if (bool(desc.usageFlags & ImageUsageFlags::Storage)) {
			usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			layout = VK_IMAGE_LAYOUT_GENERAL;
		}
		if (bool(desc.usageFlags & ImageUsageFlags::Attachment)) {
			usage |= (isDepth || isStencil) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		if (desc.levels >= 1) {
			levels = desc.levels;
		}
		else if (desc.levels == 0) {
			levels = (uint32_t)(std::floor(std::log2(std::max(desc.width, desc.height))));
		}

		extent.width = desc.width;
		extent.height = desc.height;
		extent.depth = desc.depth;
		VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_1D;

		type = VK_IMAGE_TYPE_1D;
		if (layers > 1) {
			view_type = layers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		}
		if (desc.height > 1) {
			type = VK_IMAGE_TYPE_2D;
			view_type = layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		}
		if (desc.depth > 1) {
			type = VK_IMAGE_TYPE_3D;
			view_type = VK_IMAGE_VIEW_TYPE_3D;
		}

		isCubemap = faces == 6;
		
		if (isCubemap) {
			view_type = layers > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
		}

		auto imageCI = vkinit::image_create_info(desc.format, usage, extent);
		imageCI.arrayLayers = faces * layers;
		imageCI.imageType = type;
		imageCI.mipLevels = levels;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (isCubemap) {
			imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		VmaAllocationCreateInfo dimg_allocinfo = {};
		dimg_allocinfo.usage = VMA_MEMORY_USAGE_UNKNOWN;
		dimg_allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		//allocate and create the image
		VK_CHECK(vmaCreateImage(*device, &imageCI, &dimg_allocinfo, &image, &memory, nullptr));

		auto view_create_info = vkinit::imageview_create_info(format, image, levels, VK_IMAGE_ASPECT_COLOR_BIT);
		if (isDepth) {
			view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (isStencil) {
			view_create_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		view_create_info.subresourceRange.layerCount = layers * faces;
		view_create_info.viewType = view_type;

		if (layout != VK_IMAGE_LAYOUT_UNDEFINED) {

			// layout change
			ImageBarrier ib = {};
			ib.accessMasks(VK_ACCESS_NONE, VK_ACCESS_NONE)
				.image(image)
				.oldLayout(VK_IMAGE_LAYOUT_UNDEFINED)
				.newLayout(layout)
				.subresourceRange(view_create_info.subresourceRange);

			PipelineBarrier pb(ib);
			device->executeCommands([&](VkCommandBuffer cmd)
				{
					pb.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
				});

			this->layout = layout;
		}

		VK_CHECK(vkCreateImageView(device->vkDevice, &view_create_info, nullptr, &view));

	}

	void VulkanImage::create_image(const gli::texture& texture)
	{
		usageFlags = ImageUsageFlags::Sampled;

		format = gliutils::convert_format(texture.format());

		extent = gliutils::convert_extent(texture.extent());

		layers = static_cast<uint32_t>(texture.layers());

		faces = static_cast<uint32_t>(texture.faces());

		type = gliutils::convert_type(texture.target());

		levels = static_cast<uint32_t>(texture.levels());

		const uint32_t face_total(layers * faces);

		auto create_image_info = vkinit::image_create_info(format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);
		create_image_info.arrayLayers = face_total;
		create_image_info.mipLevels = levels;
		create_image_info.imageType = type;
		create_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		create_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VmaAllocationCreateInfo dimg_allocinfo = {};
		dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		if (texture.target() == gli::TARGET_CUBE || texture.target() == gli::TARGET_CUBE_ARRAY) {
			create_image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			isCubemap = true;
		}

		//allocate and create the image
		VK_CHECK(vmaCreateImage(device->allocator, &create_image_info, &dimg_allocinfo, &image, &memory, nullptr));

		VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
		VulkanBuffer staging_buffer(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, static_cast<uint32_t>(texture.size()), VMA_MEMORY_USAGE_CPU_ONLY);

		const void* data = texture.data();
		VK_CHECK(staging_buffer.map());
		staging_buffer.copyTo(data, VkDeviceSize(texture.size()));
		staging_buffer.unmap();

		std::vector<VkBufferImageCopy> bufferCopyRegions;
		std::vector<VkImageViewCreateInfo> viewCreateInfos;
		for (uint32_t layer = 0; layer < texture.layers(); ++layer) {
			for (uint32_t face = 0; face < texture.faces(); ++face) {
				for (uint32_t level = 0; level < texture.levels(); ++level) {

					const uint32_t layer_index(layer * faces + face);
					const glm::ivec3 extent(glm::ivec3(texture.extent(level)));

					const size_t offset = size_t(texture.data(layer, face, level)) - size_t(data);
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
		subresourceRange.levelCount = levels;
		subresourceRange.layerCount = face_total;

		device->executeCommands([&](VkCommandBuffer cmd)
			{
				ImageBarrier img_barrier;

				img_barrier.image(image)
					.layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
					.subresourceRange(subresourceRange)
					.accessMasks(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);

				{
					PipelineBarrier to_transfer(img_barrier);
					to_transfer.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
				}

				vkCmdCopyBufferToImage(
					cmd,
					staging_buffer.handle,
					image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					static_cast<uint32_t>(bufferCopyRegions.size()),
					bufferCopyRegions.data()
				);

				img_barrier
					.layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
					.accessMasks(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE);

				PipelineBarrier to_shader(img_barrier);
				to_shader.issuePipelineBarrierCommand(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
			});

		layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		auto view_create_info = vkinit::imageview_create_info(format, image, levels, VK_IMAGE_ASPECT_COLOR_BIT);

		view_create_info.subresourceRange.layerCount = face_total;
		view_create_info.viewType = gliutils::convert_view_type(texture.target());
		VK_CHECK(vkCreateImageView(device->vkDevice, &view_create_info, nullptr, &view));
	}
	void VulkanImage::destroy()
	{
		// if this is a swapchain, do not destroy it
		if (swapchainImage) { return; }

		vkDestroyImageView(device->vkDevice, view, nullptr);
		vmaDestroyImage(device->allocator, image, memory);
	}
}
