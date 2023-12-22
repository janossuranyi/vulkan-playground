#include "renderer/VulkanTechnic.h"
#include "renderer/FrameCounter.h"

namespace jsr {
	VulkanTechnic::VulkanTechnic() :
		m_device(),
		m_engine(),
		m_descriptorPool(),
		m_perFrameSets(),
		m_perFrameLayout(),
		m_renderPass(),
		m_pipeline(),
		m_pipelineLayout(),
		m_perObjectLayout()
	{
	}
	VulkanTechnic::VulkanTechnic(VulkanDevice* device, VulkanRenderer* renderer, VkDescriptorPool pool) :
		m_device(device),
		m_engine(renderer),
		m_descriptorPool(pool),
		m_perFrameSets(),
		m_perFrameLayout(),
		m_perObjectLayout(),
		m_renderPass(),
		m_pipeline(),
		m_pipelineLayout()
	{
	}
	VkPipeline VulkanTechnic::getPipeline() const
	{
		return m_pipeline;
	}
	void VulkanTechnic::setDevice(VulkanDevice* device)
	{
		this->m_device = device;
	}
	void VulkanTechnic::setEngine(VulkanRenderer* renderer)
	{
		m_engine = renderer;
	}
	void VulkanTechnic::setDescriptorPool(VkDescriptorPool pool)
	{
		m_descriptorPool = pool;
	}
	VkPipelineLayout VulkanTechnic::getPipelineLayout() const
	{
		return m_pipelineLayout;
	}
	VkDescriptorSetLayout VulkanTechnic::getDescriptorSetLayout() const
	{
		return m_perFrameLayout;
	}
	VkDescriptorSetLayout VulkanTechnic::getPerObjectLayout() const
	{
		return m_perObjectLayout;
	}
	VkDescriptorSet VulkanTechnic::getDescriptorSet(uint32_t index) const
	{
		return m_perFrameSets[index % FrameCounter::INFLIGHT_FRAMES];
	}
	VkRenderPass VulkanTechnic::getRenderPass() const
	{
		return m_renderPass;
	}
	VkResult VulkanTechnic::createPipeline(const GraphichPipelineShaderBinary& shaders)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	VkResult VulkanTechnic::createPipeline(const ComputePipelineShaderBinary& shaders)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	VulkanTechnic::~VulkanTechnic()
	{
		shutdown();
	}
	void VulkanTechnic::shutdown()
	{
		if (m_pipeline) {
			vkDestroyPipeline(m_device->vkDevice, m_pipeline, nullptr);
		}

		if (m_pipelineLayout) {
			vkDestroyPipelineLayout(m_device->vkDevice, m_pipelineLayout, nullptr);
		}
#if 0
		if (m_perFrameSets[0]) {
			vkFreeDescriptorSets(m_device->vkDevice, m_descriptorPool, 1, &m_perFrameSets[0]);
			vkFreeDescriptorSets(m_device->vkDevice, m_descriptorPool, 1, &m_perFrameSets[1]);
		}
#endif
		if (m_perFrameLayout) {
			vkDestroyDescriptorSetLayout(m_device->vkDevice, m_perFrameLayout, nullptr);
		}
		if (m_perObjectLayout) {
			vkDestroyDescriptorSetLayout(m_device->vkDevice, m_perObjectLayout, nullptr);
		}

		if (m_renderPass) {
			vkDestroyRenderPass(m_device->vkDevice, m_renderPass, nullptr);
		}
	}
}