#pragma once

#include "pch.h"
#include "renderer/VulkanInitializers.h"
#include "renderer/VulkanCheck.h"
#include "renderer/VulkanDevice.h"
#include "renderer/VulkanSwapchain.h"
#include "renderer/VulkanBuffer.h"
#include "renderer/VulkanImage.h"
#include "renderer/VulkanBarrier.h"
#include "renderer/ResourceDescriptions.h"
#include "renderer/ResourceHandles.h"
#include "renderer/Resources.h"
#include "renderer/FrameCounter.h"
#include "jsrlib/jsr_resources.h"

#include <memory>

namespace jsr {

	std::vector<char> toCharArray(size_t size, const void* data);

	struct RenderPassBarriers {
		std::vector<ImageBarrier> imageBarriers;
		std::vector<BufferBarrier> bufferBarriers;
		std::vector<MemoryBarrier> memoryBarriers;
	};

	class VulkanRenderer {
	public:
		
		static const VkFormat DEPTH_FORMAT = VK_FORMAT_D24_UNORM_S8_UINT;

		static const uint32_t maxTextureCount = 256;

		VulkanRenderer(VulkanDevice* device, VulkanSwapchain* swapchain, int numDrawThreads);
		~VulkanRenderer();
		void renderFrame(bool present);
		void setWindowResized();
		VulkanSwapchain* getSwapchain();
		VkFramebuffer createTemporaryFrameBuffer(const VkFramebufferCreateInfo& framebufferInfo);
		handle::UniformBuffer createUniformBuffer(const UniformBufferDescription& desc);
		handle::StorageBuffer createStorageBuffer(const StorageBufferDescription& desc);
		void freeStorageBuffer(handle::StorageBuffer handle);
		handle::Image createImage(const ImageDescription& desc);
		handle::Image createImage(const std::filesystem::path& filename, const std::string& name);
		handle::Image getSwapchainImage() const;
		handle::Sampler createSampler(const VkSamplerCreateInfo& samplerInfo);
		handle::DescriptorSet createDescriptorSet(handle::GraphicsPipeline pipeline, const RenderPassResources& resources);
		handle::DescriptorSet createDescriptorSet(handle::ComputePipeline pipeline, const RenderPassResources& resources);

		std::vector<handle::Mesh> createMeshes(const std::vector<MeshDescription>& desc);
		void setMergedMesh(handle::Mesh hMesh);
		void freeImage(handle::Image handle);
		void prepareDrawCommands();
		void updateUniformBufferData(handle::UniformBuffer, uint32_t size, const void* data);
		void updateStorageBufferData(handle::StorageBuffer, uint32_t size, const void* data);

		template<typename TechnicType>
		handle::GraphicsPipeline createGraphicsPipeline(const GraphicsPipelineShaderInfo& shaderInfo);
		template<typename TechnicType>
		handle::ComputePipeline createComputePipeline(const ComputePipelineShaderInfo& shaderInfo, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);
		void setGraphicsPass(const GraphicsPassInfo& passInfo);
		void setComputePass(const ComputePassInfo& passInfo);
		void sendDrawCmd(uint32_t numVerts, handle::GraphicsPipeline hPipe, uint32_t threadId, const void* pushConstants);
		void drawMeshes(const std::vector<DrawMeshDescription>& descr, uint32_t threadId, const void* pushConstants);

		bool prepareNextFrame();
		VkDescriptorSetLayout getGlobalVarsLayout() const;
		VkDescriptorSetLayout getGlobalTexturesLayout() const;
		uint32_t getImageGlobalIndex(handle::Image handle) const;
		void fillBufferImmediate(handle::UniformBuffer handle, uint32_t size, const void* data);
		void fillBufferImmediate(handle::StorageBuffer handle, uint32_t size, const void* data);
		GlobalShaderVariables gpass_data;

