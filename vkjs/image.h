#ifndef VKJS_IMAGE_H
#define VKJS_IMAGE_H

#include "vkjs.h"

namespace vkjs {

	struct Device;
	struct Buffer;

	struct Image {
		Device*					device_ = nullptr;
		VkImage					image = VK_NULL_HANDLE;
		VkImageView				view = VK_NULL_HANDLE;
		VmaAllocation			mem = {};

		VkDescriptorImageInfo	descriptor;
		VkExtent3D				extent = { 0,0,0 };
		VkImageType				type;
		VkFormat				format = VK_FORMAT_R8_UNORM;
		VkImageLayout			layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout			final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageTiling			tiling = VK_IMAGE_TILING_OPTIMAL;
		VkImageAspectFlags		aspect_flags;
		VkImageUsageFlags		usage;
		VkSampleCountFlagBits	samples;
		uint32_t				levels;
		uint32_t				layers;
		uint32_t				faces;
		bool					written = false;
		bool					alias = false;
		bool					use_in_swapchain = false;
		void*					data = {};

		struct UploadInfo {
			VkExtent3D extent;
			VkDeviceSize offset;
		};

		using upload_callback = std::function<void(uint32_t layer, uint32_t face, uint32_t level, UploadInfo*)>;

		explicit Image(Device* device);
		Image() = default;

		void upload(const VkExtent3D& extent, uint32_t layer, uint32_t face, uint32_t level, VkDeviceSize offset, Buffer* buffer);
		void upload(upload_callback&& callback, Buffer* buffer);
		void change_layout(VkImageLayout newLayout);

		void record_change_layout(VkCommandBuffer cmd, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		void record_upload(VkCommandBuffer cmd, upload_callback callback, Buffer* buffer);
		VkImageMemoryBarrier get_layout_transition_barrier(VkImageLayout newLayout);

		void destroy();
	};
}
#endif // !VKJS_IMAGE_H
