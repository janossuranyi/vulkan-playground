#pragma once

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
//#define GLM_FORCE_SIMD_SSE2

#include <iostream>
#include <fstream>
#include <cinttypes>
#include <limits>
#include <vector>
#include <array>
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
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
