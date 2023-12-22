#include "renderer/VulkanDevice.h"
#include "renderer/VulkanCheck.h"
#include "renderer/VulkanInitializers.h"
#include "jsrlib/jsr_logger.h"
#include <SDL_vulkan.h>

namespace jsr {
	VulkanDevice::VulkanDevice(Window* window) :
		instance(),
		window(),
		debug_messenger(),
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
		this->window = window;
#ifdef VKS_USE_VOLK
		VK_CHECK(volkInitialize());
#endif

		vkb::InstanceBuilder builder;
		// init vulkan instance with debug features
		builder.set_app_name("JSR_Vulkan_driver")
			.set_app_version(VK_MAKE_VERSION(1, 0, 0))
			.require_api_version(1, 2, 0);

#if _DEBUG
		builder
			.enable_validation_layers()
			.use_default_debug_messenger();
#endif

		auto inst_ret = builder.build();

		if (!inst_ret) {
			throw std::runtime_error("Failed to create Vulkan instance!");
		}

		vkb::Instance vkb_inst = inst_ret.value();
		instance = vkb_inst.instance;
		vkbInstance = vkb_inst;
		debug_messenger = vkb_inst.debug_messenger;

#ifdef VKS_USE_VOLK
		volkLoadInstanceOnly(vkb_inst.instance);
#endif
		VkPhysicalDeviceFeatures features = {};
		features.samplerAnisotropy = VK_TRUE;
		features.fragmentStoresAndAtomics = VK_TRUE;
		features.fillModeNonSolid = VK_TRUE;
		features.depthClamp = VK_TRUE;
		features.geometryShader = VK_TRUE;
		features.textureCompressionBC = VK_TRUE;
		features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
		VkPhysicalDeviceVulkan12Features features12 = {}; //vulkan 1.2 features
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features12.pNext = nullptr;
		features12.hostQueryReset						= VK_TRUE;
		features12.runtimeDescriptorArray				= VK_TRUE;
		features12.descriptorBindingPartiallyBound		= VK_TRUE;
		features12.descriptorBindingVariableDescriptorCount		= VK_TRUE;
		features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		features12.shaderSampledImageArrayNonUniformIndexing	= VK_TRUE;
		features12.shaderFloat16								= VK_TRUE;
		features12.descriptorIndexing							= VK_TRUE;
		VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features = {};
		shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
		shader_draw_parameters_features.pNext = nullptr;
		shader_draw_parameters_features.shaderDrawParameters = VK_TRUE;

		SDL_bool surf_res = SDL_Vulkan_CreateSurface(window->getWindow(), instance, &surface);
		// get the surface of the window we opened with SDL
		if (SDL_TRUE != surf_res) {
			throw std::runtime_error("SDL failed to create Vulkan surface !");
		}

		//use vkbootstrap to select a GPU.
		//We want a GPU that can write to the SDL surface and supports Vulkan 1.2
		vkb::PhysicalDeviceSelector selector(vkb_inst);
		auto selector_result = selector
			.set_minimum_version(1, 2)
			.set_surface(surface)
			.set_required_features(features)
			.set_required_features_12(features12)
			.add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
			.add_required_extension(VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME)
			.add_required_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
			.add_required_extension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)
			.add_required_extension_features(shader_draw_parameters_features)
			.select();

		if (!selector_result) {
			throw std::runtime_error("No suitable physical device found !");
		}

		vkb::PhysicalDevice physicalDevice = selector_result.value();

		jsrlib::Info("Device name: %s", physicalDevice.name.c_str());
		jsrlib::Info("API version: %d.%d.%d",
			VK_API_VERSION_MAJOR(physicalDevice.properties.apiVersion),
			VK_API_VERSION_MINOR(physicalDevice.properties.apiVersion),
			VK_API_VERSION_PATCH(physicalDevice.properties.apiVersion));

		this->init(physicalDevice, instance);
	}
	VkQueue VulkanDevice::getGraphicsQueue()
	{
		return graphicsQueue;
	}
	VkQueue VulkanDevice::getComputeQueue()
	{
		return computeQueue ? computeQueue : graphicsQueue;
	}

	VkQueue VulkanDevice::getTransferQueue()
	{
		return transferQueue ? transferQueue : graphicsQueue;
	}

	void VulkanDevice::waitIdle() const
	{
		auto result = vkDeviceWaitIdle(vkDevice);
		if (result == VK_ERROR_DEVICE_LOST) {
			jsrlib::Error("Vulkan: device lost on vkDeviceWaitIdle");
		}
	}

	VkQueue VulkanDevice::createGraphicsQueue()
	{
		return vkbDevice.get_queue(vkb::QueueType::graphics).value();
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
		const auto CI = vkinit::create_commandpool_info(queueFamilyIndex, flags);
		return vkCreateCommandPool(vkDevice, &CI, nullptr, out);
	}

	VkResult VulkanDevice::createCommandBuffer(VkCommandPool pool, VkCommandBufferUsageFlags usage, bool secondary, VkCommandBuffer* out, bool begin)
	{
		const unsigned count = 1;
		const auto CI = vkinit::command_buffer_allocate_info(pool, count, secondary);

		auto res = vkAllocateCommandBuffers(vkDevice, &CI, out);
		if (res == VK_SUCCESS && begin)
		{
			VkCommandBufferBeginInfo cmdBufInfo = vkinit::command_buffer_begin_info(usage);
			VK_CHECK(vkBeginCommandBuffer(*out, &cmdBufInfo));
		}

		return res;
	}

	void VulkanDevice::executeCommands(std::function<void(VkCommandBuffer)>&& callback)
	{
		vkResetCommandPool(vkDevice, commandPool, 0);
		beginCommandBuffer(commandBuffer);

		callback(commandBuffer);

		flushCommandBuffer(commandBuffer, graphicsQueue, commandPool, false);
	}

	void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
	{
		if (commandBuffer == VK_NULL_HANDLE)
		{
			return;
		}

		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = vkinit::submit_info(&commandBuffer);
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

		auto get_queue_result = vkbDevice.get_queue(vkb::QueueType::graphics);
		if (!get_queue_result) {
			throw std::runtime_error("Graphics queue not found !");
		}
		graphicsQueue = get_queue_result.value();

		get_queue_result = vkbDevice.get_queue(vkb::QueueType::compute);
		if (get_queue_result) {
			computeQueue = get_queue_result.value();
		}
		get_queue_result = vkbDevice.get_queue(vkb::QueueType::transfer);
		if (get_queue_result) {
			transferQueue = get_queue_result.value();
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

		auto poolCI = vkinit::command_pool_create_info(queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VK_CHECK(vkCreateCommandPool(vkDevice, &poolCI, nullptr, &commandPool));
		auto cmdAI = vkinit::command_buffer_allocate_info(commandPool, 1, false);
		VK_CHECK(vkAllocateCommandBuffers(vkDevice, &cmdAI, &commandBuffer));
	}

	VulkanDevice::~VulkanDevice()
	{
		vkDestroyCommandPool(vkDevice, commandPool, nullptr);
		vmaDestroyAllocator(allocator);
		vkb::destroy_device(vkbDevice);
	}

	void* VulkanDevice::getInstanceProcAddr(const char* function_name, void*)
	{
		return vkGetInstanceProcAddr(instance, function_name);
	}

	VkResult VulkanDevice::beginCommandBuffer(VkCommandBuffer cb)
	{
		VkCommandBufferBeginInfo cbbi{};
		cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		auto result = vkBeginCommandBuffer(cb, &cbbi);

		return result;
	}

}