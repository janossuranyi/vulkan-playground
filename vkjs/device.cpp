#include "vkjs.h"
#include "vkcheck.h"
#include "jsrlib/jsr_logger.h"

namespace vkjs {

	Device::Device(vkb::PhysicalDevice vkb_physical_device_, VkInstance instance) : vkb_physical_device(vkb_physical_device_)
	{
		vkb::DeviceBuilder deviceBuilder(vkb_physical_device);

		auto device_build_result = deviceBuilder.build();
		if (!device_build_result)
		{
			throw std::runtime_error("FATAL: VKJS Cannot create logical device");
		}

		vkb_device = device_build_result.value();
		logical_device = vkb_device.device;
		

#ifdef VKJS_USE_VOLK
		volkLoadDevice(logical_device);
#endif

		auto g_queue_index = vkb_device.get_queue_index(vkb::QueueType::graphics);
		if (!g_queue_index.has_value()) {
			throw std::runtime_error("Fatal error: cannot get graphics queue index !");
		}

		queue_family_indices.graphics = g_queue_index.value();
		queue_family_indices.compute = g_queue_index.value();
		queue_family_indices.transfer = g_queue_index.value();

		if (vkb_physical_device.has_dedicated_compute_queue()) {
			auto c_queue_index = vkb_device.get_queue_index(vkb::QueueType::compute);
			queue_family_indices.compute = c_queue_index.value();
		}

		if (vkb_physical_device.has_dedicated_transfer_queue()) {
			auto t_queue_index = vkb_device.get_queue_index(vkb::QueueType::transfer);
			queue_family_indices.transfer = t_queue_index.value();
		}

		auto get_queue_result = vkb_device.get_queue(vkb::QueueType::graphics);
		if (!get_queue_result) {
			throw std::runtime_error("Graphics queue not found !");
		}

		graphics_queue = get_graphics_queue();
		for (size_t i(0); i < vkb_physical_device.memory_properties.memoryHeapCount; ++i) {
			jsrlib::Info("Device memory heap[%d] size: %u Mb", i, vkb_physical_device.memory_properties.memoryHeaps[i].size / 1024 / 1024);
		}

		for (size_t i = 0; i < vkb_physical_device.memory_properties.memoryTypeCount; ++i)
		{
			auto& m = vkb_physical_device.memory_properties.memoryTypes[i];
			std::string str = "";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) str += "device-local; ";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) str += "host-visible; ";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) str += "host-coherent; ";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) str += "host-cached; ";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) str += "lazily-allocated; ";
			if (m.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) str += "protected; ";
			if (str == "") str = "unknown [" + std::to_string(m.propertyFlags) + "]";
			//if (m.propertyFlags & vk_memory_type)
			jsrlib::Info("Memory type [%d:%d]: %s", m.heapIndex, i, str.c_str());
		}


		VmaAllocatorCreateInfo allocatorInfo{};

#ifdef VKJS_USE_VOLK
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
		allocatorInfo.physicalDevice = vkb_physical_device;
		allocatorInfo.device = logical_device;
		allocatorInfo.instance = instance;

		if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
			throw std::runtime_error("Fatal error: cannot initialize VMA Allocator !");
		}

		init_command_pools();
		init_command_buffers();
	}

	Device::~Device()
	{
		vkDestroyCommandPool(vkb_device, command_pool, nullptr);
		
		for (const auto& it : buffers) {
			VmaAllocationInfo inf;
			vmaGetAllocationInfo(allocator, it.mem, &inf);
			if (inf.pMappedData) {
				vmaUnmapMemory(allocator, it.mem);
			}
			vmaDestroyBuffer(allocator, it.buffer, it.mem);
		}

		for (const auto& it : images) {
			vkDestroyImageView(vkb_device, it.view, nullptr);
			vmaDestroyImage(allocator, it.image, it.mem);
		}

		vmaDestroyAllocator(allocator);
		vkb::destroy_device(vkb_device);
	}

	VkQueue Device::get_graphics_queue()
	{
		return get_dedicated_queue(vkb::QueueType::graphics);
	}

	VkQueue Device::get_compute_queue()
	{
		return get_dedicated_queue(vkb::QueueType::compute);
	}

	VkQueue Device::get_transfer_queue()
	{
		return get_dedicated_queue(vkb::QueueType::transfer);
	}

	VkQueue Device::get_dedicated_queue(vkb::QueueType t)
	{
		VkQueue res = VK_NULL_HANDLE;
		auto get_queue_ret = vkb_device.get_queue(t);

		if (get_queue_ret) {
			res = get_queue_ret.value();
		}

		return res;
	}

	bool Device::has_dedicated_compute_queue() const
	{
		return vkb_physical_device.has_dedicated_compute_queue();
	}

	bool Device::has_dedicated_transfer_queue() const
	{
		return vkb_physical_device.has_dedicated_transfer_queue();
	}

	VkResult Device::create_command_pool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags, VkCommandPool* result)
	{
		*result = VK_NULL_HANDLE;
		VkCommandPoolCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		ci.flags = flags;
		ci.queueFamilyIndex = queueFamilyIndex;
		VkResult err = vkCreateCommandPool(vkb_device, &ci, nullptr, result);
		
		return err;
	}
	void Device::init_command_pools()
	{
		//assert(concurrency > 0);
		VK_CHECK(create_command_pool(queue_family_indices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &command_pool));
	}

	VkResult Device::create_command_buffer(VkCommandPool pool, VkCommandBufferUsageFlags usage, bool secondary, VkCommandBuffer* out, bool begin)
	{
		VkResult res;
		const unsigned count = 1;
		VkCommandBufferAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.commandPool = pool;
		ai.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		ai.commandBufferCount = 1;
		res = vkAllocateCommandBuffers(vkb_device, &ai, out);
		if (res == VK_SUCCESS && begin)
		{
			VkCommandBufferBeginInfo cmdBufInfo = {};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdBufInfo.flags = usage;
			res = vkBeginCommandBuffer(*out, &cmdBufInfo);
		}

		return res;
	}

	void Device::init_command_buffers()
	{
		VK_CHECK(create_command_buffer(command_pool, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, false, &command_buffer, false));
	}

	VkResult Device::begin_command_buffer(VkCommandBuffer cb)
	{
		VkCommandBufferBeginInfo cbbi{};
		cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		auto result = vkBeginCommandBuffer(cb, &cbbi);

		return result;
	}

	VkResult Device::create_fence(VkFenceCreateFlags flags, VkFence* out)
	{
		VkFenceCreateInfo CI{};
		CI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		CI.flags = flags;

		return vkCreateFence(vkb_device, &CI, nullptr, out);
	}

	VkResult Device::create_semaphore(VkSemaphoreCreateFlags flags, VkSemaphore* out)
	{
		VkSemaphoreCreateInfo CI{};
		CI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		CI.flags = flags;
		return vkCreateSemaphore(vkb_device, &CI, nullptr, out);
	}

	VkResult Device::create_buffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags, VkDeviceSize size, Buffer* result)
	{
		assert(result);

		VkBufferCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		ci.usage = usage;
		ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ci.size = size;
		ci.flags = 0;
		VmaAllocationCreateInfo vmaci = {};
		vmaci.usage = VMA_MEMORY_USAGE_UNKNOWN;
		vmaci.requiredFlags = memFlags;
		vmaci.flags = 0;

		VkResult err = vmaCreateBuffer(allocator, &ci, &vmaci, &result->buffer, &result->mem, nullptr);
		if (err == VK_SUCCESS)
		{
			result->device_ = this;
			result->mem_flags = memFlags;
			result->size = size;
			result->alias = true;
			result->setup_descriptor();
			buffers.push_back({ result->buffer, result->mem });
		}

		return err;
	}

	VkResult Device::create_uniform_buffer(VkDeviceSize size, bool deviceLocal, Buffer* result)
	{
		VkMemoryPropertyFlags mflags;
		VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		if (deviceLocal)
		{
			mflags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		}
		else 
		{
			mflags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		}

		return create_buffer(
			usage,
			mflags,
			size,
			result);
	}

	VkResult Device::create_staging_buffer(VkDeviceSize size, Buffer* result)
	{
		auto err = create_buffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			size,
			result);

		if (!err) { result->map(); }

		return err;
	}

	VkResult Device::create_storage_buffer(VkDeviceSize size, Buffer* result)
	{
		return create_buffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			size,
			result);
	}

	VkResult Device::create_image(VkFormat format, const VkExtent3D& extent, VkImageUsageFlags usage, VkImageType type, VkImageTiling tiling, VkImageViewType viewType, uint32_t levels, uint32_t layers, uint32_t faces, VkSampleCountFlagBits samples, VkImageAspectFlags aspectFlags, Image* result)
	{
		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;

		info.imageType = type;

		info.format = format;
		info.extent = extent;

		info.mipLevels = levels;
		info.arrayLayers = layers;
		info.samples = samples;
		info.tiling = tiling;
		info.usage = usage;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (faces == 6) {
			info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		VmaAllocationCreateInfo ai = {};
		ai.usage = VMA_MEMORY_USAGE_UNKNOWN;
		ai.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkResult err;
		//allocate and create the image
		err = vmaCreateImage(allocator, &info, &ai, &result->image, &result->mem, nullptr);
		
		if (err) {
			return err;
		}

		result->extent = extent;
		result->format = format;
		result->type = type;
		result->written = false;
		result->levels = levels;
		result->faces = faces;
		result->layers = layers;
		result->tiling = tiling;
		result->samples = samples;
		result->usage = usage;
		result->layout = info.initialLayout;
		result->aspect_flags = aspectFlags;
		result->device_ = this;

		//build a image-view for the depth image to use for rendering
		VkImageViewCreateInfo vinfo = {};
		vinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vinfo.pNext = nullptr;

		vinfo.viewType = viewType;
		vinfo.image = result->image;
		vinfo.format = format;
		vinfo.subresourceRange.baseMipLevel = 0;
		vinfo.subresourceRange.levelCount = levels;
		vinfo.subresourceRange.baseArrayLayer = 0;
		vinfo.subresourceRange.layerCount = layers * faces;
		vinfo.subresourceRange.aspectMask = aspectFlags;
		err = vkCreateImageView(vkb_device, &vinfo, nullptr, &result->view);

		if (err) {
			vmaDestroyImage(allocator, result->image, result->mem);
			result->image = VK_NULL_HANDLE;
			result->mem = {};
		}

		images.push_back({ result->image,result->mem,result->view });

		return err;
	}

	VkResult Device::create_texture2d_with_mips(VkFormat format, const VkExtent3D& extent, Image* result)
	{
		VkResult err;
		
		const uint32_t levels = ((uint32_t)std::floor(std::log2(std::max(extent.width, extent.height)))) + 1u;

		err = create_image(
			format,
			extent,
			VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_VIEW_TYPE_2D,
			levels, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT, 
			result
		);

		result->final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		return err;
	}

	VkResult Device::create_texture2d(VkFormat format, const VkExtent3D& extent, Image* result)
	{
		VkResult err;

		const uint32_t levels = 1u;

		err = create_image(
			format,
			extent,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_VIEW_TYPE_2D,
			levels, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			result
		);

		result->final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		return err;
	}

	VkResult Device::create_texture2d_cube(VkFormat format, const VkExtent3D& extent, bool withMips, Image* result)
	{
		VkResult err;

		const uint32_t levels = withMips ? ((uint32_t)std::floor(std::log2(std::max(extent.width, extent.height)))) + 1u : 1u;

		err = create_image(
			format,
			extent,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_VIEW_TYPE_CUBE,
			levels, 1, 6,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			result
		);

		result->final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		return err;
	}

	VkResult Device::create_color_attachment(VkFormat format, const VkExtent3D& extent, Image* result)
	{
		VkResult err;

		const uint32_t levels = 1u;

		err = create_image(
			format,
			extent,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_VIEW_TYPE_2D,
			levels, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			result
		);
		result->final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		return err;
	}

	VkResult Device::create_depth_stencil_attachment(VkFormat format, const VkExtent3D& extent, Image* result)
	{
		VkResult err;

		const uint32_t levels = 1u;

		err = create_image(
			format,
			extent,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_VIEW_TYPE_2D,
			levels, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_ASPECT_STENCIL_BIT|VK_IMAGE_ASPECT_DEPTH_BIT,
			result
		);

		result->final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		return err;
	}

	void Device::destroy_buffer(Buffer* b)
	{
		for (auto it = buffers.begin(); it != buffers.end(); it++) {
			if (it->buffer == b->buffer) {
				b->alias = false;
				b->destroy();
				buffers.erase(it);
				break;
			}
		}
	}

	void Device::destroy_image(Image* i)
	{
		for (auto it = images.begin(); it != images.end(); it++) {
			if (it->image == i->image) {
				i->alias = false;
				i->destroy();
				images.erase(it);
				break;
			}
		}
	}

	void Device::buffer_copy(const Buffer* src, const Buffer* dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
	{
		execute_commands([&](VkCommandBuffer cmd)
			{
				VkBufferCopy copy;
				copy.srcOffset = srcOffset;
				copy.dstOffset = dstOffset;
				copy.size = size;
				vkCmdCopyBuffer(cmd, src->buffer, dst->buffer, 1u, &copy);
			});
	}

	void Device::flush_command_buffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
	{
		if (commandBuffer == VK_NULL_HANDLE)
		{
			return;
		}

		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Create fence to ensure that the command buffer has finished executing
		VkFence fence;
		VK_CHECK(create_fence(0, &fence));

		{
			std::lock_guard<std::mutex> lck(submit_queue_mutex);
			VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
		}

		// Wait for the fence to signal that command buffer has finished executing
		VK_CHECK(vkWaitForFences(vkb_device, 1, &fence, VK_TRUE, UINT64_MAX));
		vkDestroyFence(vkb_device, fence, nullptr);
		if (free)
		{
			vkFreeCommandBuffers(vkb_device, pool, 1, &commandBuffer);
		}
	}

	void Device::execute_commands(std::function<void(VkCommandBuffer)>&& callback)
	{
		//assert(threadId < concurrency);

		const VkCommandPool pool	= command_pool;
		const VkCommandBuffer cmd	= command_buffer;

		vkResetCommandPool(vkb_device, pool, 0);
		begin_command_buffer(cmd);

		callback(cmd);

		flush_command_buffer(cmd, graphics_queue, pool, false);
	}

}
