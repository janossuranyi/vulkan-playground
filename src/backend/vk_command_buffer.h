#pragma once

#include "vk.h"

namespace vks {

	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count, bool secondary = false);
	bool beginCommandBuffer(VkCommandBuffer cb);
}