		handle::Image getZBuffer() const;
		handle::Image getWhiteImage() const;
		handle::Image getBlackImage() const;
		handle::Image getFlatDepthImage() const;
		handle::Sampler getDefaultAnisotropeRepeatSampler() const;
	private:
		struct PerFrameData {
			VkCommandBuffer				commandBuffer;
			VkCommandPool				commandPool;
			VkSemaphore					renderSemaphore;
			VkSemaphore					presentSemaphore;
			VkFence						renderFence;
			VkDescriptorPool			globalShaderVarsPool;
			VkDescriptorSet				globalShaderVarsSet;
			handle::UniformBuffer		globalVarsUbo;
			std::vector<VkFramebuffer>	temporaryFramebuffers;
		} m_framedata[FrameCounter::INFLIGHT_FRAMES];

		struct samplers_t {
			handle::Sampler	anisotropeLinearRepeat;
			handle::Sampler	anisotropeLinearEdgeClamp;
			handle::Sampler	nearestEdgeClamp;
		};

		void init();
		void init_swapchain_images();
		void init_samplers();
		void init_gvars_descriptor_set();
		void init_global_texture_descriptor_set();
		void init_default_images();
		void init_descriptor_pool();
		void init_depth_image();
		void init_imgui();
		void reset();

		void shutdown();
		void updateUniforms(PerFrameData& frame);
		void submitGraphicsPasses(PerFrameData& frame);
		void updateDescriptorSet(VkDescriptorSet set, const RenderPassResources& resources);
		void processDeferredBufferFills(VkCommandBuffer cmd);
		void processDeferredBufferFills2();
		void renderImgui(VkCommandBuffer cmd, VkExtent2D extent);
		const std::vector<RenderPassBarriers> createRenderPassBarriers();

		handle::UniformBuffer createUniformBufferInternal(const UniformBufferDescription& desc);
		handle::StorageBuffer createStorageBufferInternal(const StorageBufferDescription& desc);
		handle::Image createImageInternal(const ImageDescription& desc);
		handle::Image createImageInternal(const std::filesystem::path& filename, const std::string& name);
		handle::Image allocateImage(std::unique_ptr<VulkanImage>& param);
		void updateGlobalTextureArray(uint32_t arrayIndex, handle::Image imageHandle);

		inline PerFrameData& currentFrameRef() { return m_framedata[FrameCounter::getCurrentFrame() % FrameCounter::INFLIGHT_FRAMES]; }

		void cleanupTemporaryFrameBuffers(PerFrameData* frame);
		std::unique_ptr<VulkanImage>& getImageRef(const handle::Image handle);
		const std::unique_ptr<VulkanImage>& getImageRef(const handle::Image handle) const;

		struct descriptorSet_t {
			VkDescriptorSet set;
		};

		int							m_numThreads = 4;
		VulkanDevice*				m_device;
		VulkanSwapchain*			m_swapchain;
		std::vector<std::unique_ptr<VulkanImage>>
									m_swapchainImages;


		std::vector<descriptorSet_t> m_descriptorSets;
		std::vector<uint32_t>		m_descriptorSetsFreeIndexes;
		std::vector<FillUniformBufferData>
									m_deferredUniformBufferFills;
		std::vector<FillStorageBufferData>
									m_deferredStorageBufferFills;
		std::unique_ptr<VulkanBuffer>
									m_deferredStagingBuffer[FrameCounter::INFLIGHT_FRAMES];
		std::unique_ptr<VulkanBuffer>
									m_stagingBuffer;

		std::vector<VkCommandPool>	m_drawCommandPools;
		VkCommandPool				m_defaultCommandPool;
		VkDescriptorSetLayout		m_globalShaderVarsLayout;
		VkDescriptorSetLayout		m_gloalTexturesLayout;
		VkDescriptorPool			m_globalTexturesPool;
		VkDescriptorPool			m_defaultDescriptorPool;
		VkDescriptorSet				m_globalTexturesArraySet;
		VkQueue						m_graphicQueue;
		VkRenderPass				m_imguiRenderPass;

