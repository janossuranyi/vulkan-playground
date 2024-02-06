#ifndef VKJS_BUFFER_H_
#define VKJS_BUFFER_H_

#include "vkjs.h"

namespace vkjs {
	
	struct Device;
	struct Buffer {
		VkBuffer				buffer = VK_NULL_HANDLE;
		VmaAllocation			mem{};
		VkDeviceSize			size{};
		VkDescriptorBufferInfo	descriptor{};
		VkBufferUsageFlags		usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
		VkMemoryPropertyFlags	mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		Device*					device_{};
		uint8_t*				mapped{};
		bool					alias = false;
		Buffer() = default;
		explicit Buffer(Device* device);
		bool map();
		void unmap();
		
		/* Do not call directly, use Device::destroy_buffer instead */
		void copyTo(VkDeviceSize offset, VkDeviceSize size, const void* data);
		void fill(VkDeviceSize offset, VkDeviceSize size, uint32_t data);
		void setup_descriptor();
	};
}
#endif // !VKJS_BUFFER_H_
