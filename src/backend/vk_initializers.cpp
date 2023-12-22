#include "vk_initializers.h"


VkSamplerCreateInfo vkinitializer::sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode)
{
	VkSamplerCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = nullptr;

	info.magFilter = filters;
	info.minFilter = filters;
	info.addressModeU = samplerAddressMode;
	info.addressModeV = samplerAddressMode;
	info.addressModeW = samplerAddressMode;

	return info;
}

VkWriteDescriptorSet vkinitializer::write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding, uint32_t arrayIndex)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding = binding;
	write.dstSet = dstSet;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = imageInfo;
	write.dstArrayElement = arrayIndex;

	return write;
}

VkCommandBufferBeginInfo vkinitializer::command_buffer_begin_info(VkCommandBufferUsageFlags flags /*= 0*/)
{
	VkCommandBufferBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.pNext = nullptr;

	info.pInheritanceInfo = nullptr;
	info.flags = flags;
	return info;
}

VkSubmitInfo vkinitializer::submit_info(VkCommandBuffer* cmd)
{
	VkSubmitInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.pNext = nullptr;

	info.waitSemaphoreCount = 0;
	info.pWaitSemaphores = nullptr;
	info.pWaitDstStageMask = nullptr;
	info.commandBufferCount = 1;
	info.pCommandBuffers = cmd;
	info.signalSemaphoreCount = 0;
	info.pSignalSemaphores = nullptr;

	return info;
}

VkWriteDescriptorSet vkinitializer::write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding)
{
	VkWriteDescriptorSet write {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstBinding = binding;
	write.dstSet = dstSet;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = bufferInfo;

	return write;
}

VkDescriptorSetLayoutBinding vkinitializer::descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding)
{
	VkDescriptorSetLayoutBinding setbind {};
	setbind.binding = binding;
	setbind.descriptorCount = 1;
	setbind.descriptorType = type;
	setbind.pImmutableSamplers = nullptr;
	setbind.stageFlags = stageFlags;

	return setbind;
}

VkCommandPoolCreateInfo vkinitializer::command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo info {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;

	info.queueFamilyIndex = queueFamilyIndex;
	info.flags = flags;
	return info;
}

VkCommandBufferAllocateInfo vkinitializer::command_buffer_allocate_info(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level)
{
	VkCommandBufferAllocateInfo info {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;

	info.commandPool = pool;
	info.commandBufferCount = count;
	info.level = level;
	return info;
}

VkPipelineShaderStageCreateInfo vkinitializer::pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
{
	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;

	//shader stage
	info.stage = stage;
	//module containing the code for this shader stage
	info.module = shaderModule;
	//the entry point of the shader
	info.pName = "main";
	return info;
}

VkPipelineVertexInputStateCreateInfo vkinitializer::vertex_input_state_create_info()
{
	VkPipelineVertexInputStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	info.pNext = nullptr;

	//no vertex bindings or attributes
	info.vertexBindingDescriptionCount = 0;
	info.vertexAttributeDescriptionCount = 0;
	return info;
}

VkPipelineInputAssemblyStateCreateInfo vkinitializer::input_assembly_create_info(VkPrimitiveTopology topology) {
	VkPipelineInputAssemblyStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.topology = topology;
	//we are not going to use primitive restart on the entire tutorial so leave it on false
	info.primitiveRestartEnable = VK_FALSE;
	return info;
}

VkPipelineRasterizationStateCreateInfo vkinitializer::rasterization_state_create_info(VkPolygonMode polygonMode)
{
	VkPipelineRasterizationStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.depthClampEnable = VK_FALSE;
	//discards all primitives before the rasterization stage if enabled which we don't want
	info.rasterizerDiscardEnable = VK_FALSE;

	info.polygonMode = polygonMode;
	info.lineWidth = 1.0f;
	//no backface cull
	info.cullMode = VK_CULL_MODE_NONE;
	info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	//no depth bias
	info.depthBiasEnable = VK_FALSE;
	info.depthBiasConstantFactor = 0.0f;
	info.depthBiasClamp = 0.0f;
	info.depthBiasSlopeFactor = 0.0f;

	return info;
}

VkPipelineMultisampleStateCreateInfo vkinitializer::multisampling_state_create_info()
{
	VkPipelineMultisampleStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.sampleShadingEnable = VK_FALSE;
	//multisampling defaulted to no multisampling (1 sample per pixel)
	info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	info.minSampleShading = 1.0f;
	info.pSampleMask = nullptr;
	info.alphaToCoverageEnable = VK_FALSE;
	info.alphaToOneEnable = VK_FALSE;
	return info;
}

VkPipelineColorBlendAttachmentState vkinitializer::color_blend_attachment_state()
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	return colorBlendAttachment;
}

VkPipelineLayoutCreateInfo vkinitializer::pipeline_layout_create_info()
{
	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;

	//empty defaults
	info.flags = 0;
	info.setLayoutCount = 0;
	info.pSetLayouts = nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;
	return info;
}

//implementation
VkFenceCreateInfo vkinitializer::fence_create_info(VkFenceCreateFlags flags)
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = flags;
	return fenceCreateInfo;
}

VkSemaphoreCreateInfo vkinitializer::semaphore_create_info(VkSemaphoreCreateFlags flags)
{
	VkSemaphoreCreateInfo semCreateInfo = {};
	semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semCreateInfo.pNext = nullptr;
	semCreateInfo.flags = flags;
	return semCreateInfo;
}

