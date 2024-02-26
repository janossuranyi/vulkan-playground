#include "image.h"
#include "VulkanInitializers.hpp"
#include "vkcheck.h"

namespace jvk {
	
	Image::Image(Device* device) : device_(device)
	{
	}
	Image::~Image() noexcept
	{
	}
	void Image::upload(const VkExtent3D& extent, uint32_t layer, uint32_t face, uint32_t level, VkDeviceSize offset, Buffer* buffer)
	{
		assert(image);
		const uint32_t layer_index(layer * faces + face);

		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = aspect_flags;
		region.imageSubresource.mipLevel = level;
		region.imageSubresource.baseArrayLayer = layer_index;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = extent;
		region.bufferOffset = offset;

		device_->execute_commands([&](VkCommandBuffer cmd)
			{
				record_layout_change(cmd, 
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT);
				vkCmdCopyBufferToImage(
					cmd,
					buffer->buffer,
					this->image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1u,
					&region
				);
				//record_change_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			});
		layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	void Image::upload(UploadCallbackFn&& callback, Buffer* buffer)
	{
		assert(image);
		device_->execute_commands([&](VkCommandBuffer cmd)
			{
				record_upload(cmd, callback, buffer);
			});
	}

	void Image::record_upload(VkCommandBuffer cmd, UploadCallbackFn callback, Buffer* buffer)
	{
		assert(image);
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		UploadInfo ui;
		const uint32_t face_total(layers * faces);

		for (uint32_t layer = 0; layer < layers; ++layer) {
			for (uint32_t face = 0; face < faces; ++face) {
				for (uint32_t level = 0; level < levels; ++level) {

					const uint32_t layer_index(layer * faces + face);
					callback(layer, face, level, &ui);

					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = aspect_flags;
					bufferCopyRegion.imageSubresource.mipLevel = level;
					bufferCopyRegion.imageSubresource.baseArrayLayer = layer_index;
					bufferCopyRegion.imageSubresource.layerCount = 1;
					bufferCopyRegion.imageExtent = ui.extent;
					bufferCopyRegion.bufferOffset = ui.offset;
					bufferCopyRegions.push_back(bufferCopyRegion);
				}
			}
		}

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = aspect_flags;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = levels;
		subresourceRange.layerCount = face_total;


		record_layout_change(cmd, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			VK_PIPELINE_STAGE_HOST_BIT, 
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,VK_ACCESS_TRANSFER_WRITE_BIT);
		
		layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		vkCmdCopyBufferToImage(
			cmd,
			buffer->buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		//record_change_layout(cmd, final_layout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		//layout = final_layout;
	}

	VkImageMemoryBarrier Image::get_layout_transition_barrier(VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess)
	{
		VkImageMemoryBarrier ibar = {};

		ibar.subresourceRange.aspectMask = aspect_flags;
		ibar.subresourceRange.baseArrayLayer = 0;
		ibar.subresourceRange.baseMipLevel = 0;
		ibar.subresourceRange.levelCount = levels;
		ibar.subresourceRange.layerCount = layers;
		ibar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		ibar.image = image;
		ibar.dstAccessMask = dstAccess;
		ibar.srcAccessMask = srcAccess;
		ibar.oldLayout = layout;
		ibar.newLayout = newLayout;
		
		return ibar;
	}

	void Image::generate_mipmaps(VkFilter filter)
	{
		assert(image && (usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)));
		VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;


		device_->execute_commands([&](VkCommandBuffer cmd)
			{
				int32_t mipWidth = extent.width;
				int32_t mipHeight = extent.height;

				record_layout_change(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

				for (uint32_t i = 1; i < levels; i++) 
				{
					barrier.subresourceRange.baseMipLevel = i - 1;
					barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

					vkCmdPipelineBarrier(cmd,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &barrier);

					VkImageBlit blit{};
					blit.srcOffsets[0] = { 0, 0, 0 };
					blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
					blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.srcSubresource.mipLevel = i - 1;
					blit.srcSubresource.baseArrayLayer = 0;
					blit.srcSubresource.layerCount = 1;
					blit.dstOffsets[0] = { 0, 0, 0 };
					blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
					blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.dstSubresource.mipLevel = i;
					blit.dstSubresource.baseArrayLayer = 0;
					blit.dstSubresource.layerCount = 1;

					vkCmdBlitImage(cmd,
						image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1, &blit,
						filter);

					barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					vkCmdPipelineBarrier(cmd,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &barrier);


					if (mipWidth > 1) mipWidth /= 2;
					if (mipHeight > 1) mipHeight /= 2;
				}

				barrier.subresourceRange.baseMipLevel = levels - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(cmd,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

				layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			});

	}

	void Image::setup_descriptor()
	{
		descriptor.imageLayout = layout;
		descriptor.imageView = view;
		descriptor.sampler = VK_NULL_HANDLE;
	}


	void Image::layout_change(VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		assert(image);
		if (layout == newLayout) return;

		device_->execute_commands([&](VkCommandBuffer cmd)
			{
				record_layout_change(cmd, newLayout, srcStageMask, dstStageMask);
			});

		layout = newLayout;
	}

	void Image::record_layout_change(VkCommandBuffer cmd, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccess, VkAccessFlags dstAccess, bool update)
	{
		assert(image);
		
		const VkImageMemoryBarrier ibar = get_layout_transition_barrier(newLayout,srcAccess,dstAccess);
		vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &ibar);
		if (update) {
			layout = newLayout;
		}
	}

}
