// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "renderer/Vulkan.h"
#include "pch.h"
namespace vkinit {

	class SamplerBuilder {
	public:
		SamplerBuilder();
		SamplerBuilder& next(const void* pNext);
		SamplerBuilder& flags(VkSamplerCreateFlags flags_);
		SamplerBuilder& magFilter(VkFilter magFilter_);
		SamplerBuilder& minFilter(VkFilter minFilter_);
		SamplerBuilder& filter(VkFilter filter_);
		SamplerBuilder& mipmapMode(VkSamplerMipmapMode mipmapMode_);
		SamplerBuilder& addressMode(VkSamplerAddressMode addressMode_);
		SamplerBuilder& addressModeU(VkSamplerAddressMode addressModeU_);
		SamplerBuilder& addressModeV(VkSamplerAddressMode addressModeV_);
		SamplerBuilder& addressModeW(VkSamplerAddressMode addressModeW_);
		SamplerBuilder& mipLodBias(float mipLodBias_);
		SamplerBuilder& anisotropyEnable(VkBool32 anisotropyEnable_);
		SamplerBuilder& maxAnisotropy(float maxAnisotropy_);
		SamplerBuilder& compareEnable(VkBool32 compareEnable_);
		SamplerBuilder& compareOp(VkCompareOp compareOp_);
		SamplerBuilder& minLod(float minLod_);
		SamplerBuilder& maxLod(float maxLod_);
		SamplerBuilder& borderColor(VkBorderColor borderColor_);
		SamplerBuilder& unnormalizedCoordinates(VkBool32 unnormalizedCoordinates_);
		SamplerBuilder& set_defaults();
		VkSamplerCreateInfo build();
	private:
		VkSamplerCreateInfo m_create_info;
	};
	VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

	VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding, uint32_t arrayIndex);

	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

	VkSubmitInfo submit_info(VkCommandBuffer* cmd);

	VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

	VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);

	VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule);

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);

	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);

	VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

	VkPipelineColorBlendAttachmentState color_blend_attachment_state();

	VkPipelineLayoutCreateInfo pipeline_layout_create_info();

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

	VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);

	VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, uint32_t mipCount, VkImageAspectFlags aspectFlags);

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

	VkCommandPoolCreateInfo create_commandpool_info(uint32_t queueFamiliy, VkCommandPoolCreateFlags flags);

	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count, bool secondary = false);

	VkFramebufferCreateInfo framebuffer_create_info(VkRenderPass renderPass, VkExtent2D extent, const std::vector<VkImageView>& attachments);

	VkShaderModuleCreateInfo shader_module_create_info(size_t codeSize, const uint32_t* pCode);
}

