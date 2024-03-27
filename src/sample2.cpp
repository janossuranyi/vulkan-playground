#include "sample2.h"
#include "vkjs/vkcheck.h"
#include "vkjs/pipeline.h"
#include "vkjs/shader_module.h"
#include "glm/glm.hpp"
#include <filesystem>

namespace fs = std::filesystem;

/* Fd is the displayed luminance in cd/m2 */
float PQinverseEOTF(float Fd)
{
	const float Y = Fd / 10000.0f;
	const float m1 = pow(Y, 0.1593017578125f);
	float res = (0.8359375f + (18.8515625f * m1)) / (1.0f + (18.6875f * m1));
	res = pow(res, 78.84375f);
	return res;
}

glm::vec3 PQinverseEOTF(glm::vec3 c)
{
	return glm::vec3(
		PQinverseEOTF(c.x),
		PQinverseEOTF(c.y),
		PQinverseEOTF(c.z));
}

void Sample2App::init_pipelines()
{
	jvk::ShaderModule vert_module(*pDevice);
	jvk::ShaderModule frag_module(*pDevice);
	const fs::path vert_spirv_filename = basePath / "shaders/bin/debug.vert.spv";
	const fs::path frag_spirv_filename = basePath / "shaders/bin/debug.frag.spv";
	VK_CHECK(vert_module.create(vert_spirv_filename));
	VK_CHECK(frag_module.create(frag_spirv_filename));
	jvk::GraphicsShaderInfo shaders{};
	shaders.vert = &vert_module;
	shaders.frag = &frag_module;

	struct spec_t {
		float VSCALE = -1.0f;
		int PARM_COUNT = 8;
	} specData;

	int specIdx = 0;
	VkSpecializationMapEntry specent[2];
	specent[specIdx].constantID = 1;
	specent[specIdx].offset = offsetof(spec_t, VSCALE);
	specent[specIdx].size = sizeof(specData.VSCALE);
	++specIdx;
	specent[specIdx].constantID = 2;
	specent[specIdx].offset = offsetof(spec_t, PARM_COUNT);
	specent[specIdx].size = sizeof(specData.PARM_COUNT);

	VkSpecializationInfo specinf = {};
	specinf.dataSize = sizeof(specData);
	specinf.mapEntryCount = 2;
	specinf.pMapEntries = &specent[0];
	specinf.pData = &specData;

	pipeline.reset(new jvk::GraphicsPipeline(pDevice, shaders, descriptorMgr.get()));

	pipeline->set_specialization_info(VK_SHADER_STAGE_VERTEX_BIT, &specinf);
	pipeline->set_cull_mode(VK_CULL_MODE_NONE)
		.set_depth_func(VK_COMPARE_OP_ALWAYS)
		.set_depth_mask(false)
		.set_depth_test(false)
		.set_draw_mode(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.set_sample_count(VK_SAMPLE_COUNT_1_BIT)
		.set_polygon_mode(VK_POLYGON_MODE_FILL)
		.set_render_pass(pass)
		.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
		.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
		.add_attachment_blend_state(vks::initializers::pipelineColorBlendAttachmentState(0xf,false))
		.set_name("debug");

	VK_CHECK(pipeline->build_pipeline());
}

void Sample2App::on_update_gui()
{
	if (settings.overlay == false) return;
	ImGui::DragFloat3("ClearColor", &hdrColor[0], 1.0f/1024.0f, 0.0f, 1.0f);
}

Sample2App::~Sample2App()
{
	vkDeviceWaitIdle(*pDevice);
	for (size_t i(0); i < swapchain.images.size(); ++i)
	{
		if (fb[i]) vkDestroyFramebuffer(*pDevice, fb[i], nullptr);
	}

	if (pass) {
		vkDestroyRenderPass(*pDevice, pass, nullptr);
	}
}

void Sample2App::create_framebuffers()
{
	if (fb) {
		for (size_t i(0); i < swapchain.images.size(); ++i)
		{
			if (fb[i]) {
				vkDestroyFramebuffer(*pDevice, fb[i], nullptr);
				fb[i] = VK_NULL_HANDLE;
			}
		}
	}
	
	fb = std::make_unique<VkFramebuffer[]>(swapchain.images.size());
	
	auto fbci = vks::initializers::framebufferCreateInfo();
	fbci.renderPass = pass;
	fbci.layers = 1;
	fbci.attachmentCount = 1;
	fbci.width = width;
	fbci.height = height;
	auto& views = swapchain.views;

	for (int i(0); i < views.size(); ++i) {
		fbci.pAttachments = &views[i];
		VK_CHECK(vkCreateFramebuffer(*pDevice, &fbci, 0, &fb[i]));
	}
}
void Sample2App::prepare()
{
	jvk::AppBase::prepare();

	basePath = fs::path("../..");

	VkAttachmentDescription color = {};
	color.format = swapchain.vkb_swapchain.image_format;
	color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color.samples = VK_SAMPLE_COUNT_1_BIT;

	VkAttachmentReference colorRef = {};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDependency colorDep = {};
	colorDep.srcSubpass = VK_SUBPASS_EXTERNAL;
	colorDep.dstSubpass = 0;
	colorDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	colorDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	colorDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubpassDescription subpass0 = {};
	subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass0.colorAttachmentCount = 1;
	subpass0.pColorAttachments = &colorRef;

	VkRenderPassCreateInfo ci = vks::initializers::renderPassCreateInfo();
	ci.attachmentCount = 1;
	ci.dependencyCount = 1;
	ci.pAttachments = &color;
	ci.subpassCount = 1;
	ci.pSubpasses = &subpass0;
	ci.pDependencies = &colorDep;

	VK_CHECK(vkCreateRenderPass(*pDevice, &ci, nullptr, &pass));
	create_framebuffers();

	descriptorMgr.reset(new vkutil::DescriptorManager());
	descriptorMgr->init(pDevice);

	init_pipelines();

	pDevice->create_uniform_buffer(sizeof(ubo_t), false, &ubo);
	parms.parms[0] = { 1.0f,0.0f,0.0f,1.0f };
	parms.parms[1] = { 0.0f,1.0f,0.0f,1.0f };
	parms.parms[2] = { 1.0f,1.0f,1.0f,1.0f };
	ubo.map();
	ubo.copyTo(0, sizeof(ubo_t), &parms);

	descriptorMgr->builder().
		bind_buffer(0, &ubo.descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
		.build(ubo_set);

	prepared = true;
}

void Sample2App::build_command_buffers()
{
	VkCommandBuffer cmd = drawCmdBuffers[currentFrame];
	VkRenderPassBeginInfo beginPass = vks::initializers::renderPassBeginInfo();
	VkClearValue clearVal;

	VkRect2D scissor = get_scissor();
	VkViewport viewport = get_viewport();
	
	glm::vec3 hdr10 = glm::pow(hdrColor,glm::vec4(1.0f/2.2f));// PQinverseEOTF(hdrColor);

	clearVal.color = { hdr10.r, hdr10.g, hdr10.b, 1.0f };
	beginPass.clearValueCount = 1;
	beginPass.pClearValues = &clearVal;
	beginPass.renderPass = pass;
	beginPass.framebuffer = fb[currentBuffer];
	beginPass.renderArea.extent = swapchain.vkb_swapchain.extent;
	beginPass.renderArea.offset = { 0,0 };
	vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);
	pipeline->bind(cmd);
	pipeline->bind_descriptor_sets(cmd, 1, &ubo_set);

	vkCmdDraw(cmd, 3, 1, 0, 0);

	vkCmdEndRenderPass(cmd);

}

void Sample2App::render()
{
	build_command_buffers();
	//swapchain_images[currentBuffer].layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

}

void Sample2App::get_enabled_extensions()
{
	enabled_instance_extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
}

void Sample2App::on_window_resized()
{
	//pDevice->wait_idle();
	create_framebuffers();
}
