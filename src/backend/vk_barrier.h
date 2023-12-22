#ifndef VKS_BARRIER_H_
#define VKS_BARRIER_H_

#include "pch.h"
#include "vk.h"

namespace vks {
	class MemoryBarrier {
	public:
		MemoryBarrier();
		MemoryBarrier& accessMasks(VkAccessFlags srcMask, VkAccessFlags dstMask);
		VkMemoryBarrier build() const;
	private:
		VkMemoryBarrier m_memoryBarrier;
	};

	class ImageBarrier {
	public:
		ImageBarrier();
		ImageBarrier& accessMasks(VkAccessFlags srcMask, VkAccessFlags dstMask);
		ImageBarrier& layout(VkImageLayout oldLayout, VkImageLayout newLayout);
		ImageBarrier& oldLayout(VkImageLayout oldLayout);
		ImageBarrier& newLayout(VkImageLayout newLayout);
		ImageBarrier& queueFamily(uint32_t srcQueueFamilyIndex, uint32_t destQueueFamilyIndex);
		ImageBarrier& srcQueueFamilyIndex(uint32_t srcQueueFamilyIndex);
		ImageBarrier& destQueueFamily(uint32_t destQueueFamilyIndex);
		ImageBarrier& image(VkImage image);
		ImageBarrier& subresourceRange(const VkImageSubresourceRange& subresourceRange);
		VkImageMemoryBarrier build() const;
	private:
		VkImageMemoryBarrier m_imageMemoryBarrier;
	};

	class BufferBarrier {
	public:
		BufferBarrier();
		BufferBarrier& accessMasks(VkAccessFlags srcMask, VkAccessFlags dstMask);
		BufferBarrier& queueFamily(uint32_t srcQueueFamilyIndex, uint32_t destQueueFamilyIndex);
		BufferBarrier& buffer(VkBuffer buffer);
		BufferBarrier& range(VkDeviceSize offset, VkDeviceSize size);
		VkBufferMemoryBarrier build() const;
	private:
		VkBufferMemoryBarrier m_bufferMemoryBarrier;
	};

	class PipelineBarrier {
	public:
		PipelineBarrier() = default;
		PipelineBarrier(const MemoryBarrier& memory);
		PipelineBarrier(const BufferBarrier& buffer);
		PipelineBarrier(const ImageBarrier& image);
		PipelineBarrier& add(const MemoryBarrier& memory);
		PipelineBarrier& add(const BufferBarrier& buffer);
		PipelineBarrier& add(const ImageBarrier& image);
		void issuePipelineBarrierCommand(VkCommandBuffer cmd, VkPipelineStageFlags srcStages, VkPipelineStageFlags dstStages);
	private:
		std::vector<MemoryBarrier> m_memory_barriers;
		std::vector<BufferBarrier> m_buffer_barriers;
		std::vector<ImageBarrier> m_image_barriers;
	};
}
#endif // !VKS_BARRIER_H_
