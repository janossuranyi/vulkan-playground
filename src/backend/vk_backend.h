#pragma once

#include "pch.h"

#include "vk.h"
#include <VkBootstrap.h>
#include <SDL.h>
#include <variant>
#include "vk_resources.h"
#include "vk_vertex.h"
#include "vk_device.h"
#include "vk_buffer.h"
#include "vk_shader_filemgr.h"
#include "common/resource_handles.h"
#include "common/resource_descriptions.h"
#include "jsrlib/jsr_memblock.h"

/* Vulkan renderer interface */

#include "vk_check.h"

namespace vks {


	struct DeferredDestroyTexture { ImageHandle handle; };

	using DeferredCommand = std::variant<DeferredDestroyTexture>;

	struct DescriptorSetGroup {
		VkDescriptorPool pool;
		VkDescriptorSetLayout layout;
		std::vector<VkDescriptorSet> sets;
	};

	struct GlobalImage {
		std::string name;
		uint32_t globalArrayIndex;
	};

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
		Buffer			globalUbo;
		std::vector<DeferredCommand> deferredCommands;
	};

	struct VkContext {
		VkInstance					instance = VK_NULL_HANDLE;
		VkPhysicalDevice			gpu = VK_NULL_HANDLE;
		VkSurfaceKHR				surface = VK_NULL_HANDLE;
		VkSwapchainKHR				swapchain = VK_NULL_HANDLE;
		VkFormat					swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
		std::vector<VkImage>		swapchainImages;
		std::vector<VkImageView>	swapchainImageViews;
		VkExtent2D					windowExtent{ 1700 , 900 };
		std::vector<VkCommandPool>	commandPools;
		VkDebugUtilsMessengerEXT	debug_messenger; // Vulkan debug output handle
		VkPhysicalDeviceProperties	gpuProperties;

		vkb::Swapchain				vkbSwapchain;
		vkb::Instance				vkbInstance;

		void cleanup() {
			vkb::destroy_surface(vkbInstance, surface);
			vkb::destroy_instance(vkbInstance);
		}
	};

	struct Samplers {
		VkSampler	anisotropeLinearRepeat;
		VkSampler	anisotropeLinearEdgeClamp;
		VkSampler	nearestEdgeClamp;
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

	class VulkanBackend {
	public:

		const uint32_t maxTextureCount = 1000;
		static constexpr size_t VERT_COUNT = 6;

		VulkanBackend() :
			m_depthImageHandle(),
			m_descriptorPool(),
			m_currentFrame(),
			m_vulkanContext(),
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
			m_globalShaderDataDescriptors(),
			m_globalTextureArrayDescriptors(),
			m_uploadContext(),
			m_defaultSamplers(),
			m_globalShaderData(),
			m_textures(),
			m_buffers(),
			m_window(),
			device()
		{}

		~VulkanBackend();

		bool init(SDL_Window* window, int concurrency);
		void renderFrame(uint32_t frame, bool present);
		ImageHandle createImageFromFile(const std::filesystem::path& filename, const std::string& name);
		ImageHandle createImageFromGliTexture(const std::string& name, const gli::texture& gliTexture);
		ImageHandle createImage(const ImageDescription& desc, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);
		SamplerHandle createSampler(const VkSamplerCreateInfo& samplerCI);
		void uploadImage(ImageHandle hImage, uint32_t layer, uint32_t level, const void* data, size_t size);
		void updateGraphicPassDescriptorSet(GraphicsPassHandle hPass, const RenderPassResources& resources);
		void updateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorSet set, const RenderPassResources& resources);
		void freeImage(ImageHandle textureHandle);
		GraphicsPassHandle createGraphicsPass(const GraphicsPassDescription& desc);
		BufferHandle createUniformBuffer(const UniformBufferDescription& ci);
		BufferHandle createStorageBuffer(const StorageBufferDescription& ci);
		std::vector<MeshHandle> createMeshes(const std::vector<MeshDescription>& ci);
		uint32_t getTextureGlobalIndex(ImageHandle textureHandle);
		void setTestBuffer(BufferHandle handle);
		void postDestroyTexture(ImageHandle hTexture);

		void addRenderMesh(const MeshRenderInfo& h);
		void windowResized(void);
		void quit();
		uint32_t getUniformBlockOffsetAlignment() const;
		uint32_t getAlignedUniformBlockOffset(uint32_t offset) const;
		uint32_t getFlightFrame() const {
			return m_currentFrame % INFLIGHT_FRAMES;
		}
		uint32_t getCurrentFrameNumber() const { return m_currentFrame; }

		void operator()(const DeferredDestroyTexture& cmd);

	protected:
		virtual void onStartup() {}
	private:
		VulkanDevice* device;
		bool get_image_by_name(const std::string& nane, ImageHandle* outhandle, Image* out);
		Image& get_imageref(ImageHandle handle);
		uint32_t insert_image_to_list(const Image& image);
		bool create_graphics_pass_internal(const GraphicsPassDescription& desc, GraphicsPass* out);

		VkResult create_texture_internal(const gli::texture& texture, Image& dest);

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
		void transfer_buffer_immediate(const Buffer& dest, uint32_t size, const void* data);
		void add_texture_to_global_array(Image& texture);
		void free_global_texture(uint32_t index);

		VkResult create_buffer(VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VkDeviceSize size, Buffer* buffer, const void* data);

		VkSubresourceLayout get_subresource_layout(VkImage image, const VkImageSubresource& subresource);

		SDL_Window*					m_window;
		ShaderFileManager			m_shaders;
		std::vector<VkCommandPool>	m_drawcallCommandPools;
		VkDescriptorPool			m_descriptorPool;
		VkContext					m_vulkanContext;
		UploadContext				m_uploadContext;
		DeletionQueue				m_deletions;
		int							m_concurrency;

		VkQueue						m_graphicQueue;
		VkQueue						m_computeQueue;
		VkQueue						m_transferQueue;

		VkFramebuffer				m_defaultFramebuffer;
		FrameData					m_frameData[INFLIGHT_FRAMES];
		std::atomic_uint			m_currentFrame;
		VkCommandPool				m_commandPool;
		VkRenderPass				m_defaultRenderPass;
		std::vector<VkFramebuffer>	m_framebuffers;
		ImageHandle					m_depthImageHandle;
		VkFormat					m_depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
		VkPipelineLayout			m_defaultPipelineLayout;
		VkPipeline					m_defaultPipeline;

		std::mutex					m_globalTextureArrayMutex;
		DescriptorSetGroup			m_globalShaderDataDescriptors;
		DescriptorSetGroup			m_globalTextureArrayDescriptors;
		std::atomic_uint			m_globalTextureUsed = 0;
		std::vector<uint32_t>		m_freeTextureIndexes;

		Samplers					m_defaultSamplers;
		std::vector<Sampler>		m_samplers;

		GlobalShaderDescription		m_globalShaderData;

		struct {
			std::unordered_map<std::string, uint32_t> name_to_index;
			std::vector<Image> list;
			std::vector<uint32_t> freeSlots;
			std::mutex mutex;
		} m_textures;
		
		struct {
			std::vector<Buffer> list;
		} m_buffers;

		std::vector<GraphicsPass> m_graphicsPasses;
		std::vector<Mesh> m_meshes;
		std::vector<MeshRenderInfo>	m_renderQueue;
		std::unordered_map<std::string, GlobalImage> m_globalImages;

		bool m_windowResized = false;
		BufferHandle testBuffer;
	};

}