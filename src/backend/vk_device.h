#ifndef JSR_VK_DEVICE_H
#define JSR_VK_DEVICE_H

#include "pch.h"
#include "vk.h"
#include <VkBootstrap.h>

namespace vks {

	struct VulkanDevice {

		VkDevice vkDevice;
		VkPhysicalDevice physicalDevice;
		VmaAllocator allocator;
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
		VkPhysicalDeviceFeatures enabledFeatures;
		VkPhysicalDeviceMemoryProperties memoryProperties;
		std::vector<VkQueueFamilyProperties> queueFamilyProperties;
		std::vector<std::string> supportedExtensions;
		VkCommandPool commandPool = VK_NULL_HANDLE;

		vkb::Device vkbDevice;
		struct {
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queueFamilyIndices;

		inline operator VkDevice() {
			return vkDevice;
		}
		inline operator VmaAllocator() {
			return allocator;
		}

		VmaVulkanFunctions vma_vulkan_func{};

		VkQueue getGraphicsQueue();
		VkQueue getComputeQueue();
		VkQueue getTransferQueue();

		VkResult createFence(VkFenceCreateFlags flags, VkFence* out);
		VkResult createSemaphore(VkSemaphoreCreateFlags flags, VkSemaphore* out);
		VkResult createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags, VkCommandPool* out);
		VkResult createCommandBuffer(VkCommandPool pool, VkCommandBufferUsageFlags usage, bool secondary, VkCommandBuffer* out, bool begin = false);
		void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free);
		explicit VulkanDevice();
		void init(vkb::PhysicalDevice& physicalDevice, VkInstance instance);
		void quit();
		virtual ~VulkanDevice();
	};
}

#endif