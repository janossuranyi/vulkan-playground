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
#include "vkjs/vk_descriptors.h"

namespace fs = std::filesystem;
using namespace glm;
using namespace jsr;

#define asfloat(x) static_cast<float>(x)
#define asuint(x) static_cast<uint32_t>(x)

class App : public vkjs::AppBase {
private:
    VkDevice d;

    vkutil::DescriptorAllocator descAllocator;
    vkutil::DescriptorLayoutCache descLayoutCache;
    vkutil::DescriptorBuilder staticDescriptorBuilder;

    vkjs::Buffer vtxbuf;
    vkjs::Buffer idxbuf;
    size_t minUboAlignment = 0;

    std::array<vkjs::Buffer,MAX_CONCURRENT_FRAMES> uboPassData;
    std::array<vkjs::Buffer,MAX_CONCURRENT_FRAMES> uboDrawData;
    size_t dynamicAlignment = 0;

    struct MeshBinary {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t firstVertex;
    };

    struct Object {
        uint32_t mesh;
        mat4 mtxModel;
        Bounds aabb;
    };


    struct DrawData {
        glm::mat4 mtxModel;
        glm::vec4 color;
    };

    std::vector<MeshBinary> meshes;
    std::vector<Object> objects;
    std::vector<uint8_t> drawData;

    struct PassData {
        glm::mat4 mtxView;
        glm::mat4 mtxProjection;
    } passData;

    struct RenderPass {
        VkRenderPass pass;
        VkPipelineLayout layout;
        VkPipeline pipeline;
    };

    VkDescriptorSet triangleDescriptors[MAX_CONCURRENT_FRAMES];

    VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};
    fs::path base_path = fs::path("../..");

    std::unique_ptr<World> scene;

    struct {
        RenderPass tonemap = {};
        RenderPass preZ = {};
        RenderPass triangle = {};
    } passes;

public:

    virtual void on_update_gui() override {}

    void setup_descriptor_sets() {


        for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
        {
            uboDrawData[i].descriptor.range = sizeof(DrawData);
            uboPassData[i].descriptor.range = sizeof(PassData);
            vkutil::DescriptorBuilder::begin(&descLayoutCache, &descAllocator)
            .bind_buffer(0, &uboPassData[i].descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_buffer(1, &uboDrawData[i].descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
            .build(triangleDescriptors[i]);
        }
    }

    void setup_descriptor_pools() {        
        descAllocator.init(*device);
        descLayoutCache.init(*device);
    }

    void setup_objects() {
        std::vector<mat4> parentMatrices(scene->scene.rootNodes.size(), mat4(1.0f));
        std::vector<int> nodesToProcess = scene->scene.rootNodes;
        objects.clear();

        std::random_device rdev;
        std::default_random_engine reng(rdev());
        std::uniform_real_distribution<float> range(0.0f, 0.25f);

        dynamicAlignment = sizeof(DrawData);
        if (minUboAlignment > 0) {
            dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
        }

        std::vector<DrawData> ddlst;
        while (nodesToProcess.empty() == false)
        {
            int idx = nodesToProcess.back();
            nodesToProcess.pop_back();
            mat4 parentMatrix = parentMatrices.back();
            parentMatrices.pop_back();

            auto& node = scene->scene.nodes[idx];
            mat4 mtxModel = parentMatrix * node.getTransform();
            if (node.isMesh()) {
                for (auto& e : node.getEntities()) {
                    Object obj;
                    obj.mesh = e;
                    obj.mtxModel = mtxModel;
                    obj.aabb = scene->meshes[e].aabb.Transform(mtxModel);
                    DrawData dd;
                    dd.mtxModel = mtxModel;
                    dd.color = vec4(range(reng), range(reng), range(reng), 1.0f);
                    ddlst.push_back(dd);
                    objects.push_back(obj);
                }
            }
            for (const auto& e : node.getChildren()) {
                nodesToProcess.push_back(e);
                parentMatrices.push_back(mtxModel);
            }
        }
        drawData.resize(ddlst.size() * dynamicAlignment);
        size_t offset = 0;
        for (size_t i(0); i < ddlst.size(); ++i) {
            memcpy(&drawData[offset], &ddlst[i], sizeof(DrawData));
            offset += dynamicAlignment;
        }
    }

    App(bool b) : AppBase(b) {
        depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    }

    virtual ~App() {
        vkDeviceWaitIdle(d);

        device->destroy_buffer(&vtxbuf);
        device->destroy_buffer(&idxbuf);

        std::array<RenderPass*, 3> rpasses = { &passes.preZ,&passes.tonemap,&passes.triangle };
        for (const auto* pass : rpasses) {
            if (pass->pipeline) vkDestroyPipeline(d, pass->pipeline, nullptr);
            if (pass->layout) vkDestroyPipelineLayout(d, pass->layout, nullptr);
        }
        vkDestroyRenderPass(d, passes.tonemap.pass, nullptr);
        vkDestroyRenderPass(d, passes.preZ.pass, nullptr);
        vkDestroyRenderPass(d, passes.triangle.pass, nullptr);

        descLayoutCache.cleanup();
        descAllocator.cleanup();

        for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
        {
            vkDestroyFramebuffer(d, fb[i], 0);
            device->destroy_buffer(&uboPassData[i]);
            device->destroy_buffer(&uboDrawData[i]);
        }
    }

    virtual void build_command_buffers() override {
        VkCommandBuffer cmd = draw_cmd_buffers[current_frame];
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.triangle.pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vtxbuf.buffer, &offset);
        vkCmdBindIndexBuffer(cmd, idxbuf.buffer, 0ull, VK_INDEX_TYPE_UINT16);
        VkViewport viewport{ 0.f,0.f,float(width),float(height),0.0f,1.0f };
        VkRect2D scissor{ 0,0,width,height };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        uint32_t dynOffset = 0;
        for (const auto& obj : objects) {
            const auto& mesh = meshes[obj.mesh];

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                passes.triangle.layout, 0, 1, &triangleDescriptors[current_frame], 1, &dynOffset);

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.firstIndex, mesh.firstVertex, 0);
            dynOffset += dynamicAlignment;
        }
    }
    virtual void render() override {        

        passData.mtxView = camera.GetViewMatrix();
        passData.mtxProjection = glm::perspective(radians(camera.Zoom), (float)width / height, .01f, 500.f);
        update_uniforms();
        
        if (fb[current_frame] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(d, fb[current_frame], 0);
        }

        std::array<VkImageView, 2> targets = { swapchain.views[current_buffer],  depth_image.view};
        auto fbci = vks::initializers::framebufferCreateInfo();
        fbci.renderPass = passes.triangle.pass;
        fbci.layers = 1;
        fbci.attachmentCount = 2;
        fbci.pAttachments = targets.data();
        fbci.width = width;
        fbci.height = height;
        VK_CHECK(vkCreateFramebuffer(d, &fbci, 0, &fb[current_frame]));

        VkRenderPassBeginInfo beginPass = vks::initializers::renderPassBeginInfo();
        VkClearValue clearVal[2];
        clearVal[0].color = {0.8f,0.8f,0.8f,1.0f};
        clearVal[1].depthStencil.depth = 1.0f;

        beginPass.clearValueCount = 2;
        beginPass.pClearValues = &clearVal[0];
        beginPass.renderPass = passes.triangle.pass;
        beginPass.framebuffer = fb[current_frame];
        beginPass.renderArea.extent = swapchain.vkb_swapchain.extent;
        beginPass.renderArea.offset = { 0,0 };

        VkCommandBuffer cmd = draw_cmd_buffers[current_frame];
        vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);
        build_command_buffers();
        vkCmdEndRenderPass(cmd);

    }

    void setup_preZ_pipeline(RenderPass& pass) {
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

        VkDescriptorSetLayoutCreateInfo ds0ci = vks::initializers::descriptorSetLayoutCreateInfo(set0bind);
        VkDescriptorSetLayoutCreateInfo ds1ci = vks::initializers::descriptorSetLayoutCreateInfo(set1bind);
        std::array<VkDescriptorSetLayout, 2> dsl;
        dsl[0] = descLayoutCache.create_descriptor_layout(&ds0ci);
        dsl[1] = descLayoutCache.create_descriptor_layout(&ds1ci);
        assert(dsl[0] && dsl[1]);

        VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo(2);
        plci.pSetLayouts = dsl.data();        
        VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &pass.layout));
        pb._pipelineLayout = pass.layout;
        pass.pipeline = pb.build_pipeline(d, pass.pass);
        vkDestroyShaderModule(d, vert_module, nullptr);
        vkDestroyShaderModule(d, frag_module, nullptr);
        assert(pass.pipeline);

    }

    void setup_triangle_pipeline(RenderPass& pass) {
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
            1: dyn. ubo
        */

        std::vector<VkDescriptorSetLayoutBinding> set0bind(2);

        set0bind[0].binding = 0;
        set0bind[0].descriptorCount = 1;
        set0bind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        set0bind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        set0bind[1].binding = 1;
        set0bind[1].descriptorCount = 1;
        set0bind[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        set0bind[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo ds0ci = vks::initializers::descriptorSetLayoutCreateInfo(set0bind);
        std::array<VkDescriptorSetLayout, 1> dsl;
        dsl[0] = descLayoutCache.create_descriptor_layout(&ds0ci);
        assert(dsl[0]);

        VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo(1);
        plci.pSetLayouts = dsl.data();
        VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &pass.layout));
        pb._pipelineLayout = pass.layout;
        
        VkPipelineColorBlendAttachmentState blend0 = vks::initializers::pipelineColorBlendAttachmentState(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
        pb._colorBlendAttachments.push_back(blend0);

        pass.pipeline = pb.build_pipeline(d, pass.pass);
        vkDestroyShaderModule(d, vert_module, nullptr);
        vkDestroyShaderModule(d, frag_module, nullptr);
        assert(pass.pipeline);

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

        VK_CHECK(vkCreateRenderPass(*device, &rpci, nullptr, &passes.preZ.pass));

    }

    void setup_triangle_pass() {
        VkRenderPassCreateInfo rpci = vks::initializers::renderPassCreateInfo();

        VkAttachmentDescription color = {};
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.format = swapchain.vkb_swapchain.image_format;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentDescription depth = {};
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.format = depth_format;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentReference colorRef = {};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference ZRef = {};
        ZRef.attachment = 1;
        ZRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDependency dep0 = {};
        dep0.dependencyFlags = 0;
        dep0.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
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

        VK_CHECK(vkCreateRenderPass(*device, &rpci, nullptr, &passes.triangle.pass));
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

        VK_CHECK(vkCreateRenderPass(*device, &rpci, nullptr, &passes.tonemap.pass));
    }

    void update_uniforms() {

        uboPassData[current_frame].copyTo(0, sizeof(passData), &passData);

    }

    virtual void prepare() override {
        vkjs::AppBase::prepare();

        setup_descriptor_pools();

        // Calculate required alignment based on minimum device offset alignment
        minUboAlignment = device->vkb_physical_device.properties.limits.minUniformBufferOffsetAlignment;

        camera.MovementSpeed = 0.001f;

        d = *device;
        
        setup_triangle_pass();
        setup_triangle_pipeline(passes.triangle);

        device->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 32ULL * 1024 * 1024, &vtxbuf);
        device->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 32ULL * 1024 * 1024, &idxbuf);

        for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
        {
            device->create_uniform_buffer(8 * 1024, false, &uboPassData[i]);
            uboPassData[i].map();
        }

        scene = std::make_unique<World>();

        jsr::gltfLoadWorld(fs::path("../../resources/models/sponza/sponza_j.gltf"), *scene);

        std::vector<vkjs::Vertex> vertices;
        std::vector<uint16_t> indices;

        uint32_t firstIndex(0);
        uint32_t firstVertex(0);

        for (size_t i(0); i < scene->meshes.size(); ++i)
        {
            auto& mesh = scene->meshes[i];
            for (size_t i(0); i < mesh.positions.size(); ++i)
            {
                vertices.emplace_back();
                auto& v = vertices.back();
                v.xyz = mesh.positions[i];
            }
            meshes.emplace_back();
            auto& rmesh = meshes.back();
            rmesh.firstVertex = firstVertex;
            firstVertex += mesh.positions.size();

            for (size_t i(0); i < mesh.indices.size(); ++i)
            {
                indices.push_back((uint16_t)mesh.indices[i]);
            }
            rmesh.firstIndex = firstIndex;
            rmesh.indexCount = mesh.indices.size();
            firstIndex += rmesh.indexCount;
            
        }

        VkDeviceSize vertexBytes = sizeof(vertices[0]) * vertices.size();
        VkDeviceSize indexBytes = sizeof(indices[0]) * indices.size();

        vkjs::Buffer stagingBuffer;

        device->create_staging_buffer(vertexBytes, &stagingBuffer);
        stagingBuffer.copyTo(0, vertexBytes, vertices.data());
        device->buffer_copy(&stagingBuffer, &vtxbuf, 0, 0, vertexBytes);
        if (indexBytes > vertexBytes) 
        {
            device->destroy_buffer(&stagingBuffer);
            device->create_staging_buffer(indexBytes, &stagingBuffer);
        }
        stagingBuffer.copyTo(0, indexBytes, indices.data());
        device->buffer_copy(&stagingBuffer, &idxbuf, 0, 0, indexBytes);
        device->destroy_buffer(&stagingBuffer);

        setup_objects();

        const size_t size = drawData.size();
        for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
        {
            device->create_uniform_buffer(size, false, &uboDrawData[i]);
            uboDrawData[i].map();
            uboDrawData[i].copyTo(0, size, drawData.data());
        }
        setup_descriptor_sets();

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