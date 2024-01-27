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
#include "jsrlib/jsr_math.h"
#include "jsrlib/jsr_frustum.h"

//#include "jsrlib/jsr_jobsystem2.h"
#include "jobsys.h"
#include "world.h"
#include "gltf_loader.h"
#include "stb_image.h"
#include "vkjs/vkjs.h"
#include "vkjs/appbase.h"
#include "vkjs/VulkanInitializers.hpp"
#include "vkjs/pipeline.h"
#include "vkjs/vkcheck.h"
#include "vkjs/vertex.h"
#include "vkjs/vk_descriptors.h"
#include "vkjs/shader_module.h"
#include "vkjs/spirv_utils.h"
#include <gli/generate_mipmaps.hpp>
#include "renderer/gli_utils.h"

namespace fs = std::filesystem;
using namespace glm;
using namespace jsr;

#define asfloat(x) static_cast<float>(x)
#define asuint(x) static_cast<uint32_t>(x)

inline static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

struct UniformBufferPool {
    vkjs::Buffer* buffer{};
    std::atomic_uint32_t bytesAlloced = 0u;
    uint32_t offsetAligment = 64u;
    uint32_t internalOffset = 0u;
    size_t _size = 0;
    VkDescriptorBufferInfo descriptor = {};
    
    UniformBufferPool() = delete;
    UniformBufferPool(vkjs::Buffer* buffer, size_t size, uint32_t startOffset, uint32_t offsetAligment) :
        buffer(buffer), _size(size), offsetAligment(offsetAligment), bytesAlloced(0) {
    
        internalOffset = startOffset;
        descriptor.buffer = buffer->buffer;
        descriptor.offset = internalOffset;
        descriptor.range = size;
    }

    template<class T>
    VkDescriptorBufferInfo Allocate(uint32_t count, const T* data)
    {   
        const uint32_t alignedSize = static_cast<uint32_t>( align_size(sizeof(T), offsetAligment) );
        const uint32_t size = count * alignedSize;
        const uint32_t offset = bytesAlloced.fetch_add(size);
        

        assert((internalOffset + offset + size) < buffer->size);

        VkDescriptorBufferInfo out = {};
        out.buffer = buffer->buffer;
        out.offset = offset;
        out.range = sizeof(T);

        if (data)
        {
            size_t it_offset = internalOffset + offset;
            for (uint32_t it = 0; it < count; ++it)
            {
                buffer->copyTo(it_offset, sizeof(T), &data[it]);
                it_offset += alignedSize;
            }
        }
        return out;
    }
    void reset() { bytesAlloced = 0; }
};

class App : public vkjs::AppBase {
private:
    //const VkFormat HDR_FMT = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    const VkFormat HDR_RT_FMT = VK_FORMAT_R16G16B16A16_SFLOAT;
    const VkFormat NORMAL_RT_FMT = VK_FORMAT_R16G16_SFLOAT;
    bool fogEnabled = true;
    float fps = 0.f;
    float maxZ = 0.0f, minZ = 0.0f;
    VkDevice d;
    bool smaaChanged = false;
    bool firstRun = true;

    vkjs::Image uvChecker;
    vkjs::Image ssaoNoise;
    std::array<VkImageView, MAX_CONCURRENT_FRAMES> depthResolvedView{};
    std::array<vkjs::Image, MAX_CONCURRENT_FRAMES> depthResolved{};
    std::array<vkjs::Image, MAX_CONCURRENT_FRAMES> HDRImage_MS{};
    std::array<vkjs::Image, MAX_CONCURRENT_FRAMES> HDRImage{};
    std::array<vkjs::Image, MAX_CONCURRENT_FRAMES> HDR_NormalImage_MS{};
    std::array<vkjs::Image, MAX_CONCURRENT_FRAMES> HDR_NormalImage{};
    std::array<VkFramebuffer, MAX_CONCURRENT_FRAMES> HDRFramebuffer{};
    std::array<VkDescriptorSet, MAX_CONCURRENT_FRAMES> HDRDescriptor{};
    std::array<UniformBufferPool*, MAX_CONCURRENT_FRAMES> drawPool{};

    VkSampler sampLinearRepeat;
    VkSampler sampNearestClampBorder;

    vkutil::DescriptorAllocator descAllocator;
    vkutil::DescriptorLayoutCache descLayoutCache;
    vkutil::DescriptorBuilder staticDescriptorBuilder;
    std::unordered_map<std::string, vkjs::Image> imageCache;

    vkjs::Buffer vtxbuf;
    vkjs::Buffer idxbuf;
    size_t minUboAlignment = 0;

    vkjs::Buffer vtxStagingBuffer;
    std::array<vkjs::Buffer, MAX_CONCURRENT_FRAMES> transientVtxBuf;

    std::array<vkjs::Buffer, MAX_CONCURRENT_FRAMES> uboPassData;
    std::array<vkjs::Buffer, MAX_CONCURRENT_FRAMES> uboPostProcessData;

    size_t drawDataBufferSize = 0;

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
    std::vector<DrawData> drawDataStruct;

    struct PassData {
        glm::mat4 mtxView;
        glm::mat4 mtxProjection;
        glm::vec4 vScaleBias;
        glm::vec4 avSSAOkernel[12];
        glm::vec4 vLightPos;
        glm::vec4 vLightColor;
    } passData;

    
    struct PostProcessData {
        mat4 mtxInvProj;
        glm::vec4 vCameraPos;
        glm::vec4 vFogParams;
        glm::vec4 vSunPos;
        /*
        glm::vec3 fogColor;
        float fogLinearStart;
        float fogLinearEnd;
        float fogDensity;
        int fogEquation;
        int fogEnabled;
        */
        float fExposure;
        float fZnear;
        float fZfar;
        int pad0;
    } postProcessData;
    static_assert((sizeof(PostProcessData) & 15) == 0);

    struct RenderPass {
        VkRenderPass pass;
        VkPipelineLayout layout;
        VkPipeline pipeline;
    };

    VkDescriptorSet triangleDescriptors[MAX_CONCURRENT_FRAMES];
    VkDescriptorSetLayout tonemapLayout = VK_NULL_HANDLE;

    VkPipelineLayout debugPipelineLayout = VK_NULL_HANDLE;
    VkPipeline debugPipeline = VK_NULL_HANDLE;

    VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};
    fs::path basePath = fs::path("../..");

    std::unique_ptr<World> world;

    struct {
        RenderPass tonemap = {};
        RenderPass preZ = {};
        RenderPass triangle = {};
    } passes;
    uint32_t visibleObjectCount{};

    static const uint32_t TRIANGLE_DESCRIPTOR_ID = 100;
public:

    mat4 perspective_revZ(float fovy, float aspect, float zNear, float zFar)
    {

        mat4 Result = glm::perspective(fovy, aspect, zFar,zNear);

        return Result;
    }

    virtual void on_update_gui() override
    { 
        static const char* items[] = { "Off","MSAAx2","MSAAx4","MSAAx8" };
        static const char* current_msaa_item = items[static_cast<size_t>(settings.msaaSamples)-1];

        static const VkSampleCountFlagBits smaaBits[] = { VK_SAMPLE_COUNT_1_BIT,VK_SAMPLE_COUNT_2_BIT,VK_SAMPLE_COUNT_4_BIT,VK_SAMPLE_COUNT_8_BIT };

        ImGui::Text("Fps: %.2f", fps);
        ImGui::Text("ViewOrg: X: %.3f  Y: %.3f  Z: %.3f", camera.Position.x, camera.Position.y, camera.Position.z);
        ImGui::Text("obj in frustum: %d", visibleObjectCount);
        ImGui::Text("maxZ: %.2f, minZ: %.2f", maxZ, minZ);
        ImGui::DragFloat3("Light pos", &passData.vLightPos[0], 0.05f, -20.0f, 20.0f);
        ImGui::ColorPicker3("LightColor", &passData.vLightColor[0]);
        ImGui::DragFloat("Light intensity", &passData.vLightColor[3], 0.5f, 0.0f, 10000.0f);
        ImGui::DragFloat("Light range", &passData.vLightPos[3], 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Exposure", &postProcessData.fExposure, 0.01f, 1.0f, 50.0f);
        ImGui::Checkbox("Fog On/Off", &fogEnabled);
        ImGui::DragFloat("Fog density", &postProcessData.vFogParams.x, 0.001f, 0.0f, 10.0f, "%.4f");
        ImGui::DragFloat("Fog scale", &postProcessData.vFogParams.y, 0.001, 0.001f, 1.0f, "%.4f");
        ImGui::DragFloat3("Sun dir", &postProcessData.vSunPos[0], 1.0f);
        if (ImGui::BeginCombo("MSAA", current_msaa_item, ImGuiComboFlags_HeightRegular))
        {
            for (int n = 0; n < IM_ARRAYSIZE(items); ++n)
            {
                bool isSelected = (current_msaa_item == items[n]);
                if (ImGui::Selectable(items[n], isSelected)) {
                    current_msaa_item = items[n];
                    settings.msaaSamples = smaaBits[n];
                    smaaChanged = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    virtual void on_window_resized() override;

    void setup_descriptor_sets();

    void setup_descriptor_pools();

    void setup_objects();
    void setup_samplers();


    App(bool b) : AppBase(b) {
        depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        settings.validation = b;
    }

    virtual ~App();

    virtual void build_command_buffers() override;

    virtual void render() override;

    void setup_debug_pipeline(RenderPass& pass);

    void setup_tonemap_pipeline(RenderPass& pass);

    void setup_triangle_pipeline(RenderPass& pass);
    void setup_preZ_pass();

    void setup_triangle_pass();
    void setup_tonemap_pass();
    void setup_images();

    void update_uniforms() {

        uboPassData[currentFrame].copyTo(0, sizeof(passData), &passData);
        uboPostProcessData[currentFrame].copyTo(0, sizeof(PostProcessData), &postProcessData);
    }

    virtual void prepare() override;

    virtual void get_enabled_features() override;

    virtual void get_enabled_extensions() override;

    void create_material_texture(const std::string& filename);
    bool load_texture2d(std::string filename, vkjs::Image* dest, bool autoMipmap, int& w, int& h, int& nchannel);
};

void demo()
{
    jsrlib::gLogWriter.SetFileName("vulkan_engine.log");

    vkjs::GraphicsPipeline::GraphicsShaderInfo shaders{};
    vkjs::GraphicsPipeline gp(shaders);
    VkDescriptorImageInfo img1;

    gp.bind_image(0, 1, img1);
    gp.bind_image(0, 3, img1);

    App* app = new App(true);
    app->settings.fullscreen = false;
    app->settings.exclusive = false;
    app->settings.vsync = true;
    app->settings.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    app->width = 1280;
    app->height = 720;
    
    if (app->init())
    {
        app->prepare();
        app->run();
    }

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
            std::array<VkDescriptorImageInfo,2> inf{};
            inf[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            inf[0].imageView = HDRImage[i].view;
            inf[0].sampler = sampNearestClampBorder;
            inf[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            inf[1].imageView = (settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? depthResolvedView[i] : deptOnlyView;
            inf[1].sampler = sampNearestClampBorder;
            std::array<VkWriteDescriptorSet, 2> write;
            write[0] = vks::initializers::writeDescriptorSet(HDRDescriptor[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &inf[0], 1);
            write[1] = vks::initializers::writeDescriptorSet(HDRDescriptor[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &inf[1], 1);
            
            vkUpdateDescriptorSets(d, 2, write.data(), 0, nullptr);
        }
    }
}

void App::setup_descriptor_sets()
{
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        
        //VkDescriptorBufferInfo drawbufInfo = uboDrawData.descriptor;
        VkDescriptorBufferInfo drawbufInfo = drawPool[i]->descriptor;
        drawbufInfo.range = sizeof(DrawData);

        uboPassData[i].descriptor.range = sizeof(PassData);
        uboPassData[i].descriptor.offset = 0;
        const VkShaderStageFlags stageBits = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        vkutil::DescriptorBuilder::begin(&descLayoutCache, &descAllocator)
            .bind_buffer(0, &uboPassData[i].descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(1, &drawbufInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_image(2, &ssaoNoise.descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(triangleDescriptors[i]);

        HDRImage[i].setup_descriptor();
        HDRImage[i].descriptor.sampler = sampNearestClampBorder;
        HDRImage[i].descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo zinf = {};
        zinf.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        zinf.imageView = (settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? depthResolvedView[i] : deptOnlyView;
        zinf.sampler = sampNearestClampBorder;

        uboPostProcessData[i].setup_descriptor();
        uboPostProcessData[i].descriptor.range = sizeof(PostProcessData);

        vkutil::DescriptorBuilder::begin(&descLayoutCache, &descAllocator)
            .bind_image(0, &HDRImage[i].descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(1, &zinf, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(2, &uboPostProcessData[i].descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(HDRDescriptor[i]);

        device->set_descriptor_set_name(HDRDescriptor[i], "Tonemap DSET " + std::to_string(i));
    }

}

void App::setup_descriptor_pools()
{
    descAllocator.init(*device);
    descLayoutCache.init(*device);
}

void App::setup_objects()
{
    std::vector<int> nodesToProcess = world->scene.rootNodes;
    objects.clear();

    std::random_device rdev;
    std::default_random_engine reng(rdev());
    std::uniform_real_distribution<float> range(0.0f, 0.25f);

    dynamicAlignment = sizeof(DrawData);
    if (minUboAlignment > 0) {
        dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }

    drawDataStruct.clear();
    while (nodesToProcess.empty() == false)
    {
        int idx = nodesToProcess.back();
        nodesToProcess.pop_back();

        auto& node = world->scene.nodes[idx];
        mat4 mtxModel = node.getTransform();
        if (node.isMesh()) {
            for (auto& e : node.getEntities()) {
                Object obj;
                obj.mesh = e;
                obj.mtxModel = mtxModel;
                obj.aabb = world->meshes[e].aabb.Transform(mtxModel);
                obj.vkResources = materials[world->meshes[e].material].resources;

                drawDataStruct.emplace_back(DrawData{ mtxModel,
                    mat4(transpose(inverse(mat3(mtxModel)))),
                    vec4(range(reng), range(reng), range(reng), 1.0f) });
                objects.push_back(obj);
            }
        }
        for (const auto& e : node.getChildren()) {
            nodesToProcess.push_back(e);
        }
    }
    
    drawData.resize(drawDataStruct.size() * dynamicAlignment);

    size_t offset = 0;
    for (size_t i(0); i < drawDataStruct.size(); ++i) {
        memcpy(&drawData[offset], &drawDataStruct[i], sizeof(DrawData));
        offset += dynamicAlignment;
    }

//    memcpy(drawData.data(), drawDataStruct.data(), drawDataStruct.size() * sizeof(DrawData));
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
    samplerCI.minLod = -1000.f;
    samplerCI.maxLod = 1000.f;
    VK_CHECK(device->create_sampler(samplerCI, &sampLinearRepeat));
    device->set_object_name((uint64_t)sampLinearRepeat, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, "Linear-Repeat Samp");

    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.magFilter = VK_FILTER_NEAREST;
    samplerCI.minFilter = VK_FILTER_NEAREST;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 0.0f;
    samplerCI.anisotropyEnable = VK_FALSE;
    VK_CHECK(device->create_sampler(samplerCI, &sampNearestClampBorder));
    device->set_object_name((uint64_t)sampNearestClampBorder, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, "Nearest-Border Samp");

}

App::~App()
{
    if (!d || !prepared) return;

    vkDeviceWaitIdle(d);

    std::array<RenderPass*, 3> rpasses = { &passes.preZ,&passes.tonemap,&passes.triangle };
    for (const auto* pass : rpasses) {
        if (pass->pipeline) vkDestroyPipeline(d, pass->pipeline, nullptr);
        if (pass->layout) vkDestroyPipelineLayout(d, pass->layout, nullptr);
    }
    if (debugPipeline) vkDestroyPipeline(d, debugPipeline, nullptr);
    if (debugPipelineLayout) vkDestroyPipelineLayout(d, debugPipelineLayout, nullptr);

    vkDestroyRenderPass(d, passes.tonemap.pass, nullptr);
    vkDestroyRenderPass(d, passes.preZ.pass, nullptr);
    vkDestroyRenderPass(d, passes.triangle.pass, nullptr);

    descLayoutCache.cleanup();
    descAllocator.cleanup();

    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        vkDestroyFramebuffer(d, fb[i], 0);
        vkDestroyFramebuffer(d, HDRFramebuffer[i], 0);
        vkDestroyImageView(d, depthResolvedView[i], 0);
    }
}

void App::build_command_buffers()
{
    const bool MSAA_ENABLED = (settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT);

    VkCommandBuffer cmd = drawCmdBuffers[currentFrame];
    const VkDeviceSize offset = 0;

    drawPool[currentFrame]->reset();

    device->begin_debug_marker_region(cmd, vec4(1.f, .5f, 0.f, 1.f), "Forward Pass");
    VkRenderPassBeginInfo beginPass = vks::initializers::renderPassBeginInfo();
    VkClearValue clearVal[3];
    glm::vec4 sky = glm::vec4{ 0.5f, 0.6f, 0.7f, 1.0f };

    clearVal[0].color = { sky.r,sky.g,sky.b,sky.a };
    clearVal[1].color = { 0.0f,0.0f,0.0f,0.0f };
    clearVal[2].depthStencil.depth = 0.0f;
    //clearVal[3].color = { 0.0f,0.0f,0.0f,0.0f };
    //clearVal[4].color = { 0.0f,0.0f,0.0f,0.0f };

    beginPass.clearValueCount = 3;
    beginPass.pClearValues = &clearVal[0];
    beginPass.renderPass = passes.triangle.pass;
    beginPass.framebuffer = HDRFramebuffer[currentFrame];
    beginPass.renderArea.extent = swapchain.vkb_swapchain.extent;
    beginPass.renderArea.offset = { 0,0 };

    HDRImage_MS[currentFrame].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.triangle.pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vtxbuf.buffer, &offset);
    vkCmdBindIndexBuffer(cmd, idxbuf.buffer, 0ull, VK_INDEX_TYPE_UINT16);
    VkViewport viewport{ 0.f,0.f,float(width),float(height),0.0f,1.0f };
    VkRect2D scissor{ 0,0,width,height };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const mat4 vp = passData.mtxProjection * passData.mtxView;
    jsr::Frustum frustum(vp);

    uint32_t dynOffset = 0;
    size_t transientVtxOffset = 0;

    vkjs::Vertex v{};

    visibleObjectCount = 0;
    uint32_t objIdx = 0;


    for (const auto& obj : objects) {
        const auto& mesh = meshes[obj.mesh];
        const int materialIndex = world->meshes[obj.mesh].material;
        const auto& material = world->materials[materialIndex];

        const bool visible = frustum.Intersects2(obj.aabb);

        if (visible && material.alphaMode != ALPHA_MODE_BLEND) {
        
            visibleObjectCount++;

            auto corners = obj.aabb.GetHomogenousCorners();
            vec4 pp[8];

            for (uint32_t i = 0; i < 8; ++i) {
                vec4 p = vp * corners[i];
                p /= p.w;
                v.xyz = p;
                assert((transientVtxOffset + sizeof(vkjs::Vertex)) < vtxStagingBuffer.size);
                vtxStagingBuffer.copyTo(transientVtxOffset, sizeof(vkjs::Vertex), &v);
                transientVtxOffset += sizeof(vkjs::Vertex);
            }

            auto bufferDescr = drawPool[currentFrame]->Allocate<DrawData>(1, &drawDataStruct[objIdx]);
            const VkDescriptorSet dsets[2] = { triangleDescriptors[currentFrame], obj.vkResources };
            //vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.triangle.layout, 0, 2, dsets, 1, &dynOffset);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.triangle.layout, 0, 2, dsets, 1, (uint32_t*)(&bufferDescr.offset));

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.firstIndex, mesh.firstVertex, objIdx);
        }
        dynOffset += dynamicAlignment;
        objIdx++;
    }
    vkCmdEndRenderPass(cmd);
    device->end_debug_marker_region(cmd);

    if (transientVtxOffset > 0)
    {
        VkBufferCopy copy;
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = transientVtxOffset;
        vkCmdCopyBuffer(cmd, vtxStagingBuffer.buffer, transientVtxBuf[currentFrame].buffer, 1u, &copy);
        VkMemoryBarrier vkmb = vks::initializers::memoryBarrier();
        vkmb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkmb.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &vkmb, 0, nullptr, 0, nullptr);
    }

/*
    HDRImage[currentFrame].record_change_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
*/
    HDRImage_MS[currentFrame].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    HDR_NormalImage_MS[currentFrame].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    device->begin_debug_marker_region (cmd, vec4(.5f, 1.f, .5f, 1.f), "PostProcess Pass");

    clearVal[0].color = { 0.f,0.f,0.f,1.f };
    beginPass.clearValueCount = 1;
    beginPass.renderPass = passes.tonemap.pass;
    beginPass.framebuffer = fb[currentFrame];
    //vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);

    // New structures are used to define the attachments used in dynamic rendering
    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView = swapchain.views[currentBuffer];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,1.0f };
    
    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea = { 0, 0, width, height };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = 0;
    renderingInfo.pStencilAttachment = 0;

    if (swapchain_images[currentBuffer].layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        swapchain_images[currentBuffer].record_change_layout(cmd,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
        swapchain_images[currentBuffer].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    // Begin dynamic rendering
    vkCmdBeginRenderingKHR(cmd, &renderingInfo);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.tonemap.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.tonemap.layout, 0, 1, &HDRDescriptor[currentFrame], 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
#if 1
    if (visibleObjectCount > 0)
    {
        vkCmdBindVertexBuffers(cmd, 0, 1, &transientVtxBuf[currentFrame].buffer, &offset);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipeline);
        vkCmdDraw(cmd, (transientVtxOffset / sizeof(vkjs::Vertex)), 1, 0, 0);
    }
#endif
    //vkCmdEndRenderPass(cmd);
    // End dynamic rendering
    vkCmdEndRenderingKHR(cmd);

    device->end_debug_marker_region(cmd);
}

void App::render()
{
    static float lastTime = 0.f;
    static uint32_t lastFrame = 0;

#if 0
    if (!firstRun) 
    {
        quit = true;
        return;
    }
#endif
    float time = ((float)SDL_GetTicks64());

    if ((time - lastTime) >= 500.f) {
        float frames = (float)(frameCounter - lastFrame);
        fps = frames / ((time - lastTime) / 1000.f) ;
        lastTime = time;
        lastFrame = frameCounter;
    }

    if (smaaChanged) {
        vkDeviceWaitIdle(d);
        setup_triangle_pass();
        setup_triangle_pipeline(passes.triangle);
        setup_depth_stencil();
        on_window_resized();
        smaaChanged = false;
    }

    const float zFar = 60000.0f;
    const float zNear = 0.1f;
    camera.MovementSpeed = .002f;
    passData.mtxView = camera.GetViewMatrix();
    passData.mtxProjection = perspective(radians(camera.Zoom), (float)width / height, zFar, zNear);
    passData.vScaleBias.x = 1.0f / swapchain.extent().width;
    passData.vScaleBias.y = 1.0f / swapchain.extent().height;
    passData.vScaleBias.z = 0.f;
    passData.vScaleBias.w = 0.f;

    postProcessData.mtxInvProj = inverse(passData.mtxProjection * passData.mtxView);
    postProcessData.fZnear = zNear;
    postProcessData.fZfar = zFar;
    postProcessData.vCameraPos = vec4(camera.Position, 1.0f);
    postProcessData.vFogParams.z = fogEnabled ? 1.0f : 0.0f;
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

    firstRun = false;

    swapchain_images[currentBuffer].layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

void App::setup_debug_pipeline(RenderPass& pass)
{
    auto vertexInput = vkjs::Vertex::vertex_input_description_position_only();

    const std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };
    vkjs::PipelineBuilder pb = {};
    pb._rasterizer = vks::initializers::pipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    pb._rasterizer.lineWidth = 1.0f;

    pb._depthStencil = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
    pb._dynamicStates = vks::initializers::pipelineDynamicStateCreateInfo(dynStates);
    pb._inputAssembly = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0, VK_FALSE);
    pb._multisampling = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    pb._vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pb._vertexInputInfo = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInput.bindings, vertexInput.attributes);

    pb._colorBlendAttachments.push_back(vks::initializers::pipelineColorBlendAttachmentState(0x0f, VK_FALSE));

    vkjs::ShaderModule vert_module(*device);
    vkjs::ShaderModule frag_module(*device);
    const fs::path vert_spirv_filename = basePath / "shaders/debug.vert.spv";
    const fs::path frag_spirv_filename = basePath / "shaders/debug.frag.spv";
    VK_CHECK(vert_module.create(vert_spirv_filename));
    VK_CHECK(frag_module.create(frag_spirv_filename));

    pb._shaderStages.resize(2);
    pb._shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pb._shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pb._shaderStages[0].pName = "main";
    pb._shaderStages[0].module = vert_module.module();
    pb._shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pb._shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pb._shaderStages[1].pName = "main";
    pb._shaderStages[1].module = frag_module.module();

    VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo();
    plci.pSetLayouts = VK_NULL_HANDLE;
    plci.setLayoutCount = 0;
    VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &debugPipelineLayout));

    pb._pipelineLayout = debugPipelineLayout;

    // New create info to define color, depth and stencil attachments at pipeline create time
    VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain.vkb_swapchain.image_format;
    pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    // Chain into the pipeline creat einfo
    pb._pNext = &pipelineRenderingCreateInfo;

    debugPipeline = pb.build_pipeline(d, /*pass.pass*/ VK_NULL_HANDLE);

    assert(debugPipeline);
    device->set_object_name((uint64_t)debugPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Debug pipeline");

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

    pb._colorBlendAttachments.push_back(vks::initializers::pipelineColorBlendAttachmentState(0x0f, VK_FALSE));

    vkjs::ShaderModule vert_module(*device);
    vkjs::ShaderModule frag_module(*device);
    const fs::path vert_spirv_filename = basePath / "shaders/triquad.vert.spv";
    const fs::path frag_spirv_filename = basePath / "shaders/post.frag.spv";
    VK_CHECK(vert_module.create(vert_spirv_filename));
    VK_CHECK(frag_module.create(frag_spirv_filename));

    pb._shaderStages.resize(2);
    pb._shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pb._shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pb._shaderStages[0].pName = "main";
    pb._shaderStages[0].module = vert_module.module();
    pb._shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pb._shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pb._shaderStages[1].pName = "main";
    pb._shaderStages[1].module = frag_module.module();

    /*
    layout:
        set 0:0 input sampler
    */

    std::vector<VkDescriptorSetLayoutBinding> set0bind(3);

    set0bind[0].binding = 0;
    set0bind[0].descriptorCount = 1;
    set0bind[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set0bind[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    set0bind[1].binding = 1;
    set0bind[1].descriptorCount = 1;
    set0bind[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set0bind[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    set0bind[2].binding = 2;
    set0bind[2].descriptorCount = 1;
    set0bind[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set0bind[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ds0ci = vks::initializers::descriptorSetLayoutCreateInfo(set0bind);
    
    tonemapLayout = descLayoutCache.create_descriptor_layout(&ds0ci);
    assert(tonemapLayout);

    VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo(1);
    plci.pSetLayouts = &tonemapLayout;
    VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &pass.layout));
    pb._pipelineLayout = pass.layout;

    // New create info to define color, depth and stencil attachments at pipeline create time
    VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain.vkb_swapchain.image_format;
    pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    // Chain into the pipeline creat einfo
    pb._pNext = &pipelineRenderingCreateInfo;

    pass.pipeline = pb.build_pipeline(d, /*pass.pass*/VK_NULL_HANDLE);

    assert(pass.pipeline);
    device->set_object_name((uint64_t)pass.pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Tonemap pipeline");
}

void App::setup_triangle_pipeline(RenderPass& pass)
{
    if (pass.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(d, pass.pipeline, 0);
        vkDestroyPipelineLayout(d, pass.layout, 0);
        pass.pipeline = VK_NULL_HANDLE;
        pass.layout = VK_NULL_HANDLE;
    }

    auto vertexInput = vkjs::Vertex::vertex_input_description();
    const std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };

    vkjs::PipelineBuilder pb = {};
    pb._rasterizer = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    pb._depthStencil = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pb._dynamicStates = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStates);
    pb._inputAssembly = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    pb._multisampling = vks::initializers::pipelineMultisampleStateCreateInfo(settings.msaaSamples, 0);
    pb._multisampling.minSampleShading = .2f;
    pb._multisampling.sampleShadingEnable = settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT ? VK_TRUE : VK_FALSE;

    pb._vertexInputInfo = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInput.bindings, vertexInput.attributes);

    vkjs::ShaderModule vert_module(*device);
    vkjs::ShaderModule frag_module(*device);
    VK_CHECK(vert_module.create(basePath / "shaders/triangle_v2.vert.spv"));
    VK_CHECK(frag_module.create(basePath / "shaders/triangle_v2.frag.spv"));

    auto merged_sets = vkjs::extract_descriptor_set_layout_data({ &frag_module,&vert_module });
    
    //assert((merged_sets[0].binding_typename[1] == "rb_DrawData"));
    //merged_sets[0].bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    
    pb._shaderStages.resize(2);
    pb._shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pb._shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pb._shaderStages[0].pName = "main";
    pb._shaderStages[0].module = vert_module.module();
    pb._shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pb._shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pb._shaderStages[1].pName = "main";
    pb._shaderStages[1].module = frag_module.module();


    /*
 layout:
 set 0
     0: ubo
     1: ssbo
     2: combined image
 */

    std::vector<VkDescriptorSetLayout> dsls;
    for (const auto& dsl : merged_sets)
    {
        dsls.push_back(descLayoutCache.create_descriptor_layout(&dsl.create_info));
    }

    VkPipelineLayoutCreateInfo plci = vks::initializers::pipelineLayoutCreateInfo((uint32_t)dsls.size());
    plci.pSetLayouts = dsls.data();
    VK_CHECK(vkCreatePipelineLayout(d, &plci, nullptr, &pass.layout));
    pb._pipelineLayout = pass.layout;

    VkPipelineColorBlendAttachmentState blend0 = vks::initializers::pipelineColorBlendAttachmentState(
        0x0f, VK_FALSE);

    pb._colorBlendAttachments.push_back(blend0);
    pb._colorBlendAttachments.push_back(blend0);

    pass.pipeline = pb.build_pipeline(d, pass.pass);
    assert(pass.pipeline);

    device->set_object_name((uint64_t) pass.pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Forward pipeline");

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
    if (passes.triangle.pass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(d, passes.triangle.pass, 0);
        passes.triangle.pass = VK_NULL_HANDLE;
    }

    VkRenderPassCreateInfo2 rpci = vks::initializers::renderPassCreateInfo2();
    const bool msaaEnabled = (settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT);

    VkAttachmentDescription2 color = {};
    color.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = msaaEnabled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    color.format = HDR_RT_FMT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = msaaEnabled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    color.samples = settings.msaaSamples;

    VkAttachmentDescription2 colorResolve = {};
    colorResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    colorResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorResolve.format = HDR_RT_FMT;
    colorResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorResolve.samples = VK_SAMPLE_COUNT_1_BIT;

    VkAttachmentDescription2 colorNormal = {};
    colorNormal.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    colorNormal.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorNormal.finalLayout = msaaEnabled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorNormal.format = NORMAL_RT_FMT;
    colorNormal.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorNormal.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorNormal.samples = settings.msaaSamples;

    VkAttachmentDescription2 colorNormalResolve = {};
    colorNormalResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    colorNormalResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorNormalResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorNormalResolve.format = NORMAL_RT_FMT;
    colorNormalResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorNormalResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorNormalResolve.samples = VK_SAMPLE_COUNT_1_BIT;

    VkAttachmentDescription2 depth = {};
    depth.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = msaaEnabled ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depth.format = depth_format;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = msaaEnabled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.samples = settings.msaaSamples;

    VkAttachmentDescription2 depthResolve = {};
    depthResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
    depthResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depthResolve.format = depth_format;
    depthResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthResolve.samples = VK_SAMPLE_COUNT_1_BIT;


    std::array<VkSubpassDependency2, 3> deps = {};
    size_t dep = 0;
    deps[dep].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    deps[dep].dependencyFlags = 0;
    deps[dep].srcAccessMask = VK_ACCESS_NONE;
    deps[dep].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    deps[dep].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    deps[dep].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[dep].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[dep].dstSubpass = 0;
    ++dep;
    deps[dep].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    deps[dep].dependencyFlags = 0;
    deps[dep].srcAccessMask = VK_ACCESS_NONE;
    deps[dep].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[dep].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[dep].dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[dep].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[dep].dstSubpass = 0;
    ++dep;
    deps[dep].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
    deps[dep].dependencyFlags = 0;
    deps[dep].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[dep].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[dep].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[dep].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[dep].srcSubpass = 0;
    deps[dep].dstSubpass = VK_SUBPASS_EXTERNAL;


    VkAttachmentReference2 colorRef = vks::initializers::attachmentReference2();
    colorRef.attachment = 0; 
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkAttachmentReference2 NormalRef = vks::initializers::attachmentReference2();
    NormalRef.attachment = 1;
    NormalRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    NormalRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkAttachmentReference2 ZRef = vks::initializers::attachmentReference2(); 
    ZRef.attachment = 2;
    ZRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    ZRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    VkAttachmentReference2 colorResolveRef = vks::initializers::attachmentReference2();
    colorResolveRef.attachment = 3;
    colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorResolveRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkAttachmentReference2 normalResolveRef = vks::initializers::attachmentReference2();
    normalResolveRef.attachment = 4;
    normalResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    normalResolveRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
 
    VkAttachmentReference2 ZResolveRef = vks::initializers::attachmentReference2();
    ZResolveRef.attachment = 5;
    ZResolveRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    ZResolveRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    const std::array<VkAttachmentDescription2,6> attachments = { color,colorNormal,depth,colorResolve, colorNormalResolve, depthResolve };
    const std::array<VkAttachmentReference2,2> colorRefs = { colorRef,NormalRef };
    const std::array<VkAttachmentReference2,2> resolveRefs = { colorResolveRef,normalResolveRef };

    VkSubpassDescriptionDepthStencilResolve depthResolveDesc = {};
    depthResolveDesc.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
    depthResolveDesc.depthResolveMode = VK_RESOLVE_MODE_MAX_BIT;
    depthResolveDesc.pDepthStencilResolveAttachment = &ZResolveRef;

    const uint32_t attachmentCount = msaaEnabled ? 6 : 3;

    VkSubpassDescription2 subpass0 = {};
    subpass0.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass0.colorAttachmentCount = (uint32_t) colorRefs.size();
    subpass0.pColorAttachments = colorRefs.data();
    subpass0.pDepthStencilAttachment = &ZRef;

    if (msaaEnabled) {
        subpass0.pResolveAttachments = resolveRefs.data();
        subpass0.pNext = &depthResolveDesc;
    }

    rpci.attachmentCount = attachmentCount;
    rpci.pAttachments = attachments.data();
    rpci.flags = 0;
    rpci.dependencyCount = (uint32_t) deps.size();
    rpci.pDependencies = deps.data();
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass0;

    VK_CHECK(vkCreateRenderPass2(d, &rpci, nullptr, &passes.triangle.pass));
    device->set_object_name((uint64_t)passes.triangle.pass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Forward pass");

}

void App::setup_tonemap_pass()
{
    VkRenderPassCreateInfo rpci = vks::initializers::renderPassCreateInfo();

    VkAttachmentDescription color = {};
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.format = swapchain.vkb_swapchain.image_format;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
    dep0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep0.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep0.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep0.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep0.dstSubpass = 0;
    VkSubpassDependency dep1 = {};
    dep1.dependencyFlags = 0;
    dep1.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep1.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep1.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep1.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep1.dstSubpass = VK_SUBPASS_EXTERNAL;
    dep1.srcSubpass = 0;
    std::array<VkSubpassDependency, 2> deps = { dep0,dep1 };

    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.flags = 0;
    rpci.dependencyCount = (uint32_t)deps.size();
    rpci.pDependencies = deps.data();
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass0;

    VK_CHECK(vkCreateRenderPass(*device, &rpci, nullptr, &passes.tonemap.pass));
    device->set_object_name((uint64_t) passes.tonemap.pass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Tonemap pass");

}

void App::setup_images()
{
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        if (HDRFramebuffer[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(*device, HDRFramebuffer[i], nullptr);
        }
        if (depthResolved[i].image != VK_NULL_HANDLE) {
            device->destroy_image(&depthResolved[i]);
            vkDestroyImageView(d, depthResolvedView[i], nullptr);
        }
        if (HDRImage_MS[i].image != VK_NULL_HANDLE) {
            device->destroy_image(&HDRImage_MS[i]);
        }
        if (HDR_NormalImage_MS[i].image != VK_NULL_HANDLE) {
            device->destroy_image(&HDR_NormalImage_MS[i]);
        }
        if (HDR_NormalImage[i].image != VK_NULL_HANDLE) {
            device->destroy_image(&HDR_NormalImage[i]);
        }

        VK_CHECK(device->create_depth_stencil_attachment(depth_format,
            swapchain.extent(),
            VK_SAMPLE_COUNT_1_BIT,
            &depthResolved[i]));

        device->set_image_name(&depthResolved[i], "Resolved Depth Img");

        VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
        view.format = depth_format;
        view.image = depthResolved[i].image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view.subresourceRange.layerCount = 1;
        view.subresourceRange.levelCount = 1;
        VK_CHECK( vkCreateImageView(d, &view, nullptr, &depthResolvedView[i]) );
        device->set_object_name((uint64_t)depthResolvedView[i], VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Resolved Depth View");

        VK_CHECK(device->create_color_attachment(HDR_RT_FMT,
            swapchain.extent(),
            settings.msaaSamples,
            &HDRImage_MS[i]));
        VK_CHECK(device->create_color_attachment(HDR_RT_FMT,
            swapchain.extent(),
            VK_SAMPLE_COUNT_1_BIT,
            &HDRImage[i]));
        VK_CHECK(device->create_color_attachment(NORMAL_RT_FMT,
            swapchain.extent(),
            settings.msaaSamples,
            &HDR_NormalImage_MS[i]));
        VK_CHECK(device->create_color_attachment(NORMAL_RT_FMT,
            swapchain.extent(),
            VK_SAMPLE_COUNT_1_BIT,
            &HDR_NormalImage[i]));

        device->set_image_name(&HDRImage_MS[i], "HDR MS Img");
        device->set_image_name(&HDRImage[i], "Resolved HDR Img");
        device->set_image_name(&HDR_NormalImage_MS[i], "Normal map MS Img");
        device->set_image_name(&HDR_NormalImage[i], "Normal map Img");

        device->set_object_name((uint64_t)HDRImage_MS[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "HDR MS View");
        device->set_object_name((uint64_t)HDRImage[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Resolved HDR View");
        device->set_object_name((uint64_t)HDR_NormalImage_MS[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Normal map View");
        device->set_object_name((uint64_t)HDR_NormalImage[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Resolved Normal map View");

        //depthResolved[i].change_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        device->set_image_name(&HDRImage_MS[i], "Forward Color Attachment " + std::to_string(i));
        device->set_image_name(&HDR_NormalImage_MS[i], "Forward Normal Attachment " + std::to_string(i));

        std::vector<VkImageView> targets;
        if (settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT)
        {
            targets = {
                HDRImage_MS[i].view,
                HDR_NormalImage_MS[i].view,
                depth_image.view,
                HDRImage[i].view,
                HDR_NormalImage[i].view,
                depthResolved[i].view
            };
        }
        else {
            targets = {
                HDRImage[i].view,
                HDR_NormalImage[i].view,
                depth_image.view
            };
        }

        auto fbci = vks::initializers::framebufferCreateInfo();
        fbci.renderPass = passes.triangle.pass;
        fbci.layers = 1;
        fbci.attachmentCount = static_cast<uint32_t>(targets.size());
        fbci.pAttachments = targets.data();
        fbci.width = HDRImage_MS[i].extent.width;
        fbci.height = HDRImage_MS[i].extent.height;
        VK_CHECK(vkCreateFramebuffer(d, &fbci, 0, &HDRFramebuffer[i]));

        if (ssaoNoise.image == VK_NULL_HANDLE)
        {
            VK_CHECK(device->create_texture2d(VK_FORMAT_R16G16_SFLOAT, VkExtent3D{ 4,4,1 }, &ssaoNoise));
            std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
            std::default_random_engine generator;
            std::vector<glm::uint> ssaoNoise;
            for (unsigned int i = 0; i < 16; i++)
            {
                glm::uint noise = glm::packHalf2x16(glm::vec2(
                    randomFloats(generator) * 2.0 - 1.0,
                    randomFloats(generator) * 2.0 - 1.0));

                ssaoNoise.push_back(noise);
            }
            vkjs::Buffer tmpbuf;
            device->create_staging_buffer(16 * 4, &tmpbuf);
            tmpbuf.copyTo(0, 16 * 4, &ssaoNoise[0]);
            this->ssaoNoise.upload(VkExtent3D{ 4,4,1 }, 0, 0, 0, 0, &tmpbuf);
            device->destroy_buffer(&tmpbuf);
            this->ssaoNoise.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            this->ssaoNoise.setup_descriptor();
            this->ssaoNoise.descriptor.sampler = sampNearestClampBorder;
        }
        //HDRImage[i].change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}


void App::prepare()
{
    vkjs::AppBase::prepare();

    setup_descriptor_pools();
    setup_samplers();

    // Calculate required alignment based on minimum device offset alignment
    minUboAlignment = device->vkbPhysicalDevice.properties.limits.minUniformBufferOffsetAlignment;
    drawDataBufferSize = 1024 * align_size(sizeof(DrawData), minUboAlignment);
    camera.MovementSpeed = 0.003f;
    passData.vLightPos = glm::vec4(0.f, 1.5f, 0.f, 10.f);
    passData.vLightColor = glm::vec4(0.800f, 0.453f, 0.100f, 15.f);
    postProcessData.vSunPos = { 100.0f, -100.0f, -100.0f, 0.0f };
    d = *device;
    int w, h, nc;

    setup_triangle_pass();
    setup_triangle_pipeline(passes.triangle);
    setup_tonemap_pass();
    setup_tonemap_pipeline(passes.tonemap);
    setup_debug_pipeline(passes.tonemap);

    setup_images();

    device->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 128ULL * 1024 * 1024, &vtxbuf);
    device->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 128ULL * 1024 * 1024, &idxbuf);

    device->create_staging_buffer(static_cast<VkDeviceSize>(1 * 1024) * 1024, &vtxStagingBuffer);

    device->set_buffer_name(&vtxbuf, "Vertex Buffer");
    device->set_buffer_name(&idxbuf, "Index Buffer");

    if (!load_texture2d("../../resources/images/uv_checker_large.png", &uvChecker, true, w, h, nc))
    {
        device->create_texture2d(VK_FORMAT_R8G8B8A8_SRGB, { 1,1,1 }, &uvChecker);
        uvChecker.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    device->set_image_name(&uvChecker, "UV-Map debug image");

    //uvChecker.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    uvChecker.setup_descriptor();
    uvChecker.descriptor.sampler = sampLinearRepeat;

    vkjs::Buffer* drawBuf = new vkjs::Buffer();
    device->create_uniform_buffer(drawDataBufferSize * MAX_CONCURRENT_FRAMES, false, drawBuf); drawBuf->map();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
    {
        VK_CHECK(device->create_buffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            1ULL * 1024 * 1024, &transientVtxBuf[i]));

        device->create_uniform_buffer(8 * 1024, false, &uboPassData[i]);
        uboPassData[i].map();
        device->set_buffer_name(&uboPassData[i], "PerPass UBO " + std::to_string(i));

        device->create_uniform_buffer(sizeof(PostProcessData), false, &uboPostProcessData[i]);
        uboPostProcessData[i].map();
        device->set_buffer_name(&uboPostProcessData[i], "PostProcData UBO " + std::to_string(i));
        
        drawPool[i] = new UniformBufferPool(drawBuf, drawDataBufferSize, i * drawDataBufferSize, minUboAlignment);
    }

    world = std::make_unique<World>();

    struct S_Scene {
        fs::path dir;
        std::string file;
        S_Scene(const fs::path& a, const std::string& b) : dir(a), file(b) {}
    };

    std::array<S_Scene, 3> scenes = {
        S_Scene{fs::path("../../resources/models/sponza"), "sponza_j.gltf"},
        S_Scene{fs::path("../../resources/models/sw_venator"), "scene.gltf"},
        S_Scene{fs::path("D:/DATA/models/crq376zqdkao-Castelia-City/OBJ"), "city.gltf"}
    };

    const int sceneIdx = 1;
    auto scenePath = scenes[sceneIdx].dir;
    jsr::gltfLoadWorld(scenePath / scenes[sceneIdx].file, *world);

    std::vector<vkjs::Vertex> vertices;
    std::vector<uint16_t> indices;

    uint32_t firstIndex(0);
    uint32_t firstVertex(0);

    materials.resize(world->materials.size());

    int i = 0;
    jsrlib::Info("Loading images...");
    for (auto& it : world->materials)
    {
        std::string fname[3];
        fname[0] = (scenePath / it.texturePaths.albedoTexturePath).u8string();
        fname[1] = (scenePath / it.texturePaths.normalTexturePath).u8string();
        fname[2] = (scenePath / it.texturePaths.specularTexturePath).u8string();

        for (int i = 0; i < 3; ++i) {
            while (true) {
                auto found = fname[i].find("%20");
                if (found == std::string::npos) {
                    break;
                }
                fname[i] = fname[i].replace(found, 3, " ");
            }
        }
        create_material_texture(fname[0]);
        create_material_texture(fname[1]);
        create_material_texture(fname[2]);

        vkutil::DescriptorBuilder::begin(&descLayoutCache, &descAllocator)
            .bind_image(0, &imageCache.at(fname[0]).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(1, &imageCache.at(fname[1]).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(2, &imageCache.at(fname[2]).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(materials[i++].resources);
    }

    for (size_t i(0); i < world->meshes.size(); ++i)
    {
        auto& mesh = world->meshes[i];
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

    const size_t size = drawDataBufferSize * MAX_CONCURRENT_FRAMES;
    device->create_uniform_buffer(size, false, &uboDrawData);
    device->set_buffer_name(&uboDrawData, "DrawData UBO");
    vkjs::Buffer stage;
    device->create_staging_buffer(size, &stage);
    stage.copyTo(0, drawData.size(), drawData.data());
    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
    {
        device->buffer_copy(&stage, &uboDrawData, 0, i * drawDataBufferSize, drawData.size());
    }
    device->destroy_buffer(&stage);
    setup_descriptor_sets();

    const int kernelSize = sizeof(passData.avSSAOkernel) / sizeof(passData.avSSAOkernel[0]);
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;
    for (unsigned int i = 0; i < kernelSize; ++i)
    {
        glm::vec4 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator),
            0.0f
        );
        sample = glm::normalize(sample);
        float scale = (float)i / float(kernelSize);
        scale = jsrlib::lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        //sample *= randomFloats(generator);
        passData.avSSAOkernel[i] = sample;
    }

    postProcessData.fExposure = 1.0f;
    postProcessData.vFogParams = { 0.001f,0.0001f,0.0f,0.0f };
    /*
    postProcessData.fogColor = vec3(0.8f);
    postProcessData.fogEquation = 1;
    postProcessData.fogDensity = 0.01f;
    postProcessData.fogEnabled = 1;
    */
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
    enabled_features.sampleRateShading = VK_TRUE;

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
    enabled_device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    //enabled_device_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeaturesKHR{};
    dynamicRenderingFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeaturesKHR.dynamicRendering = VK_TRUE;
    required_generic_features.push_back(vkjs::GenericFeature(dynamicRenderingFeaturesKHR));
}

void App::create_material_texture(const std::string& filename)
{
    std::string fn = filename;

    if (imageCache.find(fn) == imageCache.end())
    {
        int w, h, nc;
        // load image
        vkjs::Image newImage;
        fs::path base = fs::path(fn).parent_path();
        fs::path name = fs::path(fn).filename();
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
        else if (!load_texture2d(fn, &newImage, true, w, h, nc))
        {
            jsrlib::Error("%s notfund", fn.c_str());
            device->create_texture2d(VK_FORMAT_R8G8B8A8_UNORM, { 1,1,1 }, &newImage);
            newImage.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else
        {
            jsrlib::Info("%s Loaded", fn.c_str());
        }
        newImage.setup_descriptor();
        newImage.descriptor.sampler = sampLinearRepeat;
        imageCache.insert({ fn,newImage });        
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
