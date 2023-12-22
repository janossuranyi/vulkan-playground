#pragma once

#include "renderer/Window.h"
#include "renderer/Vulkan.h"
#include "pch.h"

#include <VkBootstrap.h>

namespace jsr {
	struct VulkanDevice {

		VulkanDevice(Window* window);
		VulkanDevice() = delete;

		VkInstance instance;
		VkDevice vkDevice;
		VkPhysicalDevice physicalDevice;
		VmaAllocator allocator;
		VkSurfaceKHR surface;
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
		VkPhysicalDeviceFeatures enabledFeatures;
		VkPhysicalDeviceMemoryProperties memoryProperties;
		std::vector<VkQueueFamilyProperties> queueFamilyProperties;
		std::vector<std::string> supportedExtensions;
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

		vkb::Device vkbDevice;
		vkb::Instance vkbInstance;
		VkDebugUtilsMessengerEXT debug_messenger;

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

		Window* window;
		VkQueue getGraphicsQueue();
		VkQueue getComputeQueue();
		VkQueue getTransferQueue();

		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue computeQueue = VK_NULL_HANDLE;
		VkQueue transferQueue = VK_NULL_HANDLE;

		void waitIdle() const;
		VkQueue createGraphicsQueue();
		VkResult createFence(VkFenceCreateFlags flags, VkFence* out);
		VkResult createSemaphore(VkSemaphoreCreateFlags flags, VkSemaphore* out);
		VkResult createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags, VkCommandPool* out);
		VkResult createCommandBuffer(VkCommandPool pool, VkCommandBufferUsageFlags usage, bool secondary, VkCommandBuffer* out, bool begin = false);
		void executeCommands(std::function<void(VkCommandBuffer)>&& callback);
		void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free);
		void init(vkb::PhysicalDevice& physicalDevice, VkInstance instance);
		~VulkanDevice();

		void* getInstanceProcAddr(const char* function_name, void*);
		static VkResult beginCommandBuffer(VkCommandBuffer cb);

	};


}
