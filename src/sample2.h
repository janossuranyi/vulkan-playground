#pragma once

#include "vkjs/vkjs.h"
#include "vkjs/vk_descriptors.h"
#include "vkjs/VulkanInitializers.hpp"
#include "vkjs/appbase.h"
#include "vkjs/pipeline.h"
#include "glm/glm.hpp"
#include "imgui.h"

class Sample2App : public jvk::AppBase
{
private:
	glm::vec4 hdrColor = { 0.0f,0.0f,0.0f,1.0f };
	VkRenderPass pass = {};
	VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};
public:
	Sample2App(bool b) : AppBase(b) {
		depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		settings.validation = b;

	}

	virtual void on_update_gui() override;
	virtual ~Sample2App();
	virtual void prepare() override;

	virtual void build_command_buffers() override;

	virtual void render() override;
	virtual void get_enabled_extensions() override;
};
