#ifndef VKJS_DEVICE_H_
#define VKJS_DEVICE_H_

#include "vkjs.h"

namespace vkjs {

	struct Buffer;
	struct Image;

	struct Device {

		bool debugMarkerPresent = false;
		VkDevice logicalDevice = VK_NULL_HANDLE;
		VmaAllocator allocator = {};
		vkb::Device vkbDevice = {};
		vkb::PhysicalDevice vkbPhysicalDevice = {};
		VkQueue graphics_queue = VK_NULL_HANDLE;
		VmaVulkanFunctions vma_vulkan_func{};
		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queue_family_indices;
		
		struct BufferInfo {
			VkBuffer buffer;
			VmaAllocation mem;
		};
		struct ImageInfo {
			VkImage image;
			VmaAllocation mem;
			VkImageView view;
		};
		struct SamplerInfo {
			VkSampler sampler;
		};

		constexpr operator VkDevice() const
		{
			return logicalDevice;
		};

		explicit Device(vkb::PhysicalDevice physical_device_, VkInstance instance);
		~Device();
		VkQueue get_graphics_queue();
		VkQueue get_compute_queue();
		VkQueue get_transfer_queue();
		VkQueue get_dedicated_queue(vkb::QueueType t);
		bool has_dedicated_compute_queue() const;
		bool has_dedicated_transfer_queue() const;

		/* debug marker methodes */
		void set_object_name(uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name);
		void set_image_name(const Image* image, const std::string& name);
		void set_buffer_name(const Buffer* buffer, const std::string& name);
		void set_descriptor_set_name(VkDescriptorSet ds, const std::string& name);
		void set_debug_marker(VkCommandBuffer cmd, const glm::vec4& color, const std::string& name);
		void begin_debug_marker_region(VkCommandBuffer cmd, const glm::vec4& color, const std::string& name);
		void end_debug_marker_region(VkCommandBuffer cmd);

		VkResult map_buffer(Buffer* buffer);
		VkResult unmap_buffer(Buffer* buffer);
		VkResult create_command_pool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags, VkCommandPool* result);
		VkResult create_command_buffer(VkCommandPool pool, VkCommandBufferUsageFlags usage, bool secondary, VkCommandBuffer* out, bool begin);
		VkResult create_fence(VkFenceCreateFlags flags, VkFence* out);
		VkResult create_semaphore(VkSemaphoreCreateFlags flags, VkSemaphore* out);
		VkResult create_buffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags, VkDeviceSize size, Buffer* result);
		VkResult create_uniform_buffer(VkDeviceSize size, bool deviceLocal, Buffer* result);
		VkResult create_staging_buffer(VkDeviceSize size, Buffer* result);
		VkResult create_storage_buffer(VkDeviceSize size, Buffer* result);
		VkResult create_image(VkFormat format, const VkExtent3D& extent, VkImageUsageFlags usage, VkImageType type, VkImageTiling tiling, VkImageViewType viewType, uint32_t levels, uint32_t layers, uint32_t faces, VkSampleCountFlagBits samples, VkImageAspectFlags aspectFlags, Image* result, bool transient = false);
		VkResult create_texture2d_with_mips(VkFormat format, const VkExtent3D& extent, Image* result);
		VkResult create_texture2d(VkFormat format, const VkExtent3D& extent, Image* result);
		VkResult create_texture2d_cube(VkFormat format, const VkExtent3D& extent, bool withMips, Image* result);
		VkResult create_color_attachment(VkFormat format, const VkExtent3D& extent, VkSampleCountFlagBits sampleCount, Image* result);
		VkResult create_depth_stencil_attachment(VkFormat format, const VkExtent3D& extent, VkSampleCountFlagBits sampleCount, Image* result);
		VkResult create_sampler(const VkSamplerCreateInfo& samplerCreateInfo, VkSampler *out);

		void destroy_buffer(Buffer* b);
		void destroy_image(Image* i);
		void buffer_copy(const Buffer* src, const Buffer* dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);

		void init_command_pools();
		void init_command_buffers();
		VkResult begin_command_buffer(VkCommandBuffer cb);
		void flush_command_buffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free);
		void execute_commands(std::function<void(VkCommandBuffer)>&& callback);

		VkCommandPool command_pool;
		VkCommandBuffer command_buffer;

		std::vector<BufferInfo> buffers;
		std::vector<ImageInfo> images;
		std::vector<SamplerInfo> samplers;
		std::mutex submit_queue_mutex;
	};
}
#endif // !VKJS_DEVICE_H_
