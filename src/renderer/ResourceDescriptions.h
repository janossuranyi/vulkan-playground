#pragma once

#include "renderer/Vulkan.h"
#include "renderer/ResourceHandles.h"

namespace jsr {

	enum IndexType { INDEX_TYPE_UINT16, INDEX_TYPE_UINT32 };

	enum class ImageUsageFlags : uint32_t {
		Storage = 0x00000001,
		Sampled = 0x00000002,
		Attachment = 0x00000004,
	};

	inline ImageUsageFlags operator&(const ImageUsageFlags l, const ImageUsageFlags r) {
		return ImageUsageFlags(uint32_t(l) & uint32_t(r));
	}

	inline ImageUsageFlags operator|(const ImageUsageFlags l, const ImageUsageFlags r) {
		return ImageUsageFlags(uint32_t(l) | uint32_t(r));
	}

	struct UniformBufferDescription {
		size_t size;
		const void* initialData;
	};

	struct StorageBufferDescription {
		size_t size;
		const void* initialData;
		bool readOnly;
	};

	struct ImageDescription {
		uint32_t width;
		uint32_t height;
		uint32_t depth = 1;
		uint32_t faces = 1;
		uint32_t layers = 1;
		uint32_t levels = 1;
		ImageUsageFlags usageFlags;
		bool cubeMap = false;
		VkFormat format;
		std::string name;
	};

	struct VertexInputDescription {

		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;

		VkPipelineVertexInputStateCreateFlags flags = 0;
	};

	struct GraphicsPipelineShaderInfo {
		std::string vertexShader;
		std::string fragmentShader;
		std::string geometryShader;
	};

	struct ComputePipelineShaderInfo {
		std::string computeShader;
	};

	struct GraphichPipelineShaderBinary {
		std::vector<uint8_t> vertexSPIRV;
		std::vector<uint8_t> fragmentSPIRV;
		std::vector<uint8_t> geometrySPIRV;
	};

	struct ComputePipelineShaderBinary {
		std::vector<uint8_t> computeSPIRV;
	};

	struct ImageResource {
		ImageResource(
			handle::Image	image,
			uint32_t		binding,
			uint32_t		level,
			bool			write = false) :
			image(image), binding(binding), level(level), write(write) {}

		ImageResource(
			handle::Image	image,
			handle::Sampler sampler,
			uint32_t		binding,
			uint32_t		level) :
			image(image), sampler(sampler), binding(binding), level(level) {}
		handle::Image image;
		handle::Sampler sampler;
		uint32_t binding;
		uint32_t level;
		bool write = false;
	};

	struct SamplerResource {
		SamplerResource(
			handle::Sampler sampler,
			uint32_t		binding) :
			sampler(sampler), binding(binding) {}
		handle::Sampler sampler;
		uint32_t binding;
	};

	struct StorageBufferResource {
		StorageBufferResource(
			const handle::StorageBuffer buffer,
			const bool                  readOnly,
			const uint32_t              binding) : buffer(buffer), readOnly(readOnly), binding(binding) {};
		handle::StorageBuffer buffer;
		bool                readOnly;
		uint32_t            binding;
	};

	struct UniformBufferResource {
		UniformBufferResource(
			const handle::UniformBuffer buffer,
			const uint32_t              binding) : buffer(buffer), binding(binding) {};
		handle::UniformBuffer	buffer;
		uint32_t				binding;
	};

	struct RenderPassResources {
		std::vector<SamplerResource>        samplers;
		std::vector<StorageBufferResource>  storageBuffers;
		std::vector<UniformBufferResource>  uniformBuffers;
		std::vector<ImageResource>          sampledImages;
		std::vector<ImageResource>          combinedImageSamplers;
		std::vector<ImageResource>          storageImages;
	};

	struct ComputePassInfo {
		ComputePassInfo(
			const handle::ComputePipeline pipelineHandle_,
			const RenderPassResources& resources_,
			uint32_t invocationCount_x,
			uint32_t invocationCount_y,
			uint32_t invocationCount_z) :
			pipelineHandle(pipelineHandle_),
			resources(resources_) {
			invocationCount[0] = invocationCount_x;
			invocationCount[1] = invocationCount_y;
			invocationCount[2] = invocationCount_z;
		}
		handle::ComputePipeline pipelineHandle;
		handle::DescriptorSet descriptorSet;
		RenderPassResources resources;
		uint32_t invocationCount[3];
	};

	struct GraphicsPassInfo {
		GraphicsPassInfo(
			handle::GraphicsPipeline				pipelineHandle,
			const std::vector<handle::Image>&		targets,
			const RenderPassResources&				resources) :
			pipelineHandle(pipelineHandle),
			targets(targets),
			frameBuffer(VK_NULL_HANDLE),
			resources(resources) {}

		handle::GraphicsPipeline pipelineHandle;
		std::vector<handle::Image> targets;
		RenderPassResources resources;
		VkFramebuffer frameBuffer;
	};

	enum class RenderPassType { Graphics, Compute };
	struct RenderPassInfo {
		RenderPassType type = RenderPassType::Graphics;
		uint32_t index = -1;
	};

	struct FillUniformBufferData {
		handle::UniformBuffer buffer;
		std::vector<char> data;
	};

	struct FillStorageBufferData {
		handle::StorageBuffer buffer;
		std::vector<char> data;
	};

	struct MeshDescription {
		uint32_t vertexCount;
		uint32_t indexCount;
		IndexType indexType;
		std::vector<char> vertexBuffer;
		std::vector<uint16_t> indexBuffer;

	};

	struct VertexCacheEntry {
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t baseVertex;
	};

	struct DrawMeshDescription {
		handle::GraphicsPipeline pipeline;
		handle::Mesh mesh;
		handle::DescriptorSet meshResources;
		VertexCacheEntry vc;
		uint32_t instanceCount;
		uint32_t firstInstance;
		VkPrimitiveTopology mode;
	};

	struct GlobalShaderVariables {
		glm::mat4	mView;
		glm::mat4	mProjection;
		glm::mat4	mInvProjection;
		glm::mat4	mViewProjection;
		glm::vec4	vDisplaySize;
		glm::vec4	vAmbientColor;
		glm::vec4	vFogColor;
		glm::uint	uFramenumber;
		int			uWhiteImageIndex;
		int			uBlackImageIndex;
		int			uFlatNormalIndex;
		float		fExposureBias;
		float		fFarPlane;
		float		fNearPlane;
		float		fFogDensity;
	};
	static_assert((sizeof(GlobalShaderVariables) & 15) == 0);


}