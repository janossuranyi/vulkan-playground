#pragma once

#include "vkjs/vkjs.h"
#include "vkjs/vk_descriptors.h"
#include "vkjs/VulkanInitializers.hpp"
#include "vkjs/appbase.h"
#include "vkjs/pipeline.h"
#include "vkjs/vk_descriptors.h"

#include "glm/glm.hpp"
#include "imgui.h"
#include <memory>
#include <filesystem>

#include "nvrhi/nvrhi.h"
#include "nvrhi/vulkan.h"

class Sample2App : public jvk::AppBase
{
private:
	nvrhi::DeviceHandle m_nvrhiDevice;
	std::vector<nvrhi::TextureHandle> m_swapchainImages;
	std::vector<nvrhi::FramebufferHandle> m_fbs;
	nvrhi::ShaderHandle m_vertexShader;
	nvrhi::ShaderHandle m_fragmentShader;
	nvrhi::GraphicsPipelineHandle m_graphicsPipeline;
	nvrhi::BufferHandle m_constantBuffer;
	nvrhi::CommandListHandle m_commandList;
	nvrhi::BindingSetHandle m_bindingSet;
	nvrhi::TextureHandle m_tex0;

	nvrhi::BindingLayoutHandle m_bindingLayout = {};

	glm::vec4 hdrColor = { 0.0f,0.0f,0.0f,1.0f };

	struct globals_t {
		glm::vec4 rpColorMultiplier = glm::vec4(1.0f);
		glm::vec4 rpColorBias = glm::vec4(0.0f);
		glm::vec4 rpScale = glm::vec4(1.0f);
	} globals;

	void init_pipelines();
	void create_framebuffers();
	void init_images();

	std::filesystem::path basePath;
	VkDescriptorSet ubo_set{};

	struct FragPushConstants {
		glm::vec4 data;
	} pc{};

public:
	Sample2App(bool b) : AppBase(b) {
		depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		settings.validation = b;

	}

	virtual void on_window_resized() override;
	virtual void on_update_gui() override;
	virtual ~Sample2App();
	virtual void prepare() override;

	virtual void build_command_buffers() override;

	virtual void render() override;
	virtual void get_enabled_extensions() override;
	virtual void get_enabled_features() override;
};
