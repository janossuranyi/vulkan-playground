#include "TestCompTechnic.h"
#include "renderer/VulkanRenderer.h"
#include "renderer/Vertex.h"
#include "jsrlib/jsr_resources.h"
#include "jsrlib/jsr_logger.h"

namespace jsr {
    std::vector<VkClearValue> TestCompTechnic::getClearValues() const
    {
        return std::vector<VkClearValue>();
    }

    VkResult TestCompTechnic::createPipeline(const ComputePipelineShaderBinary& shaders)
    {
        if (!m_device || !m_engine) {
            return VK_ERROR_UNKNOWN;
        }

        std::array<VkDescriptorSetLayoutBinding, 1> data_binding = {
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        };

        VkDescriptorSetLayoutCreateInfo data_layout = {};
        data_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        data_layout.bindingCount = static_cast<uint32_t>(data_binding.size());
        data_layout.pBindings = data_binding.data();
        VK_CHECK(vkCreateDescriptorSetLayout(m_device->vkDevice, &data_layout, nullptr, &m_perFrameLayout));

        /* pipeline */
        const VkDescriptorSetLayout layouts[] = { m_engine->getGlobalVarsLayout(), m_engine->getGlobalTexturesLayout(), m_perFrameLayout };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
        pipelineLayoutInfo.setLayoutCount = 3;
        pipelineLayoutInfo.pSetLayouts = layouts;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkResult vk_result = vkCreatePipelineLayout(m_device->vkDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
        VK_CHECK(vk_result);
        auto module_comp = shaders.computeSPIRV;
        VkShaderModuleCreateInfo shaderModuleCI = {};
        VkShaderModule shaderModule;
        shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCI.codeSize = module_comp.size();
        shaderModuleCI.pCode = (uint32_t*)module_comp.data();
        VK_CHECK(vkCreateShaderModule(*m_device, &shaderModuleCI, nullptr, &shaderModule));

        VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.stage = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, shaderModule);
        pipelineInfo.flags = 0;
        VK_CHECK(vkCreateComputePipelines(*m_device, nullptr, 1, &pipelineInfo, nullptr, &m_pipeline));

        vkDestroyShaderModule(*m_device, shaderModule, nullptr);
        
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_perFrameLayout;

        for (uint32_t i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i) {
            VK_CHECK(vkAllocateDescriptorSets(m_device->vkDevice, &allocInfo, &m_perFrameSets[i]));
        }

        m_perObjectLayout = VK_NULL_HANDLE;
        jsrlib::Info("TestCompTechnic: VkPipeline created %p", m_pipeline);

        return VK_SUCCESS;
    }


}