		std::vector<std::unique_ptr<VulkanBuffer>>
									m_uniformBuffers;
		std::vector<std::unique_ptr<VulkanBuffer>>
									m_storageBuffers;
		std::vector<uint32_t>		m_storageBufferFreeIndexes;
		std::vector<std::unique_ptr<VulkanImage>>
									m_images;
		std::vector<uint32_t>		m_imagesFreeIndexes;

		std::vector<GraphicsPipeline>
									m_graphicsPipelines;
		std::vector<ComputePipeline>
									m_computePipelines;
		std::vector<RenderPassInfo>
									m_activePasses;
		std::vector<GraphicsPassInfo>
									m_activeGraphicsPasses;
		std::vector<ComputePassInfo>
									m_activeComputePasses;
		std::vector<handle::Image>	m_globalTextureArray;
		std::vector<uint32_t>		m_globalTextureArrayFreeIndexes;
		std::vector<Mesh>			m_meshes;
		std::vector<Sampler>		m_samplers;
		samplers_t					m_globalSamplers;
		bool						m_windowResized = false;

		handle::Mesh				m_mergedMesh;
		handle::Image				m_zBuffer;
		handle::Image				m_whiteImage;
		handle::Image				m_blackImage;
		handle::Image				m_flatDepthImage;
	};


	template<typename TechnicType>
	handle::GraphicsPipeline VulkanRenderer::createGraphicsPipeline(const GraphicsPipelineShaderInfo& shaderInfo)
	{
		handle::GraphicsPipeline handle = {};
		handle.index = (uint32_t)m_graphicsPipelines.size();
		GraphicsPipeline pipeline = {};
		GraphichPipelineShaderBinary binshaders;
		binshaders.vertexSPIRV = jsrlib::Filesystem::root.ReadFile(shaderInfo.vertexShader);
		binshaders.fragmentSPIRV = jsrlib::Filesystem::root.ReadFile(shaderInfo.fragmentShader);

		auto cmdBufCI = vkinit::command_buffer_allocate_info(VK_NULL_HANDLE, 1, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		for (size_t k = 0; k < FrameCounter::INFLIGHT_FRAMES; ++k) {
			for (size_t i = 0; i < m_numThreads; ++i) {
				pipeline.drawCommandBuffers.push_back({});
				cmdBufCI.commandPool = m_drawCommandPools[i];
				VK_CHECK(vkAllocateCommandBuffers(m_device->vkDevice, &cmdBufCI, &pipeline.drawCommandBuffers.back()));
			}
		}
		pipeline.vkPipeline = std::make_unique<TechnicType>();
		pipeline.vkPipeline->setDescriptorPool(m_defaultDescriptorPool);
		pipeline.vkPipeline->setDevice(m_device);
		pipeline.vkPipeline->setEngine(this);
		pipeline.vkPipeline->createPipeline(binshaders);

		m_graphicsPipelines.push_back(std::move(pipeline));

		return handle;
	}

	template<typename TechnicType>
	inline handle::ComputePipeline VulkanRenderer::createComputePipeline(const ComputePipelineShaderInfo& shaderInfo, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
	{
		handle::ComputePipeline handle = {};
		handle.index = uint32_t(m_computePipelines.size());
		ComputePipeline pipeline = {};
		ComputePipelineShaderBinary binshader;
		binshader.computeSPIRV = jsrlib::Filesystem::root.ReadFile(shaderInfo.computeShader);
		pipeline.groupSize[0] = groupSizeX;
		pipeline.groupSize[1] = groupSizeY;
		pipeline.groupSize[2] = groupSizeZ;

		pipeline.vkPipeline = std::make_unique<TechnicType>();
		pipeline.vkPipeline->setDevice(m_device);
		pipeline.vkPipeline->setEngine(this);
		pipeline.vkPipeline->setDescriptorPool(m_defaultDescriptorPool);
		pipeline.vkPipeline->createPipeline(binshader);

		m_computePipelines.push_back(std::move(pipeline));

		return handle;
	}

}