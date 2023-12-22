#pragma once
#include "renderer/ResourceHandles.h"
#include "renderer/VulkanBuffer.h"

namespace jsr {

	class VulkanPipeline;

	struct GraphicsPipeline {
		std::unique_ptr<VulkanPipeline> vkPipeline;
		std::vector<VkCommandBuffer> drawCommandBuffers;
	};

	struct ComputePipeline {
		std::unique_ptr<VulkanPipeline> vkPipeline;
		uint32_t groupSize[3] = { 1,1,1 };
	};

	struct ComputePass {
		ComputePass(
			handle::ComputePipeline		pipelineHandle,
			const uint32_t				workgroupSize[3]) : pipelineHandle(pipelineHandle) {
			this->workgroupSize[0] = workgroupSize[0];
			this->workgroupSize[1] = workgroupSize[1];
			this->workgroupSize[2] = workgroupSize[2];
		}
		handle::ComputePipeline pipelineHandle;
		uint32_t workgroupSize[3] = { 1,1,1 };
	};

	struct GraphicsPass {
		GraphicsPass(
			handle::GraphicsPipeline			pipelineHandle,
			const std::vector<handle::Image>&	targets,
			VkFramebuffer						frameBuffer) :
			pipelineHandle(pipelineHandle),
			targets(targets),
			frameBuffer(frameBuffer) {}

		handle::GraphicsPipeline pipelineHandle;
		std::vector<handle::Image> targets;
		VkFramebuffer frameBuffer;
	};

	struct Sampler {
		VkSampler sampler;
	};

	struct Mesh {
		Mesh(
			uint32_t vertexCount,
			uint32_t indexCount
		) : vertexCount(vertexCount), indexCount(indexCount) {}

		uint32_t vertexCount;
		uint32_t indexCount;
		std::unique_ptr<VulkanBuffer> vertexBuffer;
		std::unique_ptr<VulkanBuffer> indexBuffer;
		VkIndexType indexType = VK_INDEX_TYPE_UINT16;
	};

}