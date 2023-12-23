#include <iostream>
#include <fstream>
#include <functional>
#include <mutex>
#include <chrono>
#include <thread>
#include <cstdio>
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <random>

#include "demo.h"
#include "renderer/Vertex.h"
#include "renderer/Window.h"
#include "renderer/VulkanDevice.h"
#include "renderer/VulkanSwapchain.h"
#include "renderer/VulkanRenderer.h"
#include "renderer/VulkanImage.h"
#include "renderer/ResourceHandles.h"
#include "renderer/ResourceDescriptions.h"
#include "renderer/pipelines/DepthPrePass.h"
#include "renderer/pipelines/ForwardLightingPass.h"
#include "renderer/pipelines/CompImageGen.h"
#include "renderer/RenderFrontend.h"
#include "renderer/Camera.h"
#include "renderer/Frustum.h"
#include "jsrlib/jsr_logger.h"
#include "jsrlib/jsr_semaphore.h"
//#include "jsrlib/jsr_jobsystem2.h"
#include "jobsys.h"
#include "world.h"
#include "gltf_loader.h"
#include "renderer/ConstPool.h"
#include <thread>

#include "renderer/gli_utils.h"

namespace fs = std::filesystem;
using namespace glm;
using namespace jsr;

#define asfloat(x) static_cast<float>(x)
#define asuint(x) static_cast<uint32_t>(x)

void setup_lights(const Bounds& worldBounds, std::vector<Light>& lights) {
    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_real_distribution<float> distr_x(worldBounds.min().x, worldBounds.max().x);
    std::uniform_real_distribution<float> distr_y(worldBounds.min().y, worldBounds.max().y);
    std::uniform_real_distribution<float> distr_z(worldBounds.min().z, worldBounds.max().z);
    std::uniform_real_distribution<float> distr_c(0.1f, c_one);

    for (size_t k = 0; k < lights.size(); ++k)
    {
        lights[k].type = LightType_Point;
        lights[k].range = 5.0f;
        lights[k].intensity = 5.0f;
        lights[k].color = clamp(vec3(distr_c(gen), distr_c(gen), distr_c(gen)), c_zero, c_one);
        lights[k].position = vec3(distr_x(gen), distr_y(gen), distr_z(gen));
    }

}

#include "vkjs/vkjs.h"
#include "vkjs/appbase.h"
#include "vkjs/VulkanInitializers.hpp"

class App : public vkjs::AppBase {
private:
    VkDevice d;

    struct RenderPass {
        VkRenderPass pass;
        VkPipelineLayout layout;
        VkPipeline pipeline;
    };
    VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};
    VkFramebufferCreateInfo fbci;
    World* scene;
    struct {
        RenderPass tonemap = {};
    } passes;

public:


    App(bool b) : AppBase(b) {
    }

    virtual ~App() {
        vkDeviceWaitIdle(d);
        vkDestroyRenderPass(d, passes.tonemap.pass, nullptr);
        for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
        {
            vkDestroyFramebuffer(d, fb[i], 0);
        }
        delete scene;
    }

    virtual void render() override {        

        
        if (fb[current_frame] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(d, fb[current_frame], 0);
        }

        fbci.pAttachments = &swapchain.views[current_buffer];
        fbci.width = width;
        fbci.height = height;
        VK_CHECK(vkCreateFramebuffer(d, &fbci, 0, &fb[current_frame]));

        VkRenderPassBeginInfo beginPass = vks::initializers::renderPassBeginInfo();
        VkClearValue clearVal;
        clearVal.color = { 0.2f,0.0f,0.2f,1.0f };
        beginPass.clearValueCount = 1;
        beginPass.pClearValues = &clearVal;
        beginPass.renderPass = passes.tonemap.pass;
        beginPass.framebuffer = fb[current_frame];
        beginPass.renderArea.extent = swapchain.vkb_swapchain.extent;
        beginPass.renderArea.offset = { 0,0 };

        VkCommandBuffer cmd = draw_cmd_buffers[current_frame];
        vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(cmd);

    }

    void init_tonemap_pass() {
        VkRenderPassCreateInfo rpci = vks::initializers::renderPassCreateInfo();

        VkAttachmentDescription color = {};
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.format = swapchain.vkb_swapchain.image_format;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentReference colorRef = {};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass0 = {};
        subpass0.colorAttachmentCount = 1;
        subpass0.pColorAttachments = &colorRef;
        subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        
        VkSubpassDependency dep0 = {};
        dep0.dependencyFlags = 0;
        dep0.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dep0.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep0.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep0.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep0.dstSubpass = 0;

        rpci.attachmentCount = 1;
        rpci.pAttachments = &color;
        rpci.flags = 0;
        rpci.dependencyCount = 1;
        rpci.pDependencies = &dep0;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass0;

        VK_CHECK(vkCreateRenderPass(*device_wrapper, &rpci, nullptr, &passes.tonemap.pass));
    }

    virtual void setup_render_pass() override {
        AppBase::setup_render_pass();
        init_tonemap_pass();
    }
    virtual void prepare() override {
        vkjs::AppBase::prepare();

        d = *device_wrapper;

        fbci = vks::initializers::framebufferCreateInfo();
        fbci.attachmentCount = 1;
        fbci.renderPass = passes.tonemap.pass;
        fbci.layers = 1;

        scene = new World();
        jsr::gltfLoadWorld(fs::path("../../resources/models/sponza/sponza_j.gltf"), *scene);

        prepared = true;
    }

    virtual void get_enabled_features() override {
        enabled_features.samplerAnisotropy = VK_TRUE;
        enabled_features.fragmentStoresAndAtomics = VK_TRUE;
        enabled_features.fillModeNonSolid = VK_TRUE;
        enabled_features.depthClamp = VK_TRUE;
        enabled_features.geometryShader = VK_TRUE;
        enabled_features.textureCompressionBC = VK_TRUE;
        enabled_features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

        enabled_features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        enabled_features12.pNext = nullptr;
        enabled_features12.hostQueryReset = VK_TRUE;
        enabled_features12.runtimeDescriptorArray = VK_TRUE;
        enabled_features12.descriptorBindingPartiallyBound = VK_TRUE;
        enabled_features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
        enabled_features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        enabled_features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        enabled_features12.shaderFloat16 = VK_TRUE;
        enabled_features12.descriptorIndexing = VK_TRUE;
    }

    virtual void get_enabled_extensions() override {
        enabled_device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
        enabled_device_extensions.push_back(VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME);
        enabled_device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        enabled_device_extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    }
};

void demo()
{
    jsrlib::gLogWriter.SetFileName("vulkan_engine.log");
    App* app = new App(true);
    app->init();
    app->prepare();
    app->run();

    delete app;    
}

int main(int argc, char* argv[])
{
    demo();

    return 0;
}