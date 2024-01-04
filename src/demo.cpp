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
#include "stb_image.h"
#include "vkjs/vkjs.h"
#include "vkjs/appbase.h"
#include "vkjs/VulkanInitializers.hpp"
#include "vkjs/pipeline.h"
#include "vkjs/vkcheck.h"
#include "vkjs/vertex.h"
#include "vkjs/vk_descriptors.h"
#include "vkjs/shader_module.h"
#include <gli/generate_mipmaps.hpp>
#include "renderer/gli_utils.h"

namespace fs = std::filesystem;
using namespace glm;
using namespace jsr;

#define asfloat(x) static_cast<float>(x)
#define asuint(x) static_cast<uint32_t>(x)

class App : public vkjs::AppBase {
private:

    VkDevice d;

    vkjs::Image uvChecker;
    std::array<vkjs::Image, MAX_CONCURRENT_FRAMES> HDRImage{};
    std::array<VkFramebuffer, MAX_CONCURRENT_FRAMES> HDRFramebuffer{};
    std::array<VkDescriptorSet, MAX_CONCURRENT_FRAMES> HDRDescriptor{};

    VkSampler sampLinearRepeat;
    VkSampler sampNearestClampBorder;

    vkutil::DescriptorAllocator descAllocator;
    vkutil::DescriptorLayoutCache descLayoutCache;
    vkutil::DescriptorBuilder staticDescriptorBuilder;
    std::unordered_map<std::string, vkjs::Image> imageCache;

    vkjs::Buffer vtxbuf;
    vkjs::Buffer idxbuf;
    size_t minUboAlignment = 0;

    std::array<vkjs::Buffer,MAX_CONCURRENT_FRAMES> uboPassData;

    size_t drawDataBufferSize = 32 * 1024;
    vkjs::Buffer uboDrawData;

    size_t dynamicAlignment = 0;

    struct MeshBinary {
        uint32_t indexCount;
        uint32_t firstIndex;
        uint32_t firstVertex;
    };

    struct Material {
        VkDescriptorSet resources;
    };
    std::vector<Material> materials;

    struct Object {
        uint32_t mesh;
        mat4 mtxModel;
        Bounds aabb;
        VkDescriptorSet vkResources;
    };


    struct DrawData {
        glm::mat4 mtxModel;
        glm::mat4 mtxNormal;
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
    VkDescriptorSetLayout tonemapLayout = VK_NULL_HANDLE;

    VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};
    fs::path basePath = fs::path("../..");

    std::unique_ptr<World> scene;

    struct {
        RenderPass tonemap = {};
        RenderPass preZ = {};
        RenderPass triangle = {};
    } passes;

public:

    virtual void on_update_gui() override { ImGui::Text("Hello Vulkan"); }
    virtual void on_window_resized() override;

    void setup_descriptor_sets();

    void setup_descriptor_pools();

    void setup_objects();
    void setup_samplers();


    App(bool b) : AppBase(b) {
        depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    virtual ~App();

    virtual void build_command_buffers() override;

    virtual void render() override;

    void setup_tonemap_pipeline(RenderPass& pass);

    void setup_triangle_pipeline(RenderPass& pass);
    void setup_preZ_pass();

    void setup_triangle_pass();
    void setup_tonemap_pass();
    void setup_images();

    void update_uniforms() {

        uboPassData[currentFrame].copyTo(0, sizeof(passData), &passData);

    }

    virtual void prepare() override;

    virtual void get_enabled_features() override;

    virtual void get_enabled_extensions() override;

    void create_material_texture(std::string filename);
    bool load_texture2d(std::string filename, vkjs::Image* dest, bool autoMipmap, int& w, int& h, int& nchannel);
};

void demo()
{
    jsrlib::gLogWriter.SetFileName("vulkan_engine.log");
    App* app = new App(true);
    app->settings.fullscreen = false;
    app->settings.exclusive = false;
    app->settings.vsync = true;
    app->width = 1900;
    app->height = 1000;
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

void App::on_window_resized()
{
    setup_images();
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i) {
        if (HDRDescriptor[i] != VK_NULL_HANDLE) {
            // update descript sets
            VkDescriptorImageInfo inf{};
            inf.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            inf.imageView = HDRImage[i].view;
            inf.sampler = sampNearestClampBorder;
            VkWriteDescriptorSet write = vks::initializers::writeDescriptorSet(HDRDescriptor[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &inf, 1);
            vkUpdateDescriptorSets(d, 1, &write, 0, nullptr);
        }
    }
}

void App::setup_descriptor_sets()
{
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        uboDrawData.descriptor.range = sizeof(DrawData);
        uboDrawData.descriptor.offset = i * drawDataBufferSize;

        uboPassData[i].descriptor.range = sizeof(PassData);
        uboPassData[i].descriptor.offset = 0;

        vkutil::DescriptorBuilder::begin(&descLayoutCache, &descAllocator)
            .bind_buffer(0, &uboPassData[i].descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_buffer(1, &uboDrawData.descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_image(2, &uvChecker.descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(triangleDescriptors[i]);

        HDRImage[i].setup_descriptor();
        HDRImage[i].descriptor.sampler = sampNearestClampBorder;
        HDRImage[i].descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkutil::DescriptorBuilder::begin(&descLayoutCache, &descAllocator)
            .bind_image(0, &HDRImage[i].descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(HDRDescriptor[i]);

    }

}

void App::setup_descriptor_pools()
{
    descAllocator.init(*device);
    descLayoutCache.init(*device);
}

void App::setup_objects()
{
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
                obj.vkResources = materials[scene->meshes[e].material].resources;

                ddlst.emplace_back(DrawData{ mtxModel,
                    mat4(transpose(inverse(mat3(mtxModel)))),
                    vec4(range(reng), range(reng), range(reng), 1.0f) });
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

void App::setup_samplers()
{
    auto samplerCI = vks::initializers::samplerCreateInfo();
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCI.anisotropyEnable = VK_TRUE;
    samplerCI.maxAnisotropy = 8.f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerCI.compareEnable = VK_FALSE;
    samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.maxLod = 16.f;
    VK_CHECK(device->create_sampler(samplerCI, &sampLinearRepeat));

    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.magFilter = VK_FILTER_NEAREST;
    samplerCI.minFilter = VK_FILTER_NEAREST;
    samplerCI.maxLod = 0.0f;
    VK_CHECK(device->create_sampler(samplerCI, &sampNearestClampBorder));

}

App::~App()
{
    vkDeviceWaitIdle(d);

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
        vkDestroyFramebuffer(d, HDRFramebuffer[i], 0);
    }
}

void App::build_command_buffers()
{
    VkCommandBuffer cmd = draw_cmd_buffers[currentFrame];

    VkRenderPassBeginInfo beginPass = vks::initializers::renderPassBeginInfo();
    VkClearValue clearVal[2];
    clearVal[0].color = { 0.8f,0.8f,0.8f,1.0f };
    clearVal[1].depthStencil.depth = 1.0f;

    beginPass.clearValueCount = 2;
    beginPass.pClearValues = &clearVal[0];
    beginPass.renderPass = passes.triangle.pass;
    beginPass.framebuffer = HDRFramebuffer[currentFrame];
    beginPass.renderArea.extent = swapchain.vkb_swapchain.extent;
    beginPass.renderArea.offset = { 0,0 };

    HDRImage[currentFrame].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    /*
    HDRImage[currentFrame].record_change_layout(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    HDRImage[currentFrame].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
*/
    vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);

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
        const int materialIndex = scene->meshes[obj.mesh].material;
        const auto& material = scene->materials[materialIndex];
        
        if (material.alphaMode != ALPHA_MODE_BLEND) {

            const VkDescriptorSet dsets[2] = { triangleDescriptors[currentFrame], obj.vkResources };
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                passes.triangle.layout, 0, 2, dsets, 1, &dynOffset);

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.firstIndex, mesh.firstVertex, 0);
        }
        dynOffset += dynamicAlignment;
    }
    vkCmdEndRenderPass(cmd);
/*
    HDRImage[currentFrame].record_change_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
*/
    HDRImage[currentFrame].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    clearVal[0].color = { 0.f,0.f,0.f,1.f };
    beginPass.clearValueCount = 1;
    beginPass.renderPass = passes.tonemap.pass;
    beginPass.framebuffer = fb[currentFrame];
    vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.tonemap.pipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        passes.tonemap.layout, 0, 1, &HDRDescriptor[currentFrame], 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

}

void App::render()
{
    passData.mtxView = camera.GetViewMatrix();
    passData.mtxProjection = glm::perspective(radians(camera.Zoom), (float)width / height, .01f, 500.f);
    update_uniforms();

    if (fb[currentFrame] != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(d, fb[currentFrame], 0);
    }

    std::array<VkImageView, 1> targets = { swapchain.views[currentBuffer] };
    auto fbci = vks::initializers::framebufferCreateInfo();
    fbci.renderPass = passes.tonemap.pass;
    fbci.layers = 1;
    fbci.attachmentCount = (uint32_t) targets.size();
    fbci.pAttachments = targets.data();
    fbci.width = width;
    fbci.height = height;
    VK_CHECK(vkCreateFramebuffer(d, &fbci, 0, &fb[currentFrame]));

    build_command_buffers();
}

void App::setup_tonemap_pipeline(RenderPass& pass)
{
    auto vertexInput = vkjs::Vertex::vertex_input_description_position_only();

    const std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };
    vkjs::PipelineBuilder pb = {};
    pb._rasterizer = vks::initializers::pipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    pb._depthStencil = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
    pb._dynamicStates = vks::initializers::pipelineDynamicStateCreateInfo(dynStates);
    pb._inputAssembly = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    pb._multisampling = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    pb._vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    pb._colorBlendAttachments.push_back(vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE));


    vkjs::ShaderModule vert_module(*device);
    vkjs::ShaderModule frag_module(*device);
    fs::path vert_spirv_filename = basePath / "shaders/triquad.vert.spv";
    fs::path frag_spirv_filename = basePath / "shaders/post.frag.spv";
    VK_CHECK(vert_module.create(vert_spirv_filename));
    VK_CHECK(frag_module.create(frag_spirv_filename));

    pb._shaderStages.emplace_back();
    auto& vert_stage = pb._shaderStages.back();
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.pName = "main";
    vert_stage.module = vert_module.module();
    pb._shaderStages.emplace_back();
    auto& frag_stage = pb._shaderStages.back();
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.pName = "main";
    frag_stage.module = frag_module.module();

    /*
    layout:
    set 0
        0: ubo
    set 1
        0: dyn. ubo
    */

    std::vector<VkDescriptorSetLayoutBinding> set0bind(1);

    set0bind[0].binding = 0;
    set0bind[0].descriptorCount = 1;
    set0bind[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set0bind[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ds0ci = vks::initializers::descriptorSetLayoutCreateInfo(set0bind);
    
    tonemapLayout = descLayoutCache.create_descriptor_layout(&ds0ci);
    assert(tonemapLayout);

    VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo(1);
    plci.pSetLayouts = &tonemapLayout;
    VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &pass.layout));
    pb._pipelineLayout = pass.layout;
    pass.pipeline = pb.build_pipeline(d, pass.pass);
    assert(pass.pipeline);
}

void App::setup_triangle_pipeline(RenderPass& pass)
{
    auto vertexInput = vkjs::Vertex::vertex_input_description();
    const std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };

    vkjs::PipelineBuilder pb = {};
    pb._rasterizer = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    pb._depthStencil = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    pb._dynamicStates = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStates);
    pb._inputAssembly = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    pb._multisampling = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    pb._vertexInputInfo = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInput.bindings, vertexInput.attributes);

    vkjs::ShaderModule vert_module(*device);
    vkjs::ShaderModule frag_module(*device);
    VK_CHECK(vert_module.create(basePath / "shaders/triangle_v2.vert.spv"));
    VK_CHECK(frag_module.create(basePath / "shaders/triangle_v2.frag.spv"));

    pb._shaderStages.emplace_back();
    auto& vert_stage = pb._shaderStages.back();
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.pName = "main";
    vert_stage.module = vert_module.module();
    pb._shaderStages.emplace_back();
    auto& frag_stage = pb._shaderStages.back();
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.pName = "main";
    frag_stage.module = frag_module.module();

    /*
    layout:
    set 0
        0: ubo
        1: dyn. ubo
        2: combined image
    */

    std::vector<VkDescriptorSetLayoutBinding> set0bind(3);
    std::vector<VkDescriptorSetLayoutBinding> set1bind(3);

    set0bind[0].binding = 0;
    set0bind[0].descriptorCount = 1;
    set0bind[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set0bind[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    set0bind[1].binding = 1;
    set0bind[1].descriptorCount = 1;
    set0bind[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    set0bind[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    set0bind[2].binding = 2;
    set0bind[2].descriptorCount = 1;
    set0bind[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set0bind[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


    set1bind[0].binding = 0;
    set1bind[0].descriptorCount = 1;
    set1bind[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set1bind[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    set1bind[1].binding = 1;
    set1bind[1].descriptorCount = 1;
    set1bind[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set1bind[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    set1bind[2].binding = 2;
    set1bind[2].descriptorCount = 1;
    set1bind[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set1bind[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ds0ci = vks::initializers::descriptorSetLayoutCreateInfo(set0bind);
    VkDescriptorSetLayoutCreateInfo ds1ci = vks::initializers::descriptorSetLayoutCreateInfo(set1bind);
    std::array<VkDescriptorSetLayout, 2> dsl;
    dsl[0] = descLayoutCache.create_descriptor_layout(&ds0ci);
    dsl[1] = descLayoutCache.create_descriptor_layout(&ds1ci);
    assert(dsl[0] && dsl[1]);

    VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo((uint32_t)dsl.size());
    plci.pSetLayouts = dsl.data();
    VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &pass.layout));
    pb._pipelineLayout = pass.layout;

    VkPipelineColorBlendAttachmentState blend0 = vks::initializers::pipelineColorBlendAttachmentState(
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
    pb._colorBlendAttachments.push_back(blend0);

    pass.pipeline = pb.build_pipeline(d, pass.pass);
    assert(pass.pipeline);
}

void App::setup_preZ_pass()
{
    VkRenderPassCreateInfo rpci = vks::initializers::renderPassCreateInfo();

    VkAttachmentDescription depth = {};
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.format = depth_format;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

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
    dep0.dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
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

void App::setup_triangle_pass()
{
    VkRenderPassCreateInfo rpci = vks::initializers::renderPassCreateInfo();

    VkAttachmentDescription color = {};
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.samples = VK_SAMPLE_COUNT_1_BIT;

    VkAttachmentDescription depth = {};
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.format = depth_format;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ZRef = {};
    ZRef.attachment = 1;
    ZRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDependency dep0 = {};
    dep0.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dep0.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dep0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dep0.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
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

void App::setup_tonemap_pass()
{
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

void App::setup_images()
{
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        if (HDRImage[i].image != VK_NULL_HANDLE) {
            device->destroy_image(&HDRImage[i]);
        }
        if (HDRFramebuffer[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(*device, HDRFramebuffer[i], nullptr);
        }
        VK_CHECK(device->create_color_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, 
            swapchain.extent(),
            &HDRImage[i]));

        std::array<VkImageView, 2> targets = { HDRImage[i].view,  depth_image.view};
        auto fbci = vks::initializers::framebufferCreateInfo();
        fbci.renderPass = passes.triangle.pass;
        fbci.layers = 1;
        fbci.attachmentCount = (uint32_t)targets.size();
        fbci.pAttachments = targets.data();
        fbci.width = HDRImage[i].extent.width;
        fbci.height = HDRImage[i].extent.height;
        VK_CHECK(vkCreateFramebuffer(d, &fbci, 0, &HDRFramebuffer[i]));

        //HDRImage[i].change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void App::prepare()
{
    vkjs::AppBase::prepare();

    setup_descriptor_pools();
    setup_samplers();

    // Calculate required alignment based on minimum device offset alignment
    minUboAlignment = device->vkb_physical_device.properties.limits.minUniformBufferOffsetAlignment;

    camera.MovementSpeed = 0.001f;

    d = *device;
    int w, h, nc;

    setup_triangle_pass();
    setup_triangle_pipeline(passes.triangle);
    setup_tonemap_pass();
    setup_tonemap_pipeline(passes.tonemap);
    setup_images();

    device->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 32ULL * 1024 * 1024, &vtxbuf);
    device->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 32ULL * 1024 * 1024, &idxbuf);

    if (!load_texture2d("../../resources/images/uv_checker_large.png", &uvChecker, true, w, h, nc))
    {
        device->create_texture2d(VK_FORMAT_R8G8B8A8_SRGB, { 1,1,1 }, &uvChecker);
        uvChecker.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    }

    //uvChecker.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    uvChecker.setup_descriptor();
    uvChecker.descriptor.sampler = sampLinearRepeat;

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
    {
        device->create_uniform_buffer(8 * 1024, false, &uboPassData[i]);
        uboPassData[i].map();
    }

    scene = std::make_unique<World>();

    auto scenePath = fs::path("../../resources/models/sponza");
    jsr::gltfLoadWorld(scenePath / "sponza_j.gltf", *scene);

    std::vector<vkjs::Vertex> vertices;
    std::vector<uint16_t> indices;

    uint32_t firstIndex(0);
    uint32_t firstVertex(0);

    materials.resize(scene->materials.size());

    int i = 0;
    jsrlib::Info("Loading images...");
    for (auto& it : scene->materials)
    {
        std::string fname1 = (scenePath / it.texturePaths.albedoTexturePath).u8string();
        std::string fname2 = (scenePath / it.texturePaths.normalTexturePath).u8string();
        std::string fname3 = (scenePath / it.texturePaths.specularTexturePath).u8string();
        create_material_texture(fname1);
        create_material_texture(fname2);
        create_material_texture(fname3);

        vkutil::DescriptorBuilder::begin(&descLayoutCache, &descAllocator)
            .bind_image(0, &imageCache.at(fname1).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(1, &imageCache.at(fname2).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(2, &imageCache.at(fname3).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(materials[i++].resources);
    }

    for (size_t i(0); i < scene->meshes.size(); ++i)
    {
        auto& mesh = scene->meshes[i];
        for (size_t i(0); i < mesh.positions.size(); ++i)
        {
            vertices.emplace_back();
            auto& v = vertices.back();
            v.xyz = mesh.positions[i];
            v.uv = mesh.uvs[i];
            v.pack_normal(mesh.normals[i]);
            v.pack_tangent(mesh.tangents[i]);
            v.pack_color(vec4(1.0f));
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

    const size_t size = drawDataBufferSize * 2;
    device->create_uniform_buffer(size, false, &uboDrawData);
    uboDrawData.map();
    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
    {
        uboDrawData.copyTo(i * drawDataBufferSize, drawData.size(), drawData.data());
    }
    setup_descriptor_sets();

    prepared = true;
}

void App::get_enabled_features()
{
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

void App::get_enabled_extensions()
{
    enabled_device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
}

void App::create_material_texture(std::string filename)
{
    if (imageCache.find(filename) == imageCache.end())
    {
        int w, h, nc;
        // load image
        vkjs::Image newImage;
        fs::path base = fs::path(filename).parent_path();
        fs::path name = fs::path(filename).filename();
        name.replace_extension("dds");
        auto dds = base / "dds" / name;

        bool bOk = false;
        gli::texture tex = gli::load(dds.u8string());
        if (!tex.empty())
        {
            device->create_texture2d_with_mips(gliutils::convert_format(tex.format()), gliutils::convert_extent(tex.extent()), &newImage);
            vkjs::Buffer stagebuf;
            device->create_staging_buffer(tex.size(), &stagebuf);
            stagebuf.copyTo(0, tex.size(), tex.data());
            newImage.upload([&tex](uint32_t layer, uint32_t face, uint32_t level, vkjs::Image::UploadInfo* inf)
                {
                    inf->extent = gliutils::convert_extent(tex.extent(level));
                    inf->offset = (size_t)tex.data(layer, face, level) - (size_t)tex.data();
                }, &stagebuf);
            newImage.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            device->destroy_buffer(&stagebuf);
            jsrlib::Info("%s Loaded", dds.u8string().c_str());
        }
        else if (!load_texture2d(filename, &newImage, true, w, h, nc))
        {
            jsrlib::Error("%s notfund", filename.c_str());
            device->create_texture2d(VK_FORMAT_R8G8B8A8_UNORM, { 1,1,1 }, &newImage);
            newImage.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else
        {
            jsrlib::Info("%s Loaded", filename.c_str());
        }
        newImage.setup_descriptor();
        newImage.descriptor.sampler = sampLinearRepeat;
        imageCache.insert({ filename,newImage });
    }
}

bool App::load_texture2d(std::string filename, vkjs::Image* dest, bool autoMipmap, int& w, int& h, int& nchannel)
{
    //int err = stbi_info(filename.c_str(), &w, &h, &nchannel);

    auto* data = stbi_load(filename.c_str(), &w, &h, &nchannel, STBI_rgb_alpha);
    if (!data)
    {
        return false;
    }


    size_t size = w * h * 4;
    vkjs::Buffer stage;
    device->create_staging_buffer(size, &stage);
    stage.copyTo(0, size, data);
    stbi_image_free(data);

    if (autoMipmap) {
        device->create_texture2d_with_mips(VK_FORMAT_R8G8B8A8_UNORM, { (uint32_t)w,(uint32_t)h,1 }, dest);
    }
    else {
        device->create_texture2d(VK_FORMAT_R8G8B8A8_UNORM, { (uint32_t)w,(uint32_t)h,1 }, dest);
    }

    dest->upload({ (uint32_t)w,(uint32_t)h,1 }, 0, 0, 0, 0ull, &stage);
    if (autoMipmap) {
        dest->generate_mipmaps();
    }
    else {
        dest->change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    device->destroy_buffer(&stage);

    return true;
}
