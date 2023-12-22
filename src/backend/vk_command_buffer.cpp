#include "vk_command_buffer.h"

namespace vks {

	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count, bool secondary)
	{
		VkCommandBufferAllocateInfo cmdAllocInfo = {};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.pNext = nullptr;

		//commands will be made from our _commandPool
		cmdAllocInfo.commandPool = pool;
		//we will allocate 1 command buffer
		cmdAllocInfo.commandBufferCount = count;
		// command level is Primary
		cmdAllocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		return cmdAllocInfo;
	}

	bool beginCommandBuffer(VkCommandBuffer cb)
	{
		VkCommandBufferBeginInfo cbbi{};
		cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		auto result = vkBeginCommandBuffer(cb, &cbbi);

		return result == VK_SUCCESS;
	}
}