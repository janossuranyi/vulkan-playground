#ifndef VKJS_IMAGE_H
#define VKJS_IMAGE_H

#include "vkjs.h"

namespace jvk {

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

		using UploadCallbackFn = std::function<void(uint32_t layer, uint32_t face, uint32_t level, UploadInfo*)>;

		explicit Image(Device* device);
		Image() = default;
		~Image() noexcept;

		void upload(const VkExtent3D& extent, uint32_t layer, uint32_t face, uint32_t level, VkDeviceSize offset, Buffer* buffer);
		void upload(UploadCallbackFn&& callback, Buffer* buffer);
		void layout_change(VkImageLayout newLayout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

		void record_layout_change(
			VkCommandBuffer cmd, 
			VkImageLayout newLayout, 
			VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VkAccessFlags srcAccess = 0,
			VkAccessFlags dstAccess = 0,
			bool updateLayout = false);
		void record_upload(VkCommandBuffer cmd, UploadCallbackFn callback, Buffer* buffer);
		VkImageMemoryBarrier get_layout_transition_barrier(VkImageLayout newLayout, VkAccessFlags srcAccess = 0, VkAccessFlags dstAccess = 0);
		void generate_mipmaps(VkFilter filter = VK_FILTER_LINEAR);

		void setup_descriptor();
	};
}
#endif // !VKJS_IMAGE_H
