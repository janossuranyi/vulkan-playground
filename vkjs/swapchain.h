#ifndef VKJS_SWAPCHAIN_H
#define VKJS_SWAPCHAIN_H

#include "vkjs.h"

namespace jvk {
	struct SwapChain {
		vkb::Swapchain vkb_swapchain{};
		std::vector<VkImage> images;
		std::vector<VkImageView> views;

		VkExtent3D extent() const {
			return VkExtent3D{ vkb_swapchain.extent.width,vkb_swapchain.extent.height,1 };
		}
		VkExtent2D extent2d() const {
			return vkb_swapchain.extent;
		}
		/**
		* Acquires the next image in the swap chain
		*
		* @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
		* @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
		*
		* @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
		*
		* @return VkResult of the image acquisition
		*/
		VkResult acquire_next_image(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);

		/**
		* Queue an image for presentation
		*
		* @param queue Presentation queue for presenting the image
		* @param imageIndex Index of the swapchain image to queue for presentation
		* @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
		*
		* @return VkResult of the queue presentation
		*/
		VkResult present_image(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

		std::vector<Image> get_swapchain_images();
	};
}
#endif // !VKJS_SWAPCHAIN_H
