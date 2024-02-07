#pragma once

#include "pch.h"
#include "renderer/Vulkan.h"
#include "renderer/VulkanDevice.h"
#include "renderer/VulkanSwapchain.h"
#include "renderer/ResourceDescriptions.h"
namespace jsr {

	class VulkanImage {
	public:
		VkImage		image;
		VkImageType	type;
		VkFormat	format;
		VmaAllocation memory;
		VulkanDevice* device;
		VkImageView view;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkAccessFlags currentAccess = 0;
		VkSampler	sampler;
		uint32_t	globalIndex = ~0;
		std::string name;
		VkExtent3D	extent;
		uint32_t	levels;
		uint32_t	layers;
		uint32_t	faces;
		bool		isCubemap;
		bool		currentlyWriting;
		bool		autoMipmap = false;
		bool		swapchainImage = false;
		ImageUsageFlags usageFlags;
		VulkanImage(VulkanDevice* device, const ImageDescription& desc);
		VulkanImage(VulkanDevice* device, const std::filesystem::path filename);
		VulkanImage(VulkanDevice* device, const VulkanSwapchain* swapchain, int index = 0);
		VulkanImage(const VulkanImage&) = delete;
		VulkanImage& operator=(const VulkanImage&) = delete;
		VulkanImage(VulkanImage&&) = delete;
		VulkanImage& operator=(VulkanImage&&) = delete;
		~VulkanImage();

		static inline bool isDepthFormat(VkFormat x)
		{
			if (x == VK_FORMAT_D16_UNORM ||
				x == VK_FORMAT_D16_UNORM_S8_UINT ||
				x == VK_FORMAT_D32_SFLOAT ||
				x == VK_FORMAT_D32_SFLOAT_S8_UINT ||
				x == VK_FORMAT_D24_UNORM_S8_UINT)
			{
				return true;
			}
			return false;
		}

		static inline bool isStencilFormat(VkFormat x)
		{
			return x == VK_FORMAT_D24_UNORM_S8_UINT || x == VK_FORMAT_D16_UNORM_S8_UINT || x == VK_FORMAT_D32_SFLOAT_S8_UINT;
		}

	private:
		void create_image(const ImageDescription& desc);

		void destroy();
	};

}