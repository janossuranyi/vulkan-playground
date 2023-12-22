#pragma once
#include "renderer/VulkanDevice.h"
#include "renderer/VulkanCheck.h"
#include "pch.h"

namespace jsr {

	struct SwapchainImage {
		VkImage image;
		VkImageView imageView;
		uint32_t index;
	};

	class VulkanSwapchain {
	public:
		VulkanSwapchain(VulkanDevice* device);
		~VulkanSwapchain();
		void initSwapchain();
		void recreateSwapchain();
		void cleanupSwapchain();
		VkResult acquaireImage(VkSemaphore presentSemaphore, SwapchainImage& output);
		VkResult presentImage(VkQueue presentQueue, VkSemaphore waitSemaphore, uint32_t imageIndex);
		VkFormat getSwapchainImageFormat() const;
		VkExtent2D getSwapChainExtent() const;
		uint32_t getLastAcquiredImageIndex() const;
		VkImageView getLastAcquiredImageView() const;
		VkImage getImage(int index) const;
		VkImageView getImageView(int index) const;
		bool isImageAcquired() const;
		float getAspect() const;
		int getImageCount() const;
	private:
		VulkanDevice* m_device;
		VkExtent2D m_swapchainExtent;
		vkb::Swapchain m_vkbSwapchain;
		VkSwapchainKHR m_swapchain;
		VkFormat m_swapchainImageFormat;
		std::vector<VkImage> m_swapchainImages;
		std::vector<VkImageView> m_swapchainImageViews;
		//std::vector<VulkanImage> m_images;
		uint32_t m_lastAcquiredImageIndex = ~0;
	};
}