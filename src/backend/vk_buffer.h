#pragma once

#include "pch.h"
#include <glm/glm.hpp>
#include "vk_device.h"

namespace vks {
	struct Buffer {
		VulkanDevice*			device;
		VkBuffer				handle;
		VmaAllocation			memory;
		uint32_t				size;
		VkDescriptorBufferInfo	descriptor;
		VkBufferUsageFlags		usageFlags;
		void*					mapped = nullptr;
		bool					valid = false;
		bool					hasWritten = false;

		void					unmap();
		void					setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void					copyTo(const void* data, VkDeviceSize size);
		VkResult				map();
		VkResult				flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		VkResult				invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void					destroy();
	};
}