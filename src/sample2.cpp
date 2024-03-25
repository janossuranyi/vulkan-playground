#include "sample2.h"
#include "vkjs/vkcheck.h"
#include "glm/glm.hpp"
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

}

void Sample2App::on_update_gui()
{
	if (settings.overlay == false) return;
	ImGui::DragFloat3("ClearColor", &hdrColor[0], 1.0f/256.0f, 0.0f, 1.0f);
}

Sample2App::~Sample2App()
{
	vkDeviceWaitIdle(*pDevice);
	for (size_t i(0); i < fb.size(); ++i)
	{
		if (fb[i]) vkDestroyFramebuffer(*pDevice, fb[i], nullptr);
	}

	if (pass) {
		vkDestroyRenderPass(*pDevice, pass, nullptr);
	}
}

void Sample2App::prepare()
{
	jvk::AppBase::prepare();
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

	auto fbci = vks::initializers::framebufferCreateInfo();
	fbci.renderPass = pass;
	fbci.layers = 1;
	fbci.attachmentCount = 1;
	fbci.width = width;
	fbci.height = height;
	auto& views = swapchain.views;
	fb.resize(views.size());

	for (int i(0); i < views.size(); ++i) {
		fbci.pAttachments = &views[i];
		VK_CHECK(vkCreateFramebuffer(*pDevice, &fbci, 0, &fb[i]));
	}

	prepared = true;
}

void Sample2App::build_command_buffers()
{
	VkCommandBuffer cmd = drawCmdBuffers[currentFrame];
	VkRenderPassBeginInfo beginPass = vks::initializers::renderPassBeginInfo();
	VkClearValue clearVal;

	glm::vec3 hdr10 = glm::pow(hdrColor,glm::vec4(1.0f/2.2f));// PQinverseEOTF(hdrColor);

	clearVal.color = { hdr10.r, hdr10.g, hdr10.b, 1.0f };
	beginPass.clearValueCount = 1;
	beginPass.pClearValues = &clearVal;
	beginPass.renderPass = pass;
	beginPass.framebuffer = fb[currentBuffer];
	beginPass.renderArea.extent = swapchain.vkb_swapchain.extent;
	beginPass.renderArea.offset = { 0,0 };
	vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);
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
