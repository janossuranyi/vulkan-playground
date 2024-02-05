#ifndef VKJS_VKJS_H_
#define VKJS_VKJS_H_

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
//#define GLM_FORCE_SIMD_SSE2

#define asfloat(x) static_cast<float>(x)
#define asuint(x) static_cast<uint32_t>(x)

#include <iostream>
#include <fstream>
#include <cinttypes>
#include <limits>
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
#include <glm/glm.hpp>
#include <gli/gli.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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
