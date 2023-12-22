#pragma once

#include "pch.h"
#include "renderer/VulkanDevice.h"

namespace jsr {

	class VulkanBuffer {
	public:
		VulkanBuffer();
		VulkanBuffer(VulkanDevice* device, VkBufferUsageFlags usage, uint32_t size, VmaMemoryUsage memoryUsage);
		~VulkanBuffer();

		void setup(VulkanDevice* device, VkBufferUsageFlags usage, uint32_t size, VmaMemoryUsage memoryUsage);
		VulkanBuffer(const VulkanBuffer& other) = delete;
		VulkanBuffer& operator=(const VulkanBuffer& other) = delete;

		VulkanDevice*			device;
		VkBuffer				handle;
		VmaAllocation			memory;
		uint32_t				size;
		VkDescriptorBufferInfo	descriptor;
		VkBufferUsageFlags		usageFlags;
		VmaMemoryUsage			memUsage;
		void*					mapped = nullptr;
		bool					hasWritten = false;
		bool					hasMovedOrCopied = false;
		void					unmap();
		void					setupDescriptor(VkDeviceSize size = 0, VkDeviceSize offset = 0);
		void					copyTo(const void* data, VkDeviceSize size);
		VkResult				map();
		VkResult				flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		VkResult				invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void					fillBufferImmediate(size_t size, const void* data);
	private:
		void					destroy();
	};

}