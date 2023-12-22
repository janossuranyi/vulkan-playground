#include "swapchain.h"
#include "image.h"

VkResult vkjs::SwapChain::acquire_next_image(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
{
	// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
	// With that we don't have to handle VK_NOT_READY

	return vkAcquireNextImageKHR(vkb_swapchain.device, vkb_swapchain, UINT64_MAX, presentCompleteSemaphore, VK_NULL_HANDLE, imageIndex);
}

VkResult vkjs::SwapChain::present_image(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vkb_swapchain.swapchain;
	presentInfo.pImageIndices = &imageIndex;
	// Check if a wait semaphore has been specified to wait for before presenting the image
	if (waitSemaphore != VK_NULL_HANDLE)
	{
		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.waitSemaphoreCount = 1;
	}
	return vkQueuePresentKHR(queue, &presentInfo);
}

std::vector<vkjs::Image> vkjs::SwapChain::create_swapchain_images()
{
	auto ret = std::vector<vkjs::Image>(this->images.size());
	for (size_t i(0); i < images.size(); ++i)
	{
		ret[i].device_ = nullptr;
		ret[i].aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
		ret[i].extent = { vkb_swapchain.extent.width, vkb_swapchain.extent.height,1 };
		ret[i].faces = 1;
		ret[i].levels = 1;
		ret[i].layers = 1;
		ret[i].format = vkb_swapchain.image_format;
		ret[i].type = VK_IMAGE_TYPE_2D;
		ret[i].final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		ret[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		ret[i].view = views[i];
		ret[i].image = images[i];
		ret[i].alias = true;
		ret[i].tiling = VK_IMAGE_TILING_OPTIMAL;
		ret[i].written = false;
		ret[i].usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		ret[i].use_in_swapchain = true;
	}
	return ret;
}
