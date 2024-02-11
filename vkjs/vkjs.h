#ifndef VKJS_VKJS_H_
#define VKJS_VKJS_H_

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define asfloat(x) static_cast<float>(x)
#define asuint(x) static_cast<uint32_t>(x)

#include <iostream>
#include <fstream>
#include <cinttypes>
#include <limits>
#include <array>
#include <vector>
#include <filesystem>
#include <optional>
#include <functional>
#include <deque>
#include <mutex>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>

#ifdef VKJS_USE_VOLK
#include <volk.h>
#else
#include <vulkan/vulkan.h>
#endif
#include <vma/vk_mem_alloc.h>
#include <VkBootstrap.h>

#include "types.h"
#include "buffer.h"
#include "image.h"
#include "device.h"
#include "swapchain.h"
#include "input.h"
#include "keycodes.h"

#endif // !VKJS_VKJS_H_
