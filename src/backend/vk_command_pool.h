#pragma once

#include "pch.h"
#include "vk.h"

namespace vks {

	VkCommandPoolCreateInfo createCommandPoolInfo(uint32_t queueFamiliy, VkCommandPoolCreateFlags flags);
}
