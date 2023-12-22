#include "buffer.h"

namespace vkjs {
	
	Buffer::Buffer(Device* device) : device_(device)
	{
	}

	bool Buffer::map()
	{
		assert(device_);
		assert(mem_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		if (mapped) return true;

		void* ptr;
		VkResult err = vmaMapMemory(device_->allocator, mem, &ptr);
		
		if (err) {
			return false;
		}

		mapped = (uint8_t*)ptr;
		
		return true;
	}

	void Buffer::unmap()
	{
		assert(device_);
		if (mapped) {
			vmaUnmapMemory(device_->allocator, mem);
			mapped = nullptr;
		}
	}

	void Buffer::destroy()
	{
		assert(device_);
		if (buffer) {
			if (mapped) { unmap(); }
			vmaDestroyBuffer(device_->allocator, buffer, mem);
			buffer = VK_NULL_HANDLE;
			size = 0ull;
			mem = {};
		}
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
