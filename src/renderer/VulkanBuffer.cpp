#include "renderer/Vulkan.h"
#include "renderer/VulkanBuffer.h"
#include "renderer/VulkanCheck.h"
#include "jsrlib/jsr_logger.h"

namespace jsr {
    VkResult VulkanBuffer::map()
    {
        assert(handle);
        assert(memory);
        assert(mapped == nullptr);
        return vmaMapMemory(device->allocator, memory, &mapped);
    }

    VulkanBuffer::VulkanBuffer() :
        device(),
        memUsage(),
        usageFlags(),
        memory(),
        handle(),
        descriptor(),
        size()
    {
    }

    VulkanBuffer::VulkanBuffer(VulkanDevice* device, VkBufferUsageFlags usage, uint32_t size, VmaMemoryUsage memoryUsage) : VulkanBuffer()
    {
        setup(device, usage, size, memoryUsage);
    }

    VulkanBuffer::~VulkanBuffer()
    {
        destroy();
    }

    void VulkanBuffer::setup(VulkanDevice* device, VkBufferUsageFlags usage, uint32_t size, VmaMemoryUsage memoryUsage)
    {
        this->device = device;
        this->usageFlags = usage;
        this->size = size;
        this->memUsage = memoryUsage;

        VkBufferCreateInfo info{};
        VmaAllocationCreateInfo vma_allocinfo{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.size = size;
        info.usage = usage;
        vma_allocinfo.usage = memUsage;

        VK_CHECK(vmaCreateBuffer(device->allocator, &info, &vma_allocinfo, &handle, &memory, nullptr));
        setupDescriptor();
    }

    void VulkanBuffer::unmap()
    {
        if (mapped) {
            vmaUnmapMemory(device->allocator, memory);
            mapped = nullptr;
        }
    }

    void VulkanBuffer::setupDescriptor(VkDeviceSize size, VkDeviceSize offset)
    {
        descriptor.buffer = handle;
        descriptor.offset = offset;
        descriptor.range = size ? size : this->size;
    }

    void VulkanBuffer::copyTo(const void* data, VkDeviceSize size)
    {
        assert(mapped);
        memcpy(mapped, data, size);
    }

    VkResult VulkanBuffer::flush(VkDeviceSize size, VkDeviceSize offset)
    {
        assert(memory);
        return vmaFlushAllocation(device->allocator, memory, offset, size);
    }

    VkResult VulkanBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
    {
        assert(memory);
        return vmaInvalidateAllocation(device->allocator, memory, offset, size);
    }

    void VulkanBuffer::destroy()
    {
        if (handle && memory) {
            vmaDestroyBuffer(device->allocator, handle, memory);
        }
    }

    void VulkanBuffer::fillBufferImmediate(size_t size, const void* data)
    {
        assert(handle);
        assert(size);
        assert(data);

        if (memUsage == VMA_MEMORY_USAGE_GPU_ONLY)
        {
            size = std::max(size, (size_t)this->size);
            VulkanBuffer staging(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, VMA_MEMORY_USAGE_CPU_ONLY);

            //staging.map();
            //staging.copyTo(data, size);
            //staging.unmap();
            staging.fillBufferImmediate(size, data);

            device->executeCommands([&](VkCommandBuffer cmd)
                {
                    VkBufferCopy copyInfo{};
                    copyInfo.size = size;
                    vkCmdCopyBuffer(cmd, staging.handle, handle, 1, &copyInfo);
                });
        }
        else
        {
            map();
            copyTo(data, size);
            unmap();
        }
    }

}