#include "renderer/VulkanSwapchain.h"
#include "jsrlib/jsr_logger.h"
#include <SDL_vulkan.h>

namespace jsr {
	
	VulkanSwapchain::VulkanSwapchain(VulkanDevice* device) :
		m_swapchain(),
		m_swapchainExtent(),
		m_swapchainImageFormat(),
		m_vkbSwapchain()
	{
		m_device = device;
		initSwapchain();
	}

	VulkanSwapchain::~VulkanSwapchain()
	{
		cleanupSwapchain();
	}

	void VulkanSwapchain::initSwapchain()
	{
		VkSurfaceFormatKHR format{};
		format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		//format.format = VK_FORMAT_R8G8B8A8_UNORM;
		format.format = VK_FORMAT_B8G8R8A8_UNORM;

		VkSurfaceKHR surface = m_device->surface;

		uint32_t sfc{0};
		std::vector<VkSurfaceFormatKHR> formats;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->physicalDevice, surface, &sfc, nullptr);	formats.resize(sfc);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_device->physicalDevice, surface, &sfc, formats.data());

		for (auto& it : formats) {
			if (it.format == VK_FORMAT_B8G8R8A8_UNORM || it.format == VK_FORMAT_R8G8B8A8_UNORM) {
				format = it;
				break;
			}
		}

		int x = 0, y = 0;
		SDL_Vulkan_GetDrawableSize(m_device->window->getWindow(), &x, &y);

		m_swapchainExtent.width		= static_cast<uint32_t>(x);
		m_swapchainExtent.height	= static_cast<uint32_t>(y);

		vkb::SwapchainBuilder builder(m_device->physicalDevice, m_device->vkDevice, surface);
		if (m_swapchain) {
			builder.set_old_swapchain(m_swapchain);
		}


		auto build_res = builder
			.set_desired_format(format)
			.set_desired_extent(m_swapchainExtent.width,m_swapchainExtent.height)
			.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
			//.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
			.build();

		if (!build_res) {
			jsrlib::Error("Mailbox present mode initialization failed, trying FIFO mode");
			// try fifo mode
			build_res = builder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR).build();
			if (!build_res) {
				throw std::runtime_error("Swapchain initialization failed !");
			}
		}

		auto swapchain = build_res.value();

		if (m_swapchain) {
			cleanupSwapchain();
		}

		m_swapchain = swapchain.swapchain;
		m_swapchainImageFormat = swapchain.image_format;
		m_swapchainImages = swapchain.get_images().value();
		m_swapchainImageViews = swapchain.get_image_views().value();
		m_vkbSwapchain = swapchain;
	}

	void VulkanSwapchain::recreateSwapchain()
	{
		m_device->waitIdle();
		initSwapchain();
	}

	void VulkanSwapchain::cleanupSwapchain()
	{
		m_vkbSwapchain.destroy_image_views(m_swapchainImageViews);
		vkb::destroy_swapchain(m_vkbSwapchain);
		m_swapchainImageViews.clear();
		m_swapchainImages.clear();
	}

	VkResult VulkanSwapchain::acquaireImage(VkSemaphore presentSemaphore, SwapchainImage& output)
	{
		uint32_t nextImage;
		auto acquaireResult = vkAcquireNextImageKHR(m_device->vkDevice, m_swapchain, 1000000000ULL, presentSemaphore, nullptr, &nextImage);

		if (acquaireResult == VK_SUCCESS) {
			output.image = m_swapchainImages[nextImage];
			output.imageView = m_swapchainImageViews[nextImage];
			output.index = nextImage;
			m_lastAcquiredImageIndex = nextImage;
		}
		return acquaireResult;
	}

	VkResult VulkanSwapchain::presentImage(VkQueue presentQueue, VkSemaphore waitSemaphore, uint32_t imageIndex)
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.swapchainCount		= 1;
		presentInfo.pSwapchains			= &m_swapchain;
		presentInfo.pImageIndices		= &imageIndex;

		if (waitSemaphore) {
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &waitSemaphore;
		}

		return vkQueuePresentKHR(presentQueue, &presentInfo);
	}

	VkFormat VulkanSwapchain::getSwapchainImageFormat() const
	{
		return m_swapchainImageFormat;
	}

	VkExtent2D VulkanSwapchain::getSwapChainExtent() const
	{
		return m_swapchainExtent;
	}

	uint32_t VulkanSwapchain::getLastAcquiredImageIndex() const
	{
		return m_lastAcquiredImageIndex;
	}

	VkImageView VulkanSwapchain::getLastAcquiredImageView() const
	{
		return isImageAcquired() ? m_swapchainImageViews[m_lastAcquiredImageIndex] : VK_NULL_HANDLE;
	}

	VkImage VulkanSwapchain::getImage(int index) const
	{
		assert(index < m_swapchainImages.size());
		return m_swapchainImages[index];
	}

	VkImageView VulkanSwapchain::getImageView(int index) const
	{
		assert(index < m_swapchainImageViews.size());
		return m_swapchainImageViews[index];
	}

	bool VulkanSwapchain::isImageAcquired() const
	{
		return m_lastAcquiredImageIndex != ~0;
	}

	float VulkanSwapchain::getAspect() const
	{
		return (float)m_swapchainExtent.width / (float)m_swapchainExtent.height;
	}

	int VulkanSwapchain::getImageCount() const
	{
		return (int) m_swapchainImages.size();
	}

}