#include "sample1.h"
#include "pch.h"

#include "jobsys.h"
#include "world.h"
#include "gltf_loader.h"
#include "stb_image.h"
#include "bounds.h"
#include "frustum.h"
#include "vkjs/vkjs.h"
#include "vkjs/appbase.h"
#include "vkjs/VulkanInitializers.hpp"
#include "vkjs/pipeline.h"
#include "vkjs/vkcheck.h"
#include "vkjs/vk_descriptors.h"
#include "vkjs/shader_module.h"
#include "vkjs/spirv_utils.h"
#include "ktx.h"
#include "ktxvulkan.h"
#include "vertex.h"

#include "imgui.h"
#include "jsrlib/jsr_logger.h"
#include "jsrlib/jsr_math.h"

#include <random>
//#define _USE_MATH_DEFINES
#include <cmath>

namespace fs = std::filesystem;
using namespace glm;
using namespace jsr;
using namespace jvk;

inline static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

constexpr float Rec4PI() { return 1.0f / (4.0f * M_PI); }

static float Watt2Candela(const float w) { return (w * 683.0f) * Rec4PI(); }
static float Watt2Lumen(const float w) { return (w * 683.0f); }

float computeEV100(float aperture, float shutterTime, float ISO)
{
    // EV number is defined as:
    // 2^ EV_s = N^2 / t and EV_s = EV_100 + log2 (S /100)
    // This gives
    // EV_s = log2 (N^2 / t)
    // EV_100 + log2 (S /100) = log2 (N^2 / t)
    // EV_100 = log2 (N^2 / t) - log2 (S /100)
    // EV_100 = log2 (N^2 / t . 100 / S)
    return log2((aperture*aperture) / shutterTime * 100.0 / ISO);
}

static float computeEV100FromAvgLuminance(float avgLuminance)
{
    // We later use the middle gray at 12.7% in order to have
    // a middle gray at 18% with a sqrt (2) room for specular highlights
    // But here we deal with the spot meter measuring the middle gray
    // which is fixed at 12.5 for matching standard camera
    // constructor settings (i.e. calibration constant K = 12.5)
    // Reference : http://en.wikipedia.org/wiki/Film_speed
    return log2(avgLuminance * 100.0 / 12.5);
}

static float convertEV100ToExposure(float EV100)
{
    // Compute the maximum luminance possible with H_sbs sensitivity
    // maxLum = 78 / ( S * q ) * N^2 / t
    // = 78 / ( S * q ) * 2^ EV_100
    // = 78 / (100 * 0.65) * 2^ EV_100
    // = 1.2 * 2^ EV
    // Reference: http://en.wikipedia.org/wiki/Film_speed
    float maxLuminance = 1.2 * pow(2.0, EV100);
    return 1.0 / maxLuminance;
}

void Sample1App::init_lights()
{
    std::random_device rdev;
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator(rdev());
    jvk::Buffer stage;

   
    /* Init ligths */
    lights = {};
    
    for (size_t i(0); i < lights.size(); ++i)
    {
        float x = -10.0f + randomFloats(generator) * 20.0f;
        float y =   0.0f + randomFloats(generator) * 9.0f;
        float z =  -3.0f + randomFloats(generator) * 6.0f;

        glm::vec3 pos{ x,y,z };
        //glm::vec3 pos{ 0.0f,3.0f,0.0f };
        glm::vec3 col{ randomFloats(generator) * 0.7,randomFloats(generator) * 0.7,randomFloats(generator) * 0.7 };
        float r = 0.65f + randomFloats(generator) * 0.2f;
        //glm::vec3 col{ 1.0f };
        lights[i].set_position(pos);
        lights[i].set_color(col);
        lights[i].intensity = /*Watt2Lumen*/(passData.vLightColor.w);
        lights[i].type = LightType_Point;

        float a = lights[i].intensity * Rec4PI();
        float Id = a / 0.02f;
        lights[i].range = range > 0.0f ? range : 0.1f * sqrtf(Id);
        lights[i].falloff = falloff;
        jsrlib::Info("Light %i: I=%f", i, lights[i].range);
    }
    pDevice->create_staging_buffer(lights.size() * sizeof(lights[0]), &stage);
    stage.copyTo(0, stage.size, lights.data());
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        //pDevice->buffer_copy(&stage, uboLights[ i ]->buffer(), 0, 0, stage.size);
        uboLights[i]->CopyTo(&stage, 0, 0, stage.size);
    }
    pDevice->destroy_buffer(&stage);
}

void Sample1App::on_window_resized()
{
    setup_images();
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i) {
        if (HDRDescriptor[i] != VK_NULL_HANDLE) {
            // update descript sets
            std::array<VkDescriptorImageInfo, 2> inf{};
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

void Sample1App::setup_descriptor_sets()
{
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {

        //VkDescriptorBufferInfo drawbufInfo = uboDrawData.descriptor;
        VkDescriptorBufferInfo drawbufInfo = drawPool[i]->descriptor;
        drawbufInfo.range = sizeof(DrawData);

        uboPassData[i]->GetBuffer()->descriptor.range = sizeof(PassData);
        uboPassData[i]->GetBuffer()->descriptor.offset = 0;
        const VkShaderStageFlags stageBits = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        uboLights[i]->SetupDescriptor();

        descMgr.builder()
            .bind_buffer(0, &uboPassData[i]->GetBuffer()->descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(1, &drawbufInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
            .bind_image(2, &ssaoNoise.descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(3, &uboLights[i]->GetBuffer()->descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(triangleDescriptors[i]);

        HDRImage[i].setup_descriptor();
        HDRImage[i].descriptor.sampler = sampNearestClampBorder;
        HDRImage[i].descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo zinf = {};
        zinf.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        zinf.imageView = (settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT) ? depthResolvedView[i] : deptOnlyView;
        zinf.sampler = sampNearestClampBorder;

        uboPostProcessData[i]->SetupDescriptor();
        uboPostProcessData[i]->GetBuffer()->descriptor.range = sizeof(PostProcessData);

        descMgr.builder()
            .bind_image(0, &HDRImage[i].descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_image(1, &zinf, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .bind_buffer(2, &uboPostProcessData[i]->GetBuffer()->descriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(HDRDescriptor[i]);

        pDevice->set_descriptor_set_name(HDRDescriptor[i], "Tonemap DSET " + std::to_string(i));
    }

}

void Sample1App::setup_descriptor_pools()
{
    descMgr.init(pDevice);
}

void Sample1App::setup_objects()
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
            for (auto e : node.getEntities()) {
                Object obj;
                obj.mesh = e;
                obj.mtxModel = mtxModel;
                obj.aabb = world->meshes[e].aabb.Transform(mtxModel);
                obj.vkResources = materials[world->meshes[e].material].resources;

                drawDataStruct.emplace_back(
                    DrawData
                    {
                        mtxModel,
                        mat4(transpose(inverse(mat3(mtxModel)))),
                        vec4(range(reng), range(reng), range(reng), 1.0f) 
                    });
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

void Sample1App::setup_samplers()
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
    VK_CHECK(pDevice->create_sampler(samplerCI, &sampLinearRepeat));
    pDevice->set_object_name((uint64_t)sampLinearRepeat, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, "Linear-Repeat Samp");

    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.magFilter = VK_FILTER_NEAREST;
    samplerCI.minFilter = VK_FILTER_NEAREST;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 0.0f;
    samplerCI.anisotropyEnable = VK_FALSE;
    VK_CHECK(pDevice->create_sampler(samplerCI, &sampNearestClampBorder));
    pDevice->set_object_name((uint64_t)sampNearestClampBorder, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, "Nearest-Border Samp");

}

Sample1App::~Sample1App()
{
    if (!d || !prepared) return;

    vkDeviceWaitIdle(d);

    std::array<RenderPass*, 3> rpasses = { &passes.preZ,&passes.tonemap,&passes.triangle };
    for (const auto* pass : rpasses)
    {
        if (pass->pPipeline) delete pass->pPipeline;
    }

    if (debugPipeline) vkDestroyPipeline(d, debugPipeline, nullptr);
    if (debugPipelineLayout) vkDestroyPipelineLayout(d, debugPipelineLayout, nullptr);

    vkDestroyRenderPass(d, passes.tonemap.pass, nullptr);
    vkDestroyRenderPass(d, passes.preZ.pass, nullptr);
    vkDestroyRenderPass(d, passes.triangle.pass, nullptr);

    //descLayoutCache.cleanup();
    //descAllocator.cleanup();

    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        vkDestroyFramebuffer(d, fb[i], 0);
        vkDestroyFramebuffer(d, HDRFramebuffer[i], 0);
        vkDestroyImageView(d, depthResolvedView[i], 0);
    }
    jsrlib::Info("Allocated descriptors: %d", vkutil::DescriptorAllocator::allocatedDescriptorCount);
}

void Sample1App::build_command_buffers()
{
    const bool MSAA_ENABLED = (settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT);

    VkCommandBuffer cmd = drawCmdBuffers[currentFrame];
    const VkDeviceSize offset = 0;

    drawPool[currentFrame]->reset();

    auto const col1 = vec4(1.f, .5f, 0.f, 1.f);
    pDevice->begin_debug_marker_region(cmd, &col1.r, "Forward Pass");
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

    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.triangle.pipeline);
    passes.triangle.pPipeline->bind(cmd);

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

    jsr::Vertex v{};

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
                v.set_position(&p.x);
                assert((transientVtxOffset + sizeof(jsr::Vertex)) < vtxStagingBuffer.size);
                vtxStagingBuffer.copyTo(transientVtxOffset, sizeof(jsr::Vertex), &v);
                transientVtxOffset += sizeof(jsr::Vertex);
            }

            auto bufferDescr = drawPool[currentFrame]->Allocate<DrawData>(1, &drawDataStruct[objIdx]);
            const VkDescriptorSet dsets[2] = { triangleDescriptors[currentFrame], obj.vkResources };
            passes.triangle.pPipeline->bind_descriptor_sets(cmd, 2, dsets, 1, (uint32_t*)(&bufferDescr.offset));

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.firstIndex, mesh.firstVertex, objIdx);
        }
        dynOffset += dynamicAlignment;
        objIdx++;
    }
    vkCmdEndRenderPass(cmd);
    pDevice->end_debug_marker_region(cmd);

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

    auto const col2 = vec4(.5f, 1.f, .5f, 1.f);
    pDevice->begin_debug_marker_region(cmd, &col2.r, "PostProcess Pass");

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
        swapchain_images[currentBuffer].record_layout_change(cmd,
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

    //vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.tonemap.pipeline);
    passes.tonemap.pPipeline->bind(cmd);
    passes.tonemap.pPipeline->bind_descriptor_sets(cmd, 1, &HDRDescriptor[currentFrame]);
    //vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, passes.tonemap.layout, 0, 1, &HDRDescriptor[currentFrame], 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
#if 0
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

    pDevice->end_debug_marker_region(cmd);
}

void Sample1App::render()
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
        fps = frames / ((time - lastTime) / 1000.f);
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

    // usage with auto settings
    float AutoEV100 = computeEV100FromAvgLuminance(fExposure);
    float exposure = convertEV100ToExposure(AutoEV100);

    postProcessData.fExposure = exposure;
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
    fbci.attachmentCount = (uint32_t)targets.size();
    fbci.pAttachments = targets.data();
    fbci.width = width;
    fbci.height = height;
    VK_CHECK(vkCreateFramebuffer(d, &fbci, 0, &fb[currentFrame]));

    build_command_buffers();

    firstRun = false;

    swapchain_images[currentBuffer].layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

void Sample1App::setup_debug_pipeline(RenderPass& pass)
{
    auto vertexInput = jsr::Vertex::vertex_input_description_position_only();

    const std::vector<VkDynamicState> dynStates = { VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_VIEWPORT };
    jvk::PipelineBuilder pb = {};
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

    jvk::ShaderModule vert_module(*pDevice);
    jvk::ShaderModule frag_module(*pDevice);
    const fs::path vert_spirv_filename = basePath / "shaders/bin/debug.vert.spv";
    const fs::path frag_spirv_filename = basePath / "shaders/bin/debug.frag.spv";
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
    pDevice->set_object_name((uint64_t)debugPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Debug pipeline");

}

void Sample1App::setup_tonemap_pipeline(RenderPass& pass)
{

    jvk::ShaderModule vert_module(*pDevice);
    jvk::ShaderModule frag_module(*pDevice);
    const fs::path vert_spirv_filename = basePath / "shaders/bin/triquad.vert.spv";
    const fs::path frag_spirv_filename = basePath / "shaders/bin/post.frag.spv";
    VK_CHECK(vert_module.create(vert_spirv_filename));
    VK_CHECK(frag_module.create(frag_spirv_filename));
    jvk::GraphicsShaderInfo shaders{};
    shaders.vert = &vert_module;
    shaders.frag = &frag_module;

    pass.pPipeline = new jvk::GraphicsPipeline(pDevice, shaders, &descMgr);

    // New create info to define color, depth and stencil attachments at pipeline create time
    VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain.vkb_swapchain.image_format;
    pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    // Chain into the pipeline create info
    pass.pPipeline->set_next_chain(&pipelineRenderingCreateInfo);
    pass.pPipeline->set_name("Tonemap pipeline");
    pass.pPipeline->set_cull_mode(VK_CULL_MODE_NONE)
        .set_depth_func(VK_COMPARE_OP_ALWAYS)
        .set_depth_mask(false)
        .set_depth_test(false)
        .set_render_pass(VK_NULL_HANDLE)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_draw_mode(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .set_sample_count(VK_SAMPLE_COUNT_1_BIT)
        .add_attachment_blend_state(vks::initializers::pipelineColorBlendAttachmentState(0x0f, VK_FALSE));

    VK_CHECK(pass.pPipeline->build_pipeline());
}

void Sample1App::setup_triangle_pipeline(RenderPass& pass)
{

    if (pass.pPipeline) delete pass.pPipeline;

    auto vertexInput = jsr::Vertex::vertex_input_description();

    jvk::ShaderModule vert_module(*pDevice);
    jvk::ShaderModule frag_module(*pDevice);
    VK_CHECK(vert_module.create(basePath / "shaders/bin/triangle_v2.vert.spv"));
    VK_CHECK(frag_module.create(basePath / "shaders/bin/triangle_v2.frag.spv"));

    jvk::GraphicsShaderInfo shaders = {};
    shaders.vert = &vert_module;
    shaders.frag = &frag_module;


    VkPipelineColorBlendAttachmentState blend0 = vks::initializers::pipelineColorBlendAttachmentState(0x0f, VK_FALSE);

    {
        pass.pPipeline = new jvk::GraphicsPipeline(pDevice, shaders, &descMgr);
        jvk::GraphicsPipeline& p = *pass.pPipeline;
        p.set_name("Forward pipeline");
        p.add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
            .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
            .set_cull_mode(VK_CULL_MODE_BACK_BIT)
            .set_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .set_draw_mode(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .set_polygon_mode(VK_POLYGON_MODE_FILL)
            .set_vertex_input_description(vertexInput)
            .set_sample_count(settings.msaaSamples)
            .set_sample_shading(settings.msaaSamples > VK_SAMPLE_COUNT_1_BIT, .2f)
            .add_attachment_blend_state(blend0)
            .add_attachment_blend_state(blend0)
            .set_render_pass(pass.pass)
            .set_depth_func(VK_COMPARE_OP_GREATER_OR_EQUAL)
            .set_depth_mask(true)
            .set_depth_test(true);

        VK_CHECK(p.build_pipeline());
    }

}

void Sample1App::setup_preZ_pass()
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

    VK_CHECK(vkCreateRenderPass(*pDevice, &rpci, nullptr, &passes.preZ.pass));
}

void Sample1App::setup_triangle_pass()
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
    deps[dep].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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

    const std::array<VkAttachmentDescription2, 6> attachments = { color,colorNormal,depth,colorResolve, colorNormalResolve, depthResolve };
    const std::array<VkAttachmentReference2, 2> colorRefs = { colorRef,NormalRef };
    const std::array<VkAttachmentReference2, 2> resolveRefs = { colorResolveRef,normalResolveRef };

    VkSubpassDescriptionDepthStencilResolve depthResolveDesc = {};
    depthResolveDesc.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
    depthResolveDesc.depthResolveMode = VK_RESOLVE_MODE_MAX_BIT;
    depthResolveDesc.pDepthStencilResolveAttachment = &ZResolveRef;

    const uint32_t attachmentCount = msaaEnabled ? 6 : 3;

    VkSubpassDescription2 subpass0 = {};
    subpass0.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass0.colorAttachmentCount = (uint32_t)colorRefs.size();
    subpass0.pColorAttachments = colorRefs.data();
    subpass0.pDepthStencilAttachment = &ZRef;

    if (msaaEnabled) {
        subpass0.pResolveAttachments = resolveRefs.data();
        subpass0.pNext = &depthResolveDesc;
    }

    rpci.attachmentCount = attachmentCount;
    rpci.pAttachments = attachments.data();
    rpci.flags = 0;
    rpci.dependencyCount = (uint32_t)deps.size();
    rpci.pDependencies = deps.data();
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass0;

    VK_CHECK(vkCreateRenderPass2(d, &rpci, nullptr, &passes.triangle.pass));
    pDevice->set_object_name((uint64_t)passes.triangle.pass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Forward pass");

}

void Sample1App::setup_tonemap_pass()
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

    VK_CHECK(vkCreateRenderPass(*pDevice, &rpci, nullptr, &passes.tonemap.pass));
    pDevice->set_object_name((uint64_t)passes.tonemap.pass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Tonemap pass");

}

void Sample1App::setup_images()
{
    for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
    {
        if (HDRFramebuffer[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(*pDevice, HDRFramebuffer[i], nullptr);
        }
        if (depthResolved[i].image != VK_NULL_HANDLE) {
            pDevice->destroy_image(&depthResolved[i]);
            vkDestroyImageView(d, depthResolvedView[i], nullptr);
        }
        if (HDRImage_MS[i].image != VK_NULL_HANDLE) {
            pDevice->destroy_image(&HDRImage_MS[i]);
        }
        if (HDR_NormalImage_MS[i].image != VK_NULL_HANDLE) {
            pDevice->destroy_image(&HDR_NormalImage_MS[i]);
        }
        if (HDR_NormalImage[i].image != VK_NULL_HANDLE) {
            pDevice->destroy_image(&HDR_NormalImage[i]);
        }

        VK_CHECK(pDevice->create_depth_stencil_attachment(depth_format,
            swapchain.extent(),
            VK_SAMPLE_COUNT_1_BIT,
            &depthResolved[i]));

        pDevice->set_image_name(&depthResolved[i], "Resolved Depth Img");

        VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
        view.format = depth_format;
        view.image = depthResolved[i].image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view.subresourceRange.layerCount = 1;
        view.subresourceRange.levelCount = 1;
        VK_CHECK(vkCreateImageView(d, &view, nullptr, &depthResolvedView[i]));
        pDevice->set_object_name((uint64_t)depthResolvedView[i], VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Resolved Depth View");

        VK_CHECK(pDevice->create_color_attachment(HDR_RT_FMT,
            swapchain.extent(),
            settings.msaaSamples,
            &HDRImage_MS[i]));
        VK_CHECK(pDevice->create_color_attachment(HDR_RT_FMT,
            swapchain.extent(),
            VK_SAMPLE_COUNT_1_BIT,
            &HDRImage[i]));
        VK_CHECK(pDevice->create_color_attachment(NORMAL_RT_FMT,
            swapchain.extent(),
            settings.msaaSamples,
            &HDR_NormalImage_MS[i]));
        VK_CHECK(pDevice->create_color_attachment(NORMAL_RT_FMT,
            swapchain.extent(),
            VK_SAMPLE_COUNT_1_BIT,
            &HDR_NormalImage[i]));

        pDevice->set_image_name(&HDRImage_MS[i], "HDR MS Img");
        pDevice->set_image_name(&HDRImage[i], "Resolved HDR Img");
        pDevice->set_image_name(&HDR_NormalImage_MS[i], "Normal map MS Img");
        pDevice->set_image_name(&HDR_NormalImage[i], "Normal map Img");

        pDevice->set_object_name((uint64_t)HDRImage_MS[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "HDR MS View");
        pDevice->set_object_name((uint64_t)HDRImage[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Resolved HDR View");
        pDevice->set_object_name((uint64_t)HDR_NormalImage_MS[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Normal map View");
        pDevice->set_object_name((uint64_t)HDR_NormalImage[i].view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Resolved Normal map View");

        //depthResolved[i].change_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        pDevice->set_image_name(&HDRImage_MS[i], "Forward Color Attachment " + std::to_string(i));
        pDevice->set_image_name(&HDR_NormalImage_MS[i], "Forward Normal Attachment " + std::to_string(i));

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
            VK_CHECK(pDevice->create_texture2d(VK_FORMAT_R16G16_SFLOAT, VkExtent3D{ 4,4,1 }, &ssaoNoise));
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
            jvk::Buffer tmpbuf;
            pDevice->create_staging_buffer(16 * 4, &tmpbuf);
            tmpbuf.copyTo(0, 16 * 4, &ssaoNoise[0]);
            this->ssaoNoise.upload(VkExtent3D{ 4,4,1 }, 0, 0, 0, 0, &tmpbuf);
            this->ssaoNoise.layout_change(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            this->ssaoNoise.setup_descriptor();
            this->ssaoNoise.descriptor.sampler = sampNearestClampBorder;
            pDevice->destroy_buffer(&tmpbuf);
        }
        //HDRImage[i].change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}


void Sample1App::prepare()
{
    jvk::AppBase::prepare();

    setup_descriptor_pools();
    setup_samplers();

    // Calculate required alignment based on minimum device offset alignment
    minUboAlignment = pDevice->vkbPhysicalDevice.properties.limits.minUniformBufferOffsetAlignment;
    drawDataBufferSize = 1024 * align_size(sizeof(DrawData), minUboAlignment);
    camera.MovementSpeed = 0.003f;
    passData.vLightPos = glm::vec4(0.f, 1.5f, 0.f, 10.f);
    passData.vLightColor = glm::vec4(0.800f, 0.453f, 0.100f, 2300.f);
    passData.vParams[0] = 0.05f;    // global ambient scale
    postProcessData.vSunPos = { 45.0f, 0.0f, 0.0f, 0.0f };
    postProcessData.fExposure = 250.f;
    postProcessData.bHDR = settings.hdr;
    postProcessData.tonemapper_P = 1000.0f;     // Max luminance
    postProcessData.tonemapper_a = 1.0f;        // contrast
    postProcessData.tonemapper_b = 0.0f;        // pedestal
    postProcessData.tonemapper_c = 1.33f;       // black
    postProcessData.tonemapper_m = 0.22f;      // linear section start
    postProcessData.tonemapper_l = 0.4f;       // linear section length
    postProcessData.tonemapper_s = 4.0f;        // scale
    postProcessData.saturation = 1.0f;
    d = *pDevice;
    int w, h, nc;

    setup_triangle_pass();
    setup_triangle_pipeline(passes.triangle);
    setup_tonemap_pass();
    setup_tonemap_pipeline(passes.tonemap);
    //setup_debug_pipeline(passes.tonemap);

    setup_images();

    pDevice->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 128ULL * 1024 * 1024, &vtxbuf);
    pDevice->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 128ULL * 1024 * 1024, &idxbuf);

    pDevice->create_staging_buffer(static_cast<VkDeviceSize>(1 * 1024) * 1024, &vtxStagingBuffer);

    pDevice->set_buffer_name(&vtxbuf, "Vertex Buffer");
    pDevice->set_buffer_name(&idxbuf, "Index Buffer");

    if (!load_texture2d("../../resources/images/uv_checker_large.png", &uvChecker, true, w, h, nc))
    {
        pDevice->create_texture2d(VK_FORMAT_R8G8B8A8_SRGB, { 1,1,1 }, &uvChecker);
        uvChecker.layout_change(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    pDevice->set_image_name(&uvChecker, "UV-Map debug image");

    //uvChecker.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    uvChecker.setup_descriptor();
    uvChecker.descriptor.sampler = sampLinearRepeat;

    jvk::Buffer* drawBuf = new jvk::Buffer();
    pDevice->create_uniform_buffer(drawDataBufferSize * MAX_CONCURRENT_FRAMES, false, drawBuf); drawBuf->map();

    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
    {
        VK_CHECK(pDevice->create_buffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            1ULL * 1024 * 1024, &transientVtxBuf[i]));

        uboPassData[i] = UniformBuffer::CreateShared(pDevice, 8 * 1024, false);
        uboPassData[i]->Map();
        uboPassData[i]->SetName("PerPass UBO " + std::to_string(i));

        uboPostProcessData[i] = UniformBuffer::CreateShared(pDevice, sizeof(PostProcessData), false);
        uboPostProcessData[i]->Map();
        uboPostProcessData[i]->SetName("PostProcData UBO " + std::to_string(i));

        drawPool[i] = new UniformBufferPool(drawBuf, drawDataBufferSize, i * drawDataBufferSize, minUboAlignment);
        //pDevice->create_uniform_buffer(lights.size() * sizeof(Light), true, &uboLights[i]);
        uboLights[i] = UniformBuffer::CreateShared(pDevice, lights.size() * sizeof(Light), true);
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

    const int sceneIdx = 0;
    auto scenePath = scenes[sceneIdx].dir;
    jsr::gltfLoadWorld(scenePath / scenes[sceneIdx].file, *world);

    std::vector<jsr::Vertex> vertices;
    std::vector<uint16_t> indices;

    uint32_t firstIndex(0);
    uint32_t firstVertex(0);

    materials.resize(world->materials.size());

    int i = 0;
    jsrlib::Info("Loading images...");
    for (auto it = world->materials.begin(); it != world->materials.end(); it++)
    {
        std::string fname[4];
        fname[0] = (scenePath / it->texturePaths.albedoTexturePath).u8string();
        fname[1] = (scenePath / it->texturePaths.normalTexturePath).u8string();
        fname[2] = (scenePath / it->texturePaths.specularTexturePath).u8string();
        fname[3] = (scenePath / it->texturePaths.emissiveTexturePath).u8string();

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
        create_material_texture(fname[3]);

        std::array<VkDescriptorImageInfo, 4> images{
            imageCache.at(fname[0]).descriptor,
            imageCache.at(fname[1]).descriptor,
            imageCache.at(fname[2]).descriptor,
            imageCache.at(fname[3]).descriptor
        };

        descMgr.builder()
            .bind_images(0, static_cast<uint32_t>(images.size()), images.data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            //            .bind_image(1, &imageCache.at(fname[1]).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            //            .bind_image(2, &imageCache.at(fname[2]).descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(materials[i++].resources);
    }

    const vec4 v4_one = vec4(1.0f);
    for (size_t i(0); i < world->meshes.size(); ++i)
    {
        auto& mesh = world->meshes[i];
        const size_t numVerts = mesh.positions.size() / (3 * sizeof(float));
        for (size_t i(0); i < numVerts; ++i)
        {
            vertices.emplace_back();
            auto& v = vertices.back();
            v.set_position((float*)&mesh.positions[i * 3 * sizeof(float)]);
            v.set_uv((float*)&mesh.uvs[i * 2 * sizeof(float)]);
            v.pack_normal((float*)&mesh.normals[i * 3 * sizeof(float)]);
            v.pack_tangent((float*)&mesh.tangents[i * 4 * sizeof(float)]);
            v.pack_color(&v4_one[0]);
        }
        meshes.emplace_back();
        auto& rmesh = meshes.back();
        rmesh.firstVertex = firstVertex;
        firstVertex += numVerts;

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

    jvk::Buffer stagingBuffer;

    pDevice->create_staging_buffer(std::max(vertexBytes, indexBytes), &stagingBuffer);
    stagingBuffer.copyTo(0, vertexBytes, vertices.data());
    pDevice->buffer_copy(&stagingBuffer, &vtxbuf, 0, 0, vertexBytes);
    stagingBuffer.copyTo(0, indexBytes, indices.data());
    pDevice->buffer_copy(&stagingBuffer, &idxbuf, 0, 0, indexBytes);
    pDevice->destroy_buffer(&stagingBuffer);

    setup_objects();

    const size_t size = drawDataBufferSize * MAX_CONCURRENT_FRAMES;
    //pDevice->create_uniform_buffer(size, false, &uboDrawData);
    uboDrawData = std::make_unique<jvk::UniformBuffer>(pDevice, size, false);
    uboDrawData->SetName("DrawData UBO");
    
    jvk::Buffer stage;
    pDevice->create_staging_buffer(size, &stage);
    stage.copyTo(0, drawData.size(), drawData.data());
    for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
    {
        //pDevice->buffer_copy(&stage, &uboDrawData, 0, i * drawDataBufferSize, drawData.size());
        uboDrawData->CopyTo(&stage, 0, i * drawDataBufferSize, drawData.size());
    }
    pDevice->destroy_buffer(&stage);


    const int kernelSize = sizeof(passData.avSSAOkernel) / sizeof(passData.avSSAOkernel[0]);

    init_lights();
    setup_descriptor_sets();

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

void Sample1App::get_enabled_features()
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

void Sample1App::get_enabled_extensions()
{
    enabled_instance_extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    enabled_device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    //enabled_device_extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeaturesKHR{};
    dynamicRenderingFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeaturesKHR.dynamicRendering = VK_TRUE;
    required_generic_features.push_back(jvk::GenericFeature(dynamicRenderingFeaturesKHR));
}

void Sample1App::create_material_texture(const std::string& filename)
{
    std::string fn = filename;

    if (imageCache.find(fn) == imageCache.end())
    {
        int w, h, nc;
        // load image
        jvk::Image newImage;
        fs::path base = fs::path(fn).parent_path();
        fs::path name = fs::path(fn).filename();
        name.replace_extension("ktx2");
        auto dds = base / "dds" / name;
        auto ktx = base / "ktx" / name;

        bool bOk = false;
#if 0
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
#endif

        ktxTexture* kTexture;
        KTX_error_code result;

        result = ktxTexture_CreateFromNamedFile(ktx.u8string().c_str(),
            KTX_TEXTURE_CREATE_NO_FLAGS,
            &kTexture);

        if (result == KTX_SUCCESS)
        {
            if (ktxTexture2_NeedsTranscoding((ktxTexture2*)kTexture)) {
                ktx_texture_transcode_fmt_e tf;

                // Using VkGetPhysicalDeviceFeatures or GL_COMPRESSED_TEXTURE_FORMATS or
                // extension queries, determine what compressed texture formats are
                // supported and pick a format. For example
                VkPhysicalDeviceFeatures deviceFeatures = pDevice->vkbPhysicalDevice.features;
                khr_df_model_e colorModel = ktxTexture2_GetColorModel_e((ktxTexture2*)kTexture);
                if (colorModel == KHR_DF_MODEL_UASTC
                    && deviceFeatures.textureCompressionASTC_LDR) {
                    tf = KTX_TTF_ASTC_4x4_RGBA;
                }
                else if (colorModel == KHR_DF_MODEL_ETC1S
                    && deviceFeatures.textureCompressionETC2) {
                    tf = KTX_TTF_ETC;
                }
                else if (deviceFeatures.textureCompressionASTC_LDR) {
                    tf = KTX_TTF_ASTC_4x4_RGBA;
                }
                else if (deviceFeatures.textureCompressionETC2)
                    tf = KTX_TTF_ETC2_RGBA;
                else if (deviceFeatures.textureCompressionBC)
                    tf = KTX_TTF_BC7_RGBA;
                else {
                    const std::string message = "Vulkan implementation does not support any available transcode target.";
                    throw std::runtime_error(message.c_str());
                }

                result = ktxTexture2_TranscodeBasis((ktxTexture2*)kTexture, tf, 0);

                // Then use VkUpload or GLUpload to create a texture object on the GPU.
            }

            // Retrieve information about the texture from fields in the ktxTexture
            // such as:
            ktx_uint32_t numLevels = kTexture->numLevels;
            ktx_uint32_t baseWidth = kTexture->baseWidth;
            ktx_uint32_t baseHeight = kTexture->baseHeight;
            ktx_bool_t isArray = kTexture->isArray;

            pDevice->create_texture2d_with_mips(
                ktxTexture2_GetVkFormat((ktxTexture2*)kTexture),
                VkExtent3D{ kTexture->baseWidth,kTexture->baseHeight,kTexture->baseDepth },
                &newImage);

            jvk::StagingBuffer stagebuf(pDevice, ktxTexture_GetDataSize(kTexture));
            stagebuf.CopyTo(0, ktxTexture_GetDataSize(kTexture), ktxTexture_GetData(kTexture));

            pDevice->execute_commands([&](VkCommandBuffer cmd)
                {
                    newImage.record_upload(cmd, [kTexture, baseWidth, baseHeight](uint32_t layer, uint32_t face, uint32_t level, jvk::Image::UploadInfo* inf)
                        {
                            size_t offset{};
                            uint32_t w = baseWidth >> level;
                            uint32_t h = baseHeight >> level;
                            w = std::max(w, 1u);
                            h = std::max(h, 1u);

                            // Retrieve a pointer to the image for a specific mip level, array layer
                            // & face or depth slice.
                            ktxResult res = ktxTexture_GetImageOffset(kTexture, level, layer, face, &offset);
                            assert(res == KTX_SUCCESS);

                            inf->extent = { w,h,1 };
                            inf->offset = offset;

                        }, stagebuf.GetBuffer());

                    newImage.record_layout_change(cmd,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                    newImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                });

            //newImage.change_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            ktxTexture_Destroy(kTexture);
        }
        else if (!load_texture2d(fn, &newImage, true, w, h, nc))
        {
            jsrlib::Error("%s notfund", fn.c_str());
            pDevice->create_texture2d(VK_FORMAT_R8G8B8A8_UNORM, { 1,1,1 }, &newImage);
            newImage.layout_change(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

bool Sample1App::load_texture2d(std::string filename, jvk::Image* dest, bool autoMipmap, int& w, int& h, int& nchannel)
{
    //int err = stbi_info(filename.c_str(), &w, &h, &nchannel);

    auto* data = stbi_load(filename.c_str(), &w, &h, &nchannel, STBI_rgb_alpha);
    if (!data)
    {
        return false;
    }


    size_t size = w * h * 4;
    jvk::Buffer stage;
    pDevice->create_staging_buffer(size, &stage);
    stage.copyTo(0, size, data);
    stbi_image_free(data);
    const VkExtent3D extent = { (uint32_t)w,(uint32_t)h,1 };

    if (autoMipmap) {
        pDevice->create_texture2d_with_mips(VK_FORMAT_R8G8B8A8_UNORM, extent, dest);
    }
    else {
        pDevice->create_texture2d(VK_FORMAT_R8G8B8A8_UNORM, extent, dest);
    }

    dest->upload(extent, 0, 0, 0, 0ull, &stage);
    if (autoMipmap) {
        dest->generate_mipmaps();
    }
    else {
        dest->layout_change(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    return true;
}

