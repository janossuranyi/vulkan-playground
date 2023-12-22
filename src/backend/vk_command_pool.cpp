#include "vk_command_pool.h"

namespace vks {

	VkCommandPoolCreateInfo createCommandPoolInfo(uint32_t queueFamiliy, VkCommandPoolCreateFlags flags)
	{
		VkCommandPoolCreateInfo commandPoolInfo = {};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.pNext = nullptr;

		//the command pool will be one that can submit graphics commands
		commandPoolInfo.queueFamilyIndex = queueFamiliy;
		//we also want the pool to allow for resetting of individual command buffers
		commandPoolInfo.flags = flags;

		return commandPoolInfo;
	}
}