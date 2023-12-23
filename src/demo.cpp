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
#include "jsrlib/jsr_logger.h"
#include "jsrlib/jsr_semaphore.h"
#include "jsrlib/jsr_resources.h"
//#include "jsrlib/jsr_jobsystem2.h"
#include "jobsys.h"
#include "world.h"
#include "gltf_loader.h"
#include <thread>

#include "vkjs/vkjs.h"
#include "vkjs/appbase.h"
#include "vkjs/VulkanInitializers.hpp"
#include "vkjs/pipeline.h"
#include "vkjs/vkcheck.h"
#include "vkjs/vertex.h"

namespace fs = std::filesystem;
using namespace glm;
using namespace jsr;

#define asfloat(x) static_cast<float>(x)
#define asuint(x) static_cast<uint32_t>(x)

class App : public vkjs::AppBase {
private:
    VkDevice d;

    struct RenderPass {
        VkRenderPass pass;
        VkPipelineLayout layout;
        VkPipeline pipeline;
        std::vector<VkDescriptorSetLayout> ds_layouts;
    };
    VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};
    VkFramebufferCreateInfo fbci;
    fs::path base_path = fs::path("../..");

    World* scene;
    struct {
        RenderPass tonemap = {};
        RenderPass preZ = {};
        RenderPass triangle = {};
    } passes;

