// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk.h"

namespace vkinitializer {

	class sampler_builder {
	public:
		sampler_builder();
		sampler_builder& next(const void* pNext);
		sampler_builder& flags(VkSamplerCreateFlags flags_);
		sampler_builder& magFilter(VkFilter magFilter_);
		sampler_builder& minFilter(VkFilter minFilter_);
		sampler_builder& filter(VkFilter filter_);
		sampler_builder& mipmapMode(VkSamplerMipmapMode mipmapMode_);
		sampler_builder& addressMode(VkSamplerAddressMode addressMode_);
		sampler_builder& addressModeU(VkSamplerAddressMode addressModeU_);
		sampler_builder& addressModeV(VkSamplerAddressMode addressModeV_);
		sampler_builder& addressModeW(VkSamplerAddressMode addressModeW_);
		sampler_builder& mipLodBias(float mipLodBias_);
		sampler_builder& anisotropyEnable(VkBool32 anisotropyEnable_);
		sampler_builder& maxAnisotropy(float maxAnisotropy_);
		sampler_builder& compareEnable(VkBool32 compareEnable_);
		sampler_builder& compareOp(VkCompareOp compareOp_);
		sampler_builder& minLod(float minLod_);
		sampler_builder& maxLod(float maxLod_);
		sampler_builder& borderColor(VkBorderColor borderColor_);
		sampler_builder& unnormalizedCoordinates(VkBool32 unnormalizedCoordinates_);
		sampler_builder& set_defaults();
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

	VkCommandPoolCreateInfo createCommandPoolInfo(uint32_t queueFamiliy, VkCommandPoolCreateFlags flags);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count, bool secondary = false);
}

