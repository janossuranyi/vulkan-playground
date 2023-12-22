#ifndef VK_RENDER_GLIUTILS_H
#define VK_RENDER_GLIUTILS_H

#include <gli/gli.hpp>
#include <glm/glm.hpp>
#include "renderer/Vulkan.h"

namespace gliutils {
	VkImageType convert_type(gli::target target);
	VkImageViewType convert_view_type(gli::target target);
	VkExtent3D convert_extent(const gli::extent3d& extent);
	VkFormat convert_format(gli::format format);
	glm::ivec3 compressed_extent(const gli::texture& texture, uint32_t level);
}

#endif // !VK_RENDER_GLIUTILS_H
