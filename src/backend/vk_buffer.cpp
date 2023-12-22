#include "vk_buffer.h"

namespace vks {
    VkResult Buffer::map()
    {
        assert(handle);
        assert(memory);
        assert(mapped == nullptr);
        return vmaMapMemory(device->allocator, memory, &mapped);
    }

    void Buffer::unmap()
    {
        vmaUnmapMemory(device->allocator, memory);
        mapped = nullptr;
    }

    void Buffer::setupDescriptor(VkDeviceSize size, VkDeviceSize offset)
    {
        descriptor.buffer = handle;
        descriptor.offset = offset;
        descriptor.range = size ? size : this->size;
    }

    void Buffer::copyTo(const void* data, VkDeviceSize size)
    {
        assert(mapped);
        memcpy(mapped, data, size);
    }

    VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset)
    {
        return vmaFlushAllocation(device->allocator, memory, offset, size);
    }

    VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
    {
        return vmaInvalidateAllocation(device->allocator, memory, offset, size);
    }

    void Buffer::destroy()
    {
        if (valid) {
            valid = false;
            vmaDestroyBuffer(device->allocator, handle, memory);
        }
    }

}