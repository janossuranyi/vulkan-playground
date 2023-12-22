#include "image.h"
#include "gli_utils.h"

namespace vkjs {
	
	Image::Image(Device* device) : device_(device)
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

		change_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		device_->execute_commands([&](VkCommandBuffer cmd)
			{
				vkCmdCopyBufferToImage(
					cmd,
					buffer->buffer,
					this->image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1u,
					&region
				);
			});
	}

	void Image::upload(upload_callback&& callback, Buffer* buffer)
	{
		assert(image);
		device_->execute_commands([&](VkCommandBuffer cmd)
			{
				record_upload(cmd, callback, buffer);
			});
	}

	void Image::record_upload(VkCommandBuffer cmd, upload_callback callback, Buffer* buffer)
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


		record_change_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdCopyBufferToImage(
			cmd,
			buffer->buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		record_change_layout(cmd, final_layout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		layout = final_layout;
	}

	VkImageMemoryBarrier Image::get_layout_transition_barrier(VkImageLayout newLayout)
	{
		VkImageMemoryBarrier ibar = {};

		ibar.subresourceRange.aspectMask = aspect_flags;
		ibar.subresourceRange.baseArrayLayer = 0;
		ibar.subresourceRange.baseMipLevel = 0;
		ibar.subresourceRange.levelCount = levels;
		ibar.subresourceRange.layerCount = layers;
		ibar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		ibar.image = image;
		ibar.dstAccessMask = VK_ACCESS_NONE;
		ibar.srcAccessMask = VK_ACCESS_NONE;
		ibar.oldLayout = layout;
		ibar.newLayout = newLayout;
		
		return ibar;
	}


	void Image::change_layout(VkImageLayout newLayout)
	{
		assert(image);
		if (layout == newLayout) return;

		device_->execute_commands([&](VkCommandBuffer cmd)
			{
				record_change_layout(cmd, newLayout);
			});

		layout = newLayout;
	}

	void Image::record_change_layout(VkCommandBuffer cmd, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		assert(image);
		
		const VkImageMemoryBarrier ibar = get_layout_transition_barrier(newLayout);
		vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &ibar);
	}

	void Image::destroy()
	{
		if (image && !alias) {
			vkDestroyImageView(*device_, view, nullptr);
			vmaDestroyImage(device_->allocator, image, mem);
			image = VK_NULL_HANDLE;
			mem = {};
		}
	}
}
