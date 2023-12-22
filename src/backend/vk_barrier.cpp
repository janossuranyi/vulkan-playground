#include "vk_barrier.h"

namespace vks {
	MemoryBarrier::MemoryBarrier()
	{
		m_memoryBarrier = {};
		m_memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	}
	MemoryBarrier& MemoryBarrier::accessMasks(VkAccessFlags srcMask, VkAccessFlags dstMask)
	{
		m_memoryBarrier.srcAccessMask = srcMask;
		m_memoryBarrier.dstAccessMask = dstMask;
		return *this;
	}
	VkMemoryBarrier MemoryBarrier::build() const
	{
		return m_memoryBarrier;
	}
	ImageBarrier::ImageBarrier()
	{
		m_imageMemoryBarrier = {};
		m_imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	}
	ImageBarrier& ImageBarrier::accessMasks(VkAccessFlags srcMask, VkAccessFlags dstMask)
	{
		m_imageMemoryBarrier.srcAccessMask = srcMask;
		m_imageMemoryBarrier.dstAccessMask = dstMask;
		return *this;
	}
	ImageBarrier& ImageBarrier::layout(VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		m_imageMemoryBarrier.oldLayout = oldLayout;
		m_imageMemoryBarrier.newLayout = newLayout;
		return *this;
	}
	ImageBarrier& ImageBarrier::oldLayout(VkImageLayout oldLayout)
	{
		m_imageMemoryBarrier.oldLayout = oldLayout;
		return *this;
	}
	ImageBarrier& ImageBarrier::newLayout(VkImageLayout newLayout)
	{
		m_imageMemoryBarrier.newLayout = newLayout;
		return *this;
	}
	ImageBarrier& ImageBarrier::queueFamily(uint32_t srcQueueFamilyIndex, uint32_t destQueueFamilyIndex)
	{
		m_imageMemoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
		m_imageMemoryBarrier.dstQueueFamilyIndex = destQueueFamilyIndex;
		return *this;
	}
	ImageBarrier& ImageBarrier::srcQueueFamilyIndex(uint32_t srcQueueFamilyIndex)
	{
		m_imageMemoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
		return *this;
	}
	ImageBarrier& ImageBarrier::destQueueFamily(uint32_t destQueueFamilyIndex)
	{
		m_imageMemoryBarrier.dstQueueFamilyIndex = destQueueFamilyIndex;
		return *this;
	}
	ImageBarrier& ImageBarrier::image(VkImage image)
	{
		m_imageMemoryBarrier.image = image;
		return *this;
	}
	ImageBarrier& ImageBarrier::subresourceRange(const VkImageSubresourceRange& subresourceRange)
	{
		m_imageMemoryBarrier.subresourceRange = subresourceRange;
		return *this;
	}
	VkImageMemoryBarrier ImageBarrier::build() const
	{
		return m_imageMemoryBarrier;
	}
	BufferBarrier::BufferBarrier()
	{
		m_bufferMemoryBarrier = {};
		m_bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	}
	BufferBarrier& BufferBarrier::accessMasks(VkAccessFlags srcMask, VkAccessFlags dstMask)
	{
		m_bufferMemoryBarrier.srcAccessMask = srcMask;
		m_bufferMemoryBarrier.dstAccessMask = dstMask;
		return *this;
	}
	BufferBarrier& BufferBarrier::queueFamily(uint32_t srcQueueFamilyIndex, uint32_t destQueueFamilyIndex)
	{
		m_bufferMemoryBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
		m_bufferMemoryBarrier.dstQueueFamilyIndex = destQueueFamilyIndex;
		return *this;
	}
	BufferBarrier& BufferBarrier::buffer(VkBuffer buffer)
	{
		m_bufferMemoryBarrier.buffer = buffer;
		return *this;
	}
	BufferBarrier& BufferBarrier::range(VkDeviceSize offset, VkDeviceSize size)
	{
		m_bufferMemoryBarrier.offset = offset;
		m_bufferMemoryBarrier.size = size;
		return *this;
	}
	VkBufferMemoryBarrier BufferBarrier::build() const
	{
		return m_bufferMemoryBarrier;
	}
	PipelineBarrier::PipelineBarrier(const MemoryBarrier& memory)
	{
		m_memory_barriers.push_back(memory);
	}
	PipelineBarrier::PipelineBarrier(const BufferBarrier& buffer)
	{
		m_buffer_barriers.push_back(buffer);
	}
	PipelineBarrier::PipelineBarrier(const ImageBarrier& image)
	{
		m_image_barriers.push_back(image);
	}
	PipelineBarrier& PipelineBarrier::add(const MemoryBarrier& memory)
	{
		m_memory_barriers.push_back(memory);
		return *this;
	}
	PipelineBarrier& PipelineBarrier::add(const BufferBarrier& buffer)
	{
		m_buffer_barriers.push_back(buffer);
		return *this;
	}
	PipelineBarrier& PipelineBarrier::add(const ImageBarrier& image)
	{
		m_image_barriers.push_back(image);
		return *this;
	}
	void PipelineBarrier::issuePipelineBarrierCommand(VkCommandBuffer cmd, VkPipelineStageFlags srcStages, VkPipelineStageFlags dstStages)
	{
		std::vector<VkMemoryBarrier> mem;
		for (auto& it : m_memory_barriers) { mem.push_back(it.build()); }
		std::vector<VkBufferMemoryBarrier> buf;
		for (auto& it : m_buffer_barriers) { buf.push_back(it.build()); }
		std::vector<VkImageMemoryBarrier> img;
		for (auto& it : m_image_barriers) { img.push_back(it.build()); }

		vkCmdPipelineBarrier(cmd, srcStages, dstStages, 0,
			static_cast<unsigned>(mem.size()), mem.data(),
			static_cast<unsigned>(buf.size()), buf.data(),
			static_cast<unsigned>(img.size()), img.data());
	}
}
