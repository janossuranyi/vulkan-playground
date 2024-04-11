#ifndef VKJS_BUFFER_H_
#define VKJS_BUFFER_H_

#include "vkjs.h"

namespace jvk {
	
	enum class BufferUsage { UBO, SSBO, Vertex, Index, Staging };

	struct Device;
	struct Buffer {
		VkBuffer				buffer = VK_NULL_HANDLE;
		VmaAllocation			mem{};
		VkDeviceSize			size{};
		VkDescriptorBufferInfo	descriptor{};
		VkBufferUsageFlags		usage = VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
		VkMemoryPropertyFlags	mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		Device*					device_{};
		uint8_t*				mapped{};
		bool					alias = false;
		Buffer() = default;
		explicit Buffer(Device* device);
		bool map();
		void unmap();
		
		/* Do not call directly, use Device::destroy_buffer instead */
		void copyTo(VkDeviceSize offset, VkDeviceSize size, const void* data);
		void fill(VkDeviceSize offset, VkDeviceSize size, uint32_t data);
		void setup_descriptor();
	};

	/**
	* RAII implementation of Vulkan buffer
	*/
	class BufferObject 
	{
	public:
		BufferObject() : m_buffer() {}
		BufferObject(const BufferObject&) = delete;
		BufferObject(BufferObject&&) = delete;
		BufferObject& operator=(const BufferObject&) = delete;
		BufferObject& operator=(BufferObject&&) = delete;
		inline Buffer* buffer() { return &m_buffer; }
		inline const Buffer* buffer() const { return &m_buffer; }
		inline operator Buffer* () { return &m_buffer; }
		inline operator const Buffer* () const { return &m_buffer; }
		void copyTo(VkDeviceSize offset, VkDeviceSize size, const void* data);
		void copyTo(const Buffer* data, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);
		void fill(VkDeviceSize offset, VkDeviceSize size, uint32_t data);
		void setup_descriptor();
		void set_name(const char* name);
		virtual BufferUsage usage() const = 0;
		virtual ~BufferObject();
	protected:
		Buffer	m_buffer;
	};

	class UniformBuffer : public BufferObject
	{
	public:
		UniformBuffer(Device* pDevice, VkDeviceSize size, bool deviceLocal);
		UniformBuffer() = delete;
		virtual BufferUsage usage() const override;
	};

	class StagingBuffer : public BufferObject
	{
	public:
		StagingBuffer() = delete;
		StagingBuffer(Device* pDevice, VkDeviceSize size);
		virtual BufferUsage usage() const override;
	};
}
#endif // !VKJS_BUFFER_H_
