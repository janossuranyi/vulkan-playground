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
		VkMemoryPropertyFlags	mem_flags;
		Device*					device_{};
		uint8_t*				mapped{};
		bool					alias = false;

		Buffer() = default;
		explicit Buffer(Device* device);

		bool map();
		void unmap();
		
		/* Do not call directly, use Device::destroy_buffer instead */
		void destroy();
		void copyTo(VkDeviceSize offset, VkDeviceSize size, const void* data);
		void fill(VkDeviceSize offset, VkDeviceSize size, uint32_t data);
		void setup_descriptor();
	};
}
#endif // !VKJS_BUFFER_H_
