#pragma once

#include "common/resource_handles.h"
#include "common/resource_descriptions.h"
#include "vk_buffer.h"
#include "pch.h"

namespace vks {
	constexpr uint32_t invalidIndex = std::numeric_limits<uint32_t>::max();

	struct GlobalShaderDescription {
		glm::mat4	g_modelMatrix;
		glm::mat4	g_viewMatrix;
		glm::mat4	g_projectionMatrix;
		glm::vec4	g_windowExtent;
		float		g_frameCount;
		glm::uint	g_whiteImageIndex;
	};

	struct FragmentPushConstants {
		int texture0 = -1;
		int texture1 = -1;
		int texture2 = -1;
		int texture3 = -1;
	};

	struct VulkanDevice;
	struct Image {
		VkImage		vulkanImage;
		VkImageType	type;
		VkFormat	format;
		VmaAllocation memory;
		VulkanDevice* device;
		VkImageView view;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		std::vector<VkImageView> layerViews;
		VkSampler	sampler;
		uint32_t	globalIndex = invalidIndex;
		std::string name;
		VkExtent3D	extent;
		uint32_t	levels;
		uint32_t	layers;
		bool		isCubemap;
		bool		hasWritten;
		uint32_t	faces() const { return isCubemap ? 6 : 1; }
		Image() :
			type(VK_IMAGE_TYPE_1D),
			isCubemap(),
			hasWritten(),
			memory(),
			format(VK_FORMAT_R8_UNORM),
			vulkanImage(),
			view(),
			sampler(),
			name(),
			extent(),
			levels(),
			device(),
			layers(){}

		Image(const std::string& name) : Image() {
			this->name = name;
		}
	};

	struct Mesh {
		Mesh(
			uint32_t vertexCount,
			uint32_t indexCount
		) : vertexCount(vertexCount), indexCount(indexCount) {}

		uint32_t vertexCount;
		uint32_t indexCount;
		Buffer vertexBuffer;
		Buffer indexBuffer;
		VkIndexType indexType = VK_INDEX_TYPE_UINT16;
	};

	struct GraphicsPass {
		GraphicsPassDescription description;

		GraphicShadersHandle shaderHandle;
		VkRenderPass renderPass;
		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
		VkDescriptorSet descriptorSets[INFLIGHT_FRAMES] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
		VkDescriptorSetLayout descriptorSetLayaout;
		std::vector<VkCommandBuffer> meshCommandBuffers;
		std::vector<VkClearValue> clearValues;
		size_t pushConstantSize = 0;
	};

	struct Sampler {
		VkSampler vulkanSampler;
		VkSamplerCreateInfo info;
	};

	VkCompareOp convertCompareOp(CompareOp op);
	VkPrimitiveTopology convertTopology(PrimitiveTopology x);
	VkCullModeFlags convertCullMode(CullMode x);
	VkPolygonMode convertPolygonMode(PolygonMode x);
	VkFrontFace convertFrontFace(FrontFace x);
	VkDescriptorType convertDescriptorType(ResourceType x);
	VkBlendOp convertBlendOp(BlendOp x);
	VkBlendFactor convertBlendFactor(BlendFactor x);
	bool isDepthFormat(VkFormat x);
	bool isStencilFormat(VkFormat x);
}