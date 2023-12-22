#pragma once

#include "pch.h"

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <VkBootstrap.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_resources.h"
#include "vk_vertex.h"
#include "vk_device.h"
#include "vk_buffer.h"
#include "common/resource_handles.h"
#include "common/resource_descriptions.h"
#include "jsrlib/jsr_memblock.h"

/* Vulkan renderer interface */

/*

Basic elements
==============

Resources
---------

Sampled image
Sampler
Combined sampler+image
Storage image
Uniform buffer
Storage buffer

*/

#include "vk_check.h"
#define INFLIGHT_FRAMES 2

namespace vks {

	struct DescriptorPoolAllocationSizes {
		uint32_t setCount = 0;
		uint32_t imageSampled = 0;
		uint32_t imageStorage = 0;
		uint32_t uniformBuffer = 0;
		uint32_t storageBuffer = 0;
		uint32_t sampler = 0;
	};

	struct UploadContext {
		VkFence uploadFence;
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;
	};

	struct FrameData {
		VkCommandBuffer	commandBuffer;
		VkSemaphore		renderSemaphore;
		VkSemaphore		presentSemaphore;
		VkFence			renderFence;
		BufferResource			globalUbo;
		VkDescriptorSet	globalDescriptor;
	};

	struct VkContext {
		VkInstance					instance = VK_NULL_HANDLE;
		//VkDevice					device = VK_NULL_HANDLE;
		VkPhysicalDevice			gpu = VK_NULL_HANDLE;
		VkSurfaceKHR				surface = VK_NULL_HANDLE;
		VkSwapchainKHR				swapchain = VK_NULL_HANDLE;
		VkFormat					swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
		std::vector<VkImage>		swapchainImages;
		std::vector<VkImageView>	swapchainImageViews;
		VkExtent2D					windowExtent{ 1700 , 900 };
		std::vector<VkCommandPool>	commandPools;
		VkDebugUtilsMessengerEXT	debug_messenger; // Vulkan debug output handle
		SDL_Window* window;
		VkPhysicalDeviceProperties	gpuProperties;
	};

	struct Samplers {
		VkSampler	anisotropeLinearRepeat;
		VkSampler	anisotropeLinearCube;
	};

	class DeletionQueue
	{
	private:
		std::deque<std::function<void()>> m_deletors;

	public:
		void push_function(const std::function<void()>&& function) {
			m_deletors.push_back(function);
		}

		void flush() {
			// reverse iterate the deletion queue to execute all the functions
			for (auto it = m_deletors.rbegin(); it != m_deletors.rend(); it++) {
				(*it)(); //call the function
			}

			m_deletors.clear();
		}
	};

	class RenderBackend {
	public:

		const uint32_t maxTextureCount = 1000;
		static constexpr size_t VERT_COUNT = 6;

		RenderBackend() :
			m_vkctx(),
			m_concurrency(0),
			m_graphicQueue(),
			m_transferQueue(),
			m_defaultFramebuffer(),
			m_commandPool(),
			m_defaultRenderPass(),
			m_frameData(),
			m_computeQueue(),
			m_defaultPipeline(),
			m_defaultPipelineLayout(),
			m_defaultDescriptorPool(),
			m_defaultSetLayout(),
			m_globalTextureArrayDescriptorSet(),
			m_globalTextureArrayLayout(),
			m_globalTextureArrayPool(),
			m_triangles(),
			m_uploadContext(),
			m_samplers(),
			m_globalShaderData(),
			m_textures(),
			m_buffers(),
			device()
		{}

		~RenderBackend();

		bool init(int concurrency);
		void renderFrame(uint32_t frameNumber, bool present);
		TextureHandle createTexture(const std::filesystem::path& filename, const std::string& name);
		TextureHandle createTexture(const std::string& name, const gli::texture& gliTexture);
		std::vector<MeshHandle> createMeshes(const std::vector<MeshResourceCreateInfo>& ci);
		void addRenderMesh(const MeshRenderInfo& h);
		void windowResized(int width, int height);
		void quit();

	protected:
		virtual void onStartup() {}
	private:
		VulkanDevice* device;

		bool get_texture(const std::string& nane, TextureHandle* outhandle, TextureResource* out);
		void get_texture(TextureHandle handle, TextureResource* out);

		VkResult createTextureInternal(const gli::texture& texture, TextureResource& dest);

		void upload_image(const ImageResource& image, const VkExtent3D& extent, uint32_t mipLevel, const void* data, VkDeviceSize size);
		bool init_vulkan();
		bool init_swapchain();
		bool init_commands();
		bool init_default_renderpass();
		bool init_framebuffers();
		bool init_sync_structures();
		bool init_descriptors();
		bool init_pipelines();
		bool init_buffers();
		bool init_images();

		bool init_global_texture_array_layout();
		bool init_global_texture_array_descriptor();

		bool init_samplers();

		void cleanup_swapchain();
		void recreate_swapchain();

		void immediate_submit(const std::function<void(VkCommandBuffer)>&& function);
		void add_texture_to_global_array(TextureResource& texture);
		VkResult create_buffer(VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VkDeviceSize size, BufferResource* buffer, const void* data);

		VkSubresourceLayout get_subresource_layout(VkImage image, const VkImageSubresource& subresource);

		VkContext					m_vkctx;
		UploadContext				m_uploadContext;
		DeletionQueue				m_deletions;
		int							m_concurrency;

		VkQueue						m_graphicQueue;
		VkQueue						m_computeQueue;
		VkQueue						m_transferQueue;

		VkFramebuffer				m_defaultFramebuffer;
		FrameData					m_frameData[INFLIGHT_FRAMES];
		VkCommandPool				m_commandPool;
		VkRenderPass				m_defaultRenderPass;
		std::vector<VkFramebuffer>	m_framebuffers;
		VkPipelineLayout			m_defaultPipelineLayout;
		VkPipeline					m_defaultPipeline;
		VkDescriptorSetLayout		m_defaultSetLayout;
		VkDescriptorPool			m_defaultDescriptorPool;
		VkDescriptorSetLayout		m_globalTextureArrayLayout;
		VkDescriptorPool			m_globalTextureArrayPool;
		VkDescriptorSet				m_globalTextureArrayDescriptorSet;
		std::atomic_int				m_globalTextureUsed = 0;

		Vertex						m_triangles[VERT_COUNT];
		Samplers					m_samplers;

		GlobalShaderDescription		m_globalShaderData;
		BufferResource						m_vertexBuffer;

		struct {
			std::unordered_map<std::string, uint32_t> namehash;
			std::vector<TextureResource> list;
			std::mutex mutex;
		} m_textures;
		
		struct {
			std::vector<BufferResource> list;
		} m_buffers;

		std::vector<MeshResource> m_meshes;

		std::vector<MeshRenderInfo>	m_renderQueue;
		bool					m_windowResized = false;
	};

}