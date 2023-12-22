#include "pch.h"
#include "vk_device.h"
#include "vk_command_pool.h"
#include "vk_command_buffer.h"
#include "vk_check.h"
#include "vk_initializers.h"
#include <jsrlib/jsr_logger.h>

namespace vks {
	VkQueue VulkanDevice::getGraphicsQueue()
	{
		auto result = vkbDevice.get_queue(vkb::QueueType::graphics);
		return result.value();
	}
	VkQueue VulkanDevice::getComputeQueue()
	{
		auto result = vkbDevice.get_queue(vkb::QueueType::compute);
		return result.has_value() ? result.value() : getGraphicsQueue();
	}

	VkQueue VulkanDevice::getTransferQueue()
	{
		auto result = vkbDevice.get_queue(vkb::QueueType::transfer);
		return result.has_value() ? result.value() : getGraphicsQueue();
	}

	VkResult VulkanDevice::createFence(VkFenceCreateFlags flags, VkFence* out)
	{
		VkFenceCreateInfo CI{};
		CI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		CI.flags = flags;

		return vkCreateFence(vkDevice, &CI, nullptr, out);
	}

	VkResult VulkanDevice::createSemaphore(VkSemaphoreCreateFlags flags, VkSemaphore* out)
	{
		VkSemaphoreCreateInfo CI{};
		CI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		CI.flags = flags;
		return vkCreateSemaphore(vkDevice, &CI, nullptr, out);
	}

	VkResult VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags, VkCommandPool* out)
	{
		const auto CI = createCommandPoolInfo(queueFamilyIndex, flags);
		return vkCreateCommandPool(vkDevice, &CI, nullptr, out);
	}

	VkResult VulkanDevice::createCommandBuffer(VkCommandPool pool, VkCommandBufferUsageFlags usage, bool secondary, VkCommandBuffer* out, bool begin)
	{
		const unsigned count = 1;
		const auto CI = commandBufferAllocateInfo(pool, count, secondary);

		auto res = vkAllocateCommandBuffers(vkDevice, &CI, out);
		if (res == VK_SUCCESS && begin)
		{
			VkCommandBufferBeginInfo cmdBufInfo = vkinitializer::command_buffer_begin_info(usage);
			VK_CHECK(vkBeginCommandBuffer(*out, &cmdBufInfo));
		}

		return res;
	}

	void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
	{
		if (commandBuffer == VK_NULL_HANDLE)
		{
			return;
		}

		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = vkinitializer::submit_info(&commandBuffer);
		// Create fence to ensure that the command buffer has finished executing
		VkFence fence;
		VK_CHECK(createFence(0, &fence));
		VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
		// Wait for the fence to signal that command buffer has finished executing
		VK_CHECK(vkWaitForFences(vkDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
		vkDestroyFence(vkDevice, fence, nullptr);
		if (free)
		{
			vkFreeCommandBuffers(vkDevice, pool, 1, &commandBuffer);
		}
	}

	VulkanDevice::VulkanDevice() :
		vkDevice(),
		vkbDevice(),
		physicalDevice(),
		allocator(),
		properties(),
		features(),
		enabledFeatures(),
		memoryProperties(),
		queueFamilyIndices()
	{
	}
	void VulkanDevice::init(vkb::PhysicalDevice& physicalDevice, VkInstance instance)
	{
		vkb::DeviceBuilder deviceBuilder(physicalDevice);

		uint32_t i = 0;
		for (auto& it : physicalDevice.get_queue_families()) {
			std::string str = "";
			if (it.queueFlags & VK_QUEUE_COMPUTE_BIT) str += "Compute;";
			if (it.queueFlags & VK_QUEUE_GRAPHICS_BIT) str += "Graphics;";
			if (it.queueFlags & VK_QUEUE_TRANSFER_BIT) str += "Transfer;";
			if (it.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) str += "SparseBinding;";
			if (it.queueFlags & VK_QUEUE_PROTECTED_BIT) str += "Protected;";
			if (str == "") str = "Unkonwn;";

			jsrlib::Info("Queue family %d: %s Count: %d", i, str.c_str(), it.queueCount);
			++i;
		}

		for (i = 0; i < physicalDevice.memory_properties.memoryTypeCount; ++i)
		{
			auto& m = physicalDevice.memory_properties.memoryTypes[i];
			std::string str = "";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) str += "device-local;";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) str += "host-visible;";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) str += "host-coherent;";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) str += "host-cached;";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) str += "lazily-allocated;";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) str += "protected;";

			//if (m.propertyFlags & vk_memory_type)
			jsrlib::Info("Memory type %d: %s heap: %d", i, str.c_str(), m.heapIndex);
		}

		for (i = 0; i < physicalDevice.memory_properties.memoryHeapCount; ++i)
		{
			auto& h = physicalDevice.memory_properties.memoryHeaps[i];
			jsrlib::Info("Heap index %d, size: %d (Mb)", i, h.size >> 20);
		}

		auto device_build = deviceBuilder.build();
		if (!device_build.has_value()) {
			throw std::runtime_error("Fatal error: cannot create vulkan device [" + device_build.error().message() + "]");
		}

		this->vkbDevice = device_build.value();
		this->vkDevice = vkbDevice.device;
		this->physicalDevice = physicalDevice.physical_device;

#ifdef VKS_USE_VOLK
		volkLoadDevice(vkDevice);
#endif

		auto g_queue_index = vkbDevice.get_queue_index(vkb::QueueType::graphics);
		if (!g_queue_index.has_value()) {
			throw std::runtime_error("Fatal error: cannot get graphics queue index !");
		}

		queueFamilyIndices.graphics = g_queue_index.value();
		queueFamilyIndices.compute = g_queue_index.value();
		queueFamilyIndices.transfer = g_queue_index.value();

		auto c_queue_index = vkbDevice.get_queue_index(vkb::QueueType::compute);
		if (c_queue_index.has_value()) {
			queueFamilyIndices.compute = c_queue_index.value();
		}
		auto t_queue_index = vkbDevice.get_queue_index(vkb::QueueType::transfer);
		if (t_queue_index.has_value()) {
			queueFamilyIndices.transfer = t_queue_index.value();
		}

		VmaAllocatorCreateInfo allocatorInfo{};

#ifdef VKS_USE_VOLK
		vma_vulkan_func.vkAllocateMemory = vkAllocateMemory;
		vma_vulkan_func.vkBindBufferMemory = vkBindBufferMemory;
		vma_vulkan_func.vkBindImageMemory = vkBindImageMemory;
		vma_vulkan_func.vkCreateBuffer = vkCreateBuffer;
		vma_vulkan_func.vkCreateImage = vkCreateImage;
		vma_vulkan_func.vkDestroyBuffer = vkDestroyBuffer;
		vma_vulkan_func.vkDestroyImage = vkDestroyImage;
		vma_vulkan_func.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
		vma_vulkan_func.vkFreeMemory = vkFreeMemory;
		vma_vulkan_func.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
		vma_vulkan_func.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
		vma_vulkan_func.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
		vma_vulkan_func.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
		vma_vulkan_func.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
		vma_vulkan_func.vkMapMemory = vkMapMemory;
		vma_vulkan_func.vkUnmapMemory = vkUnmapMemory;
		vma_vulkan_func.vkCmdCopyBuffer = vkCmdCopyBuffer;
		allocatorInfo.pVulkanFunctions = &vma_vulkan_func;
#endif
		//initialize the memory allocator
		allocatorInfo.physicalDevice = physicalDevice;
		allocatorInfo.device = vkDevice;
		allocatorInfo.instance = instance;

		if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
			throw std::runtime_error("Fatal error: cannot initialize VMA Allocator !");
		}

		this->properties = physicalDevice.properties;
		this->memoryProperties = physicalDevice.memory_properties;
		this->queueFamilyProperties = physicalDevice.get_queue_families();
	}
	void VulkanDevice::quit()
	{
		vkb::destroy_device(vkbDevice);
	}
	VulkanDevice::~VulkanDevice()
	{
		quit();
	}
}