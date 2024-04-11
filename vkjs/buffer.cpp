#include "buffer.h"

namespace jvk {
	
	Buffer::Buffer(Device* device) : device_(device)
	{
	}

	bool Buffer::map()
	{
		assert(device_);
		
		VkResult r = device_->map_buffer(this);

		return r == VK_SUCCESS;
	}

	void Buffer::unmap()
	{
		device_->unmap_buffer(this);
	}

	void Buffer::copyTo(VkDeviceSize offset, VkDeviceSize size, const void* data)
	{
		assert(mapped);
		assert(device_);
		assert((offset + size) <= this->size);

		memcpy(mapped + offset, data, size);
	}

	void Buffer::fill(VkDeviceSize offset, VkDeviceSize size, uint32_t data)
	{
		assert(device_);
		assert(mapped);
		assert((size & 3) == 0);

		const size_t n = size >> 2;
		uint32_t* ptr = (uint32_t*)(mapped + offset);
		for (size_t i = 0; i < n; ++i)
		{
			*(ptr++) = data;
		}
	}

	void Buffer::setup_descriptor()
	{
		descriptor.buffer = buffer;
		descriptor.offset = 0;
		descriptor.range = size;
	}

	BufferObject::~BufferObject()
	{
		assert(m_buffer.buffer != VK_NULL_HANDLE);

		m_buffer.device_->destroy_buffer(&m_buffer);
		m_buffer = {};
	}

	UniformBuffer::UniformBuffer(Device* pDevice, VkDeviceSize size, bool deviceLocal)
	{
		auto result = pDevice->create_uniform_buffer(size, deviceLocal, &m_buffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Cannot create uniform buffer; VkResult: " + std::to_string(result));
		}
	}

	BufferUsage UniformBuffer::usage() const
	{
		return BufferUsage::UBO;
	}

	void BufferObject::copyTo(VkDeviceSize offset, VkDeviceSize size, const void* data)
	{
		m_buffer.copyTo(offset, size, data);
	}

	void BufferObject::copyTo(const Buffer* data, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
	{
		m_buffer.device_->buffer_copy(data, &m_buffer, srcOffset, dstOffset, size);
	}

	void BufferObject::fill(VkDeviceSize offset, VkDeviceSize size, uint32_t data)
	{
		m_buffer.fill(offset, size, data);
	}

	void BufferObject::setup_descriptor()
	{
		m_buffer.setup_descriptor();
	}

	void BufferObject::set_name(const char* name)
	{
		m_buffer.device_->set_buffer_name(&m_buffer, name);
	}

	StagingBuffer::StagingBuffer(Device* pDevice, VkDeviceSize size)
	{
		auto result = pDevice->create_staging_buffer(size, &m_buffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Cannot create staging buffer; VkResult: " + std::to_string(result));
		}
	}

	BufferUsage StagingBuffer::usage() const
	{
		return BufferUsage::Staging;
	}

}