VkImageCreateInfo vkinitializer::image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent)
{
	VkImageCreateInfo info = { };
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.imageType = VK_IMAGE_TYPE_2D;

	info.format = format;
	info.extent = extent;

	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usageFlags;

	return info;
}

VkImageViewCreateInfo vkinitializer::imageview_create_info(VkFormat format, VkImage image, uint32_t mipCount, VkImageAspectFlags aspectFlags)
{
	//build a image-view for the depth image to use for rendering
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;

	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = mipCount;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspectFlags;

	return info;
}

VkPipelineDepthStencilStateCreateInfo vkinitializer::depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp)
{
	VkPipelineDepthStencilStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
	info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
	info.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
	info.depthBoundsTestEnable = VK_FALSE;
	info.minDepthBounds = 0.0f; // Optional
	info.maxDepthBounds = 1.0f; // Optional
	info.stencilTestEnable = VK_FALSE;

	return info;
}

namespace vkinitializer {
	sampler_builder::sampler_builder()
	{
		set_defaults();
	}
	sampler_builder& sampler_builder::next(const void* pNext)
	{
		m_create_info.pNext = pNext;
		return *this;
	}

	sampler_builder& sampler_builder::flags(VkSamplerCreateFlags flags_)
	{
		m_create_info.flags = flags_;
		return *this;
	}

	sampler_builder& sampler_builder::magFilter(VkFilter magFilter_)
	{
		m_create_info.magFilter = magFilter_;
		return *this;
	}

	sampler_builder& sampler_builder::minFilter(VkFilter minFilter_)
	{
		m_create_info.minFilter = minFilter_;
		return *this;
	}

	sampler_builder& sampler_builder::filter(VkFilter filter_)
	{
		return minFilter(filter_).magFilter(filter_);
	}

	sampler_builder& sampler_builder::mipmapMode(VkSamplerMipmapMode mipmapMode_)
	{
		m_create_info.mipmapMode = mipmapMode_;
		return *this;
	}

	sampler_builder& sampler_builder::addressMode(VkSamplerAddressMode addressMode_)
	{
		m_create_info.addressModeU = addressMode_;
		m_create_info.addressModeV = addressMode_;
		m_create_info.addressModeW = addressMode_;
		return *this;
	}

	sampler_builder& sampler_builder::addressModeU(VkSamplerAddressMode addressModeU_)
	{
		m_create_info.addressModeU = addressModeU_;
		return *this;
	}

	sampler_builder& sampler_builder::addressModeV(VkSamplerAddressMode addressModeV_)
	{
		m_create_info.addressModeV = addressModeV_;
		return *this;
	}

	sampler_builder& sampler_builder::addressModeW(VkSamplerAddressMode addressModeW_)
	{
		m_create_info.addressModeW = addressModeW_;
		return *this;
	}

	sampler_builder& sampler_builder::mipLodBias(float mipLodBias_)
	{
		m_create_info.mipLodBias = mipLodBias_;
		return *this;
	}

	sampler_builder& sampler_builder::anisotropyEnable(VkBool32 anisotropyEnable_)
	{
		m_create_info.anisotropyEnable = anisotropyEnable_;
		return *this;
	}

	sampler_builder& sampler_builder::maxAnisotropy(float maxAnisotropy_)
	{
		m_create_info.maxAnisotropy = maxAnisotropy_;
		return *this;
	}

	sampler_builder& sampler_builder::compareEnable(VkBool32 compareEnable_)
	{
		m_create_info.compareEnable = compareEnable_;
		return *this;
	}

	sampler_builder& sampler_builder::compareOp(VkCompareOp compareOp_)
	{
		m_create_info.compareOp = compareOp_;
		return *this;
	}

	sampler_builder& sampler_builder::minLod(float minLod_)
	{
		m_create_info.minLod = minLod_;
		return *this;
	}

	sampler_builder& sampler_builder::maxLod(float maxLod_)
	{
		m_create_info.maxLod = maxLod_;
		return *this;
	}

	sampler_builder& sampler_builder::borderColor(VkBorderColor borderColor_)
	{
		m_create_info.borderColor = borderColor_;
		return *this;
	}

	sampler_builder& sampler_builder::unnormalizedCoordinates(VkBool32 unnormalizedCoordinates_)
	{
		m_create_info.unnormalizedCoordinates = unnormalizedCoordinates_;
		return *this;
	}

	sampler_builder& sampler_builder::set_defaults()
	{
		m_create_info = {};
		m_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		m_create_info.pNext = nullptr;

		m_create_info.magFilter = VK_FILTER_LINEAR;
		m_create_info.minFilter = VK_FILTER_LINEAR;
		m_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		m_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		m_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		m_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		return *this;
	}

	VkSamplerCreateInfo sampler_builder::build()
	{
		return m_create_info;
	}

	VkCommandPoolCreateInfo createCommandPoolInfo(uint32_t queueFamiliy, VkCommandPoolCreateFlags flags)
	{
		VkCommandPoolCreateInfo commandPoolInfo = {};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.pNext = nullptr;

		//the command pool will be one that can submit graphics commands
		commandPoolInfo.queueFamilyIndex = queueFamiliy;
		//we also want the pool to allow for resetting of individual command buffers
		commandPoolInfo.flags = flags;

		return commandPoolInfo;
	}

	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count, bool secondary)
	{
		VkCommandBufferAllocateInfo cmdAllocInfo = {};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.pNext = nullptr;

		//commands will be made from our _commandPool
		cmdAllocInfo.commandPool = pool;
		//we will allocate 1 command buffer
		cmdAllocInfo.commandBufferCount = count;
		// command level is Primary
		cmdAllocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		return cmdAllocInfo;
	}

}