public:


    App(bool b) : AppBase(b) {
        depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    }

    virtual ~App() {
        vkDeviceWaitIdle(d);

        std::array<RenderPass*, 3> rpasses = { &passes.preZ,&passes.tonemap,&passes.triangle };
        for (const auto* pass : rpasses) {
            if (pass->pipeline) vkDestroyPipeline(d, pass->pipeline, nullptr);
            if (pass->layout) vkDestroyPipelineLayout(d, pass->layout, nullptr);
            for (auto& dset : pass->ds_layouts) {
                vkDestroyDescriptorSetLayout(d, dset, nullptr);
            }
        }
        vkDestroyRenderPass(d, passes.tonemap.pass, nullptr);
        vkDestroyRenderPass(d, passes.preZ.pass, nullptr);
        vkDestroyRenderPass(d, passes.triangle.pass, nullptr);

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

    void setup_preZ_pipeline() {
        auto vertexInput = vkjs::Vertex::vertex_input_description_position_only();

        const std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };
        vkjs::PipelineBuilder pb = {};
        pb._rasterizer = vks::initializers::pipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        pb._depthStencil = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        pb._dynamicStates = vks::initializers::pipelineDynamicStateCreateInfo(dynStates);
        pb._inputAssembly = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        pb._multisampling = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        pb._vertexInputInfo = vks::initializers::pipelineVertexInputStateCreateInfo(
            vertexInput.bindings,
            vertexInput.attributes);
        
        VkShaderModule vert_module;
        VkShaderModule frag_module;
        fs::path vert_spirv_filename = base_path / "shaders/depthpass_v2.vert.spv";
        fs::path frag_spirv_filename = base_path / "shaders/depthpass_v2.frag.spv";

        const std::vector<uint8_t> vert_spirv = jsrlib::Filesystem::root.ReadFile(vert_spirv_filename.u8string());
        const std::vector<uint8_t> frag_spirv = jsrlib::Filesystem::root.ReadFile(frag_spirv_filename.u8string());
        VkShaderModuleCreateInfo smci = {};
        smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = vert_spirv.size();
        smci.pCode = (uint32_t*)vert_spirv.data();
        VK_CHECK(vkCreateShaderModule(d, &smci, nullptr, &vert_module));
        smci.codeSize = frag_spirv.size();
        smci.pCode = (uint32_t*)frag_spirv.data();
        VK_CHECK(vkCreateShaderModule(d, &smci, nullptr, &frag_module));

        pb._shaderStages.emplace_back();
        auto& vert_stage = pb._shaderStages.back();
        vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_stage.pName = "main";
        vert_stage.module = vert_module;
        pb._shaderStages.emplace_back();
        auto& frag_stage = pb._shaderStages.back();
        frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_stage.pName = "main";
        frag_stage.module = frag_module;

        /*
        layout:
        set 0
            0: ubo
        set 1
            0: dyn. ubo
        */
        
        std::vector<VkDescriptorSetLayoutBinding> set0bind(1);
        std::vector<VkDescriptorSetLayoutBinding> set1bind(1);

        set0bind[0].binding = 0;
        set0bind[0].descriptorCount = 1;
        set0bind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set0bind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        set1bind[0].binding = 0;
        set1bind[0].descriptorCount = 1;
        set1bind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        set1bind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        const VkDescriptorSetLayoutCreateInfo ds0ci = vks::initializers::descriptorSetLayoutCreateInfo(set0bind);
        const VkDescriptorSetLayoutCreateInfo ds1ci = vks::initializers::descriptorSetLayoutCreateInfo(set1bind);
        std::array<VkDescriptorSetLayout, 2> dsl;
        VK_CHECK(vkCreateDescriptorSetLayout(d, &ds0ci, nullptr, &dsl[0]));
        VK_CHECK(vkCreateDescriptorSetLayout(d, &ds1ci, nullptr, &dsl[1]));
        passes.preZ.ds_layouts.push_back(dsl[0]);
        passes.preZ.ds_layouts.push_back(dsl[1]);
        VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo(2);
        plci.pSetLayouts = dsl.data();        
        VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &passes.preZ.layout));
        pb._pipelineLayout = passes.preZ.layout;
        passes.preZ.pipeline = pb.build_pipeline(d, passes.preZ.pass);
        vkDestroyShaderModule(d, vert_module, nullptr);
        vkDestroyShaderModule(d, frag_module, nullptr);
        assert(passes.preZ.pipeline);

    }

    void setup_triangle_pipeline() {
        auto vertexInput = vkjs::Vertex::vertex_input_description_position_only();

        const std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };
        vkjs::PipelineBuilder pb = {};
        pb._rasterizer = vks::initializers::pipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_LINE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
        pb._depthStencil = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        pb._dynamicStates = vks::initializers::pipelineDynamicStateCreateInfo(dynStates);
        pb._inputAssembly = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        pb._multisampling = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
        pb._vertexInputInfo = vks::initializers::pipelineVertexInputStateCreateInfo(
            vertexInput.bindings,
            vertexInput.attributes);

        VkShaderModule vert_module;
        VkShaderModule frag_module;
        fs::path vert_spirv_filename = base_path / "shaders/triangle_v2.vert.spv";
        fs::path frag_spirv_filename = base_path / "shaders/triangle_v2.frag.spv";

        const std::vector<uint8_t> vert_spirv = jsrlib::Filesystem::root.ReadFile(vert_spirv_filename.u8string());
        const std::vector<uint8_t> frag_spirv = jsrlib::Filesystem::root.ReadFile(frag_spirv_filename.u8string());
        VkShaderModuleCreateInfo smci = {};
        smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smci.codeSize = vert_spirv.size();
        smci.pCode = (uint32_t*)vert_spirv.data();
        VK_CHECK(vkCreateShaderModule(d, &smci, nullptr, &vert_module));
        smci.codeSize = frag_spirv.size();
        smci.pCode = (uint32_t*)frag_spirv.data();
        VK_CHECK(vkCreateShaderModule(d, &smci, nullptr, &frag_module));

        pb._shaderStages.emplace_back();
        auto& vert_stage = pb._shaderStages.back();
        vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_stage.pName = "main";
        vert_stage.module = vert_module;
        pb._shaderStages.emplace_back();
        auto& frag_stage = pb._shaderStages.back();
        frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_stage.pName = "main";
        frag_stage.module = frag_module;

        /*
        layout:
        set 0
            0: ubo
        set 1
            0: dyn. ubo
        */

        std::vector<VkDescriptorSetLayoutBinding> set0bind(1);
        std::vector<VkDescriptorSetLayoutBinding> set1bind(1);

        set0bind[0].binding = 0;
        set0bind[0].descriptorCount = 1;
        set0bind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set0bind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        set1bind[0].binding = 0;
        set1bind[0].descriptorCount = 1;
        set1bind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        set1bind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        const VkDescriptorSetLayoutCreateInfo ds0ci = vks::initializers::descriptorSetLayoutCreateInfo(set0bind);
        const VkDescriptorSetLayoutCreateInfo ds1ci = vks::initializers::descriptorSetLayoutCreateInfo(set1bind);
        std::array<VkDescriptorSetLayout, 2> dsl;
        VK_CHECK(vkCreateDescriptorSetLayout(d, &ds0ci, nullptr, &dsl[0]));
        VK_CHECK(vkCreateDescriptorSetLayout(d, &ds1ci, nullptr, &dsl[1]));
        passes.triangle.ds_layouts.push_back(dsl[0]);
        passes.triangle.ds_layouts.push_back(dsl[1]);
        VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo(2);
        plci.pSetLayouts = dsl.data();
        VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &passes.preZ.layout));
        pb._pipelineLayout = passes.triangle.layout;
        
        passes.triangle.pipeline = pb.build_pipeline(d, passes.triangle.pass);
        vkDestroyShaderModule(d, vert_module, nullptr);
        vkDestroyShaderModule(d, frag_module, nullptr);
        assert(passes.triangle.pipeline);

    }
    void setup_preZ_pass() {
        VkRenderPassCreateInfo rpci = vks::initializers::renderPassCreateInfo();

        VkAttachmentDescription depth = {};
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.format = depth_format;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentReference ZRef = {};
        ZRef.attachment = 0;
        ZRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass0 = {};
        subpass0.pDepthStencilAttachment = &ZRef;
        subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkSubpassDependency dep0 = {};
        dep0.dependencyFlags = 0;
        dep0.srcAccessMask = VK_ACCESS_NONE;
        dep0.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep0.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep0.dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep0.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep0.dstSubpass = 0;

        rpci.attachmentCount = 1;
        rpci.pAttachments = &depth;
        rpci.flags = 0;
        rpci.dependencyCount = 1;
        rpci.pDependencies = &dep0;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass0;

        VK_CHECK(vkCreateRenderPass(*device_wrapper, &rpci, nullptr, &passes.preZ.pass));

    }

    void setup_triangle_pass() {
        VkRenderPassCreateInfo rpci = vks::initializers::renderPassCreateInfo();

        VkAttachmentDescription color = {};
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentDescription depth = {};
        depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.format = depth_format;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentReference colorRef = {};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ZRef = {};
        ZRef.attachment = 1;
        ZRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDependency dep0 = {};
        dep0.dependencyFlags = 0;
        dep0.srcAccessMask = VK_ACCESS_NONE;
        dep0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dep0.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep0.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep0.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep0.dstSubpass = 0;
        VkSubpassDependency dep1 = {};
        dep1.dependencyFlags = 0;
        dep1.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep1.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep1.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep1.dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep1.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep1.dstSubpass = 0;

        const VkAttachmentDescription attachments[2] = { color,depth };
        const VkSubpassDependency deps[2] = { dep0,dep1 };

        VkSubpassDescription subpass0 = {};
        subpass0.colorAttachmentCount = 1;
        subpass0.pColorAttachments = &colorRef;
        subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass0.pDepthStencilAttachment = &ZRef;

        rpci.attachmentCount = 2;
        rpci.pAttachments = attachments;
        rpci.flags = 0;
        rpci.dependencyCount = 2;
        rpci.pDependencies = deps;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass0;

        VK_CHECK(vkCreateRenderPass(*device_wrapper, &rpci, nullptr, &passes.triangle.pass));
    }
    void setup_tonemap_pass() {
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

    virtual void prepare() override {
        vkjs::AppBase::prepare();

        d = *device_wrapper;
        setup_tonemap_pass();
        setup_preZ_pass();
        setup_triangle_pass();

        setup_preZ_pipeline();

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