#include "buffer.h"

namespace vkjs {
	
	Buffer::Buffer(Device* device) : device_(device)
	{
	}

	bool Buffer::map()
	{
		assert(device_);
		
		VkResult r = device_->map_buffer(this);

		return r == VK_SUCCESS;
	}

	void Buffer::unmap()
	{
		device_->unmap_buffer(this);
	}

	void Buffer::copyTo(VkDeviceSize offset, VkDeviceSize size, const void* data)
	{
		assert(mapped);
		assert(device_);
		assert((offset + size) <= this->size);

		memcpy(mapped + offset, data, size);
	}

	void Buffer::fill(VkDeviceSize offset, VkDeviceSize size, uint32_t data)
	{
		assert(device_);
		assert(mapped);
		assert((size & 3) == 0);

		const size_t n = size >> 2;
		uint32_t* ptr = (uint32_t*)(mapped + offset);
		for (size_t i = 0; i < n; ++i) {
			*(ptr++) = data;
		}
	}

	void Buffer::setup_descriptor()
	{
		descriptor.buffer = buffer;
		descriptor.offset = 0;
		descriptor.range = size;
	}

}
