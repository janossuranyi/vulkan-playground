#ifndef BACKEND_VK_H_
#define BACKEND_VK_H_

//#define VK_ENABLE_BETA_EXTENSIONS

#ifdef VKS_USE_VOLK
#include <volk.h>
#else
#include <vulkan/vulkan.h>
#endif

#include <vma/vk_mem_alloc.h>

#endif // !BACKEND_VK_H_
