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
public:

    VkDevice d;

    VkRenderPass renderPass = {};
    VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};

    App(bool b) : AppBase(b) {
    }

    virtual ~App() {
        vkDeviceWaitIdle(d);
        vkDestroyRenderPass(d, renderPass, nullptr);
        for (size_t i(0); i < MAX_CONCURRENT_FRAMES; ++i)
        {
            vkDestroyFramebuffer(d, fb[i], 0);
        }
    }

    virtual void update_imgui() override {
        ImGui::ShowDemoWindow();
    }

    virtual void render() override {        
        if (fb[current_frame]) {
            vkDestroyFramebuffer(d, fb[current_frame], 0);
        }

        VkFramebufferCreateInfo fbci = vks::initializers::framebufferCreateInfo();
        fbci.attachmentCount = 1;
        fbci.renderPass = renderPass;
        fbci.layers = 1;
        fbci.pAttachments = &swapchain.views[current_buffer];
        fbci.width = swapchain.vkb_swapchain.extent.width;
        fbci.height = swapchain.vkb_swapchain.extent.height;
        VK_CHECK(vkCreateFramebuffer(d, &fbci, 0, &fb[current_frame]));

        VkRenderPassBeginInfo beginPass = vks::initializers::renderPassBeginInfo();
        VkClearValue clearVal;
        clearVal.color = { 0.4f,0.0f,0.4f,1.0f };
        beginPass.clearValueCount = 1;
        beginPass.pClearValues = &clearVal;
        beginPass.renderPass = renderPass;
        beginPass.framebuffer = fb[current_frame];
        beginPass.renderArea.extent = swapchain.vkb_swapchain.extent;
        beginPass.renderArea.offset = { 0,0 };

        VkCommandBuffer cmd = draw_cmd_buffers[current_frame];
        vkCmdBeginRenderPass(cmd, &beginPass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(cmd);

    }

    virtual void setup_render_pass() override {
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

        VK_CHECK(vkCreateRenderPass(d, &rpci, nullptr, &renderPass));

    }
    virtual void prepare() override {
        vkjs::AppBase::prepare();

        d = *device_wrapper;

        setup_render_pass();

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
private:
    vkjs::Buffer ubo;
    vkjs::Buffer stg;
    vkjs::Buffer ssb1;
    vkjs::Buffer ssb2;
};

void demo()
{
    App* app = new App(true);
    app->init();
    app->prepare();
    app->run();

    delete app;
    exit(0);

    jsr::Camera view;
    view.Position = glm::vec3(c_zero, c_half, c_zero);
    view.Front = glm::vec3(c_zero, c_zero, c_one);
    view.MovementSpeed = 0.001f;
    view.Zoom = 55.0f;
    view.ProcessMouseMovement(c_zero, c_zero);
    glm::mat4 proj = jsr::perspective(radians(view.Zoom), 4.0f / 3.0f, 0.1f, 1000.0f);

    World world;
    if (gltfLoadWorld(fs::path("../../resources/models/sw_venator/scene.gltf"), world)) {
        jsrlib::Info("World loaded");
        world.updateBVH();

        Frustum frustum(proj * view.GetViewMatrix());
        auto vis = world.getVisibleEntities(frustum);
    }
    //return;

    jsr::Window window;
    jsr::WindowCreateInfo windowInfo = {};
    windowInfo.width = 1800;
    windowInfo.height = 1000;
    windowInfo.fullscreen = false;
    
    windowInfo.title = "JSR's Vulkan Engine";
    window.init(windowInfo);

    VulkanDevice device(&window);
    VulkanSwapchain swapchain(&device);
    VulkanRenderer renderer(&device, &swapchain, 1);
    
    jsrlib::gLogWriter.SetFileName("vulkan_engine.log");

    GraphicsPipelineShaderInfo shader;
    shader.vertexShader = "../../shaders/default.vert.spv";
    shader.fragmentShader = "../../shaders/default.frag.spv";
    handle::GraphicsPipeline forwardPipe = renderer.createGraphicsPipeline<jsr::ForwardLightingPass>(shader);
    shader.vertexShader = "../../shaders/depthPass.vert.spv";
    shader.fragmentShader = "../../shaders/depthPass.frag.spv";
    handle::GraphicsPipeline depthPassPipe = renderer.createGraphicsPipeline<jsr::DepthPrePass>(shader);
    
    ComputePipelineShaderInfo comp;
    comp.computeShader = "../../shaders/sinus.comp.spv";
    handle::ComputePipeline compPipe = renderer.createComputePipeline<jsr::CompImageGen>(comp, 16, 16, 1);


    const int VERTEX_COUNT = 6;
    std::vector<jsr::Vertex> m_triangles(VERTEX_COUNT);
    std::vector<uint16_t> index(6);

    m_triangles[0].xyz = vec3(1.0f, -1.0f, 0.0f);
    m_triangles[1].xyz = vec3(-1.0f, -1.0f, 0.0f);
    m_triangles[2].xyz = vec3(-1.0f, 1.0f, 0.0f);

    m_triangles[3].xyz = vec3(-1.0f, 1.0f, 0.0f);
    m_triangles[4].xyz = vec3(1.0f, 1.0f, 0.0f);
    m_triangles[5].xyz = vec3(1.0f, -1.0f, 0.0f);

    m_triangles[0].uv = vec2(1.0f, 0.0f);
    m_triangles[1].uv = vec2(0.0f, 0.0f);
    m_triangles[2].uv = vec2(0.0f, 1.0f);

    m_triangles[3].uv = vec2(0.0f, 1.0f);
    m_triangles[4].uv = vec2(1.0f, 1.0f);
    m_triangles[5].uv = vec2(1.0f, 0.0f);

    m_triangles[0].pack_color(vec4(1.0f, 0.0f, 0.0f, 1.0f));
    m_triangles[1].pack_color(vec4(0.0f, 1.0f, 0.0f, 1.0f));
    m_triangles[2].pack_color(vec4(0.0f, 1.0f, 0.0f, 1.0f));
    m_triangles[3].pack_color(vec4(0.0f, 1.0f, 0.0f, 1.0f));
    m_triangles[4].pack_color(vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_triangles[5].pack_color(vec4(1.0f, 0.0f, 0.0f, 1.0f));

    for (int i = 0; i < VERTEX_COUNT; ++i) {
        index[i] = i;
    }


    for (int i = 0; i < VERTEX_COUNT; ++i) {
        m_triangles[i].uv.y = c_one - m_triangles[i].uv.y;
        m_triangles[i].xyz.y = -m_triangles[i].xyz.y;
    }

    SDL_Event e;
    bool quit = false;
    uint32_t frame{};

    int win_x(1), win_y(1);
    bool minimized(false);

    Model model(&renderer);
#if 1

    if (model.load_from_gltf("../../resources/models/sponza/sponza_j.gltf")) {
        jsrlib::Info("Model loaded");
    }
#else
    if (model.load_from_gltf("../../resources/models/sw_venator/scene.gltf")) {
        jsrlib::Info("Model loaded");
    }
#endif

    bool keystate[256];
    memset(keystate, 0, 256 * sizeof(keystate[0]));

    int mpx, mpy;
    SDL_GetGlobalMouseState(&mpx, &mpy);

    const float zNear = asfloat(0.1);
    const float zFar = asfloat(500);

    struct VisibleObject {
        VisibleObject(int object_, int primitive_) : object(object_), primitive(primitive_) {}
        VisibleObject() = default;
        int object;
        int primitive;
    };

    const int PVS_SIZE = 300;
    std::vector<VisibleObject> visibleSurfs(PVS_SIZE);
    std::vector<S_OBJECT> rbObjectIndices(PVS_SIZE);
    jsr::StorageBufferDescription objectBufferDesc;
    objectBufferDesc.initialData = nullptr;
    objectBufferDesc.readOnly = true;
    objectBufferDesc.size = rbObjectIndices.size() * sizeof(S_OBJECT);
    handle::StorageBuffer objectBuffers[FrameCounter::INFLIGHT_FRAMES];
    handle::Image generatedImage[FrameCounter::INFLIGHT_FRAMES];
    handle::DescriptorSet generatedImageSet[FrameCounter::INFLIGHT_FRAMES];

    jsr::ImageDescription imgDesc = {};
    imgDesc.depth = 1;
    imgDesc.faces = 1;
    imgDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgDesc.height = 512;
    imgDesc.width = 512;
    imgDesc.name = "generated-%d";
    imgDesc.usageFlags = jsr::ImageUsageFlags::Storage | jsr::ImageUsageFlags::Sampled;

    for (int i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i)
    {
        objectBuffers[i] = renderer.createStorageBuffer(objectBufferDesc);

        imgDesc.name = "generated-" + std::to_string(i);
        generatedImage[i] = renderer.createImage(imgDesc);

        ImageResource imgres(generatedImage[i], 0, 1);
        RenderPassResources compResources;
        compResources.storageImages.push_back(imgres);
        generatedImageSet[i] = renderer.createDescriptorSet(compPipe, compResources);
    }

    const float fpsLimit = (1000.f / 60.0f);

    auto fpsPrevTime = std::chrono::steady_clock::now();

    bool mouseCapture = false;
    ImGuiIO& io = ImGui::GetIO();

    float time = static_cast<float>(SDL_GetTicks64());
    float lastTime = time;
    float dt = time - lastTime;
    int lastNumDrawSurf = 0;
    std::chrono::nanoseconds frameTime;

    renderer.gpass_data.vAmbientColor = vec4(vec3(c_one), 0.005f);
    renderer.gpass_data.fExposureBias = asfloat(1);
    renderer.gpass_data.vFogColor = vec4(asfloat(1));
    renderer.gpass_data.fFarPlane = 200.f;
    renderer.gpass_data.fNearPlane = 0.1f;
    renderer.gpass_data.fFogDensity = 0.001f;

    int ONE_MESH = 0;
    bool keystate_LSHIFT = false;
    std::vector<jsr::DrawMeshDescription> meshes(512);

    Bounds worldBounds = model.m_bounds;
    std::vector<Light> lights(64);
    setup_lights(worldBounds, lights);

    jsr::UniformBufferDescription lightsBufferDesc;
    handle::UniformBuffer lightsBuffer[FrameCounter::INFLIGHT_FRAMES];

    for (size_t i = 0; i < FrameCounter::INFLIGHT_FRAMES; ++i) {
        lightsBufferDesc.initialData = lights.data();
        //lightsBufferDesc.readOnly = true;
        lightsBufferDesc.size = lights.size() * sizeof(Light);

        lightsBuffer[i] = renderer.createUniformBuffer(lightsBufferDesc);
    }

    uvec2 extent = uvec2(renderer.getSwapchain()->getSwapChainExtent().width, renderer.getSwapchain()->getSwapChainExtent().height);

    renderer.gpass_data.mProjection = jsr::perspective(radians(view.Zoom), swapchain.getAspect(), renderer.gpass_data.fNearPlane, renderer.gpass_data.fFarPlane);
    renderer.gpass_data.mProjection[1][1] *= -1;
    renderer.gpass_data.mInvProjection = glm::inverse(renderer.gpass_data.mProjection);
    bool setupLightsLock = false;

    while (!quit) {
        while (SDL_PollEvent(&e) != SDL_FALSE) {

            if (e.type == SDL_QUIT) {
                quit = true;
                break;
            }
#if 1
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
                continue;
            }
#endif

            if (e.type == SDL_WINDOWEVENT) {
                switch (e.window.event) {
                case SDL_WINDOWEVENT_MINIMIZED:
                    minimized = true;
                    break;
                case SDL_WINDOWEVENT_RESTORED:
                    minimized = false;
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                    break;
                }
            }
            else if (e.type == SDL_KEYDOWN) {
                keystate[e.key.keysym.sym & 255] = true;
                if (e.key.keysym.sym == SDLK_LSHIFT) {
                    keystate_LSHIFT = true;
                }
                if (e.key.keysym.sym == SDLK_x) {
                    setupLightsLock = false;
                }
            }
            else if (e.type == SDL_KEYUP) {
                keystate[e.key.keysym.sym & 255] = false;
                if (e.key.keysym.sym == SDLK_LSHIFT) {
                    keystate_LSHIFT = false;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT && !mouseCapture)
            {
                SDL_SetRelativeMouseMode(SDL_TRUE);
                mouseCapture = true;
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT)
            {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                mouseCapture = false;
            }
            else if (e.type == SDL_MOUSEWHEEL)
            {
                float r = (float)e.wheel.y;
                if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) r = -r;
                view.ProcessMouseScroll((float)e.wheel.y);
            }

        } // while(SDL_PollEvent)

        int x, y;
        SDL_GetRelativeMouseState(&x, &y);
        if (mouseCapture)
        {
            view.ProcessMouseMovement(asfloat(x), asfloat(-y), true);
            //jsrlib::Info("Camera.: %.3f, Camera.Pitch: %.3f", view.Yaw, view.Pitch);
        }


        if (minimized) {
            SDL_WaitEvent(&e);
            continue;
        }

        if (keystate[SDLK_ESCAPE]) {
            quit = true;
        }
        if (keystate[SDLK_w]) {
            view.ProcessKeyboard(jsr::FORWARD, dt);
            //jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
        }
        if (keystate[SDLK_s]) {
            view.ProcessKeyboard(jsr::BACKWARD, dt);
            //jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
        }
        if (keystate[SDLK_a]) {
            view.ProcessKeyboard(jsr::LEFT, dt);
            //jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
        }
        if (keystate[SDLK_d]) {
            view.ProcessKeyboard(jsr::RIGHT, dt);
            //jsrlib::Info("Camera.Pos[%.3f, %.3f, %.3f]", view.Position.x, view.Position.y, view.Position.z);
        }
        if (keystate[SDLK_x] && !setupLightsLock) {
            setupLightsLock = true;
            setup_lights(worldBounds, lights);
        }
        if (keystate_LSHIFT) {
            view.MovementSpeed = 0.01f;
        }
        else {
            view.MovementSpeed = 0.001f;
        }
        //SDL_WaitEvent(&e);
        time = static_cast<float>(SDL_GetTicks64());
        dt = time - lastTime;
        lastTime = time;

        if ((jsr::FrameCounter::getCurrentFrame() % 60) == 0) {
            frameTime = std::chrono::steady_clock::now() - fpsPrevTime;
        }
        fpsPrevTime = std::chrono::steady_clock::now();

        if (!renderer.prepareNextFrame())
        {
            continue;
        }


        ImGui::NewFrame();
        ImGui::LabelText("Visible surfaces", "%d", lastNumDrawSurf);
        ImGui::LabelText("Frame time (ms)", "%.2f", asfloat(frameTime.count()/1e6));
        ImGui::LabelText("Camera Pos.", "[%.2f %.2f %.2f]", view.Position.x, view.Position.y, view.Position.z);
        ImGui::LabelText("Camera Rot.", "[Y: %.2f P: %.2f]", view.Yaw, view.Pitch);

        ImGui::DragFloat("Exposure bias", &renderer.gpass_data.fExposureBias, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Ambient factor", &renderer.gpass_data.vAmbientColor[3], 0.001f, 0.001f, c_one);
        ImGui::ColorEdit3("Ambient color", &renderer.gpass_data.vAmbientColor[0]);
        ImGui::ColorEdit3("Fog color", &renderer.gpass_data.vFogColor[0]);
        ImGui::DragFloat("Fog density", &renderer.gpass_data.fFogDensity, 0.0001f, c_zero, 0.1f, "%.4f");

        lastNumDrawSurf = 0;

        float fFrame = float(jsr::FrameCounter::getCurrentFrame());
        const uint32_t frameIndex = jsr::FrameCounter::getFrameIndex();

        renderer.gpass_data.mProjection = jsr::perspective(radians(view.Zoom), swapchain.getAspect(), renderer.gpass_data.fNearPlane,renderer.gpass_data.fFarPlane);
        renderer.gpass_data.mProjection[1][1] *= -c_one;
        renderer.gpass_data.mInvProjection = glm::inverse(renderer.gpass_data.mProjection);
        renderer.gpass_data.mView = view.GetViewMatrix();
        renderer.gpass_data.mViewProjection = renderer.gpass_data.mProjection * renderer.gpass_data.mView;

//        clusterLights.cullLights(lights, renderer.gvars.mView);

        RenderPassResources compResources;
        auto genImageRes = ImageResource(generatedImage[frameIndex], 0, 0, true);
        compResources.storageImages.push_back(genImageRes);
        jsr::ComputePassInfo compPass(compPipe, compResources, 512, 512, 1);
        compPass.descriptorSet = generatedImageSet[frameIndex];
        renderer.setComputePass(compPass);

        jsr::RenderPassResources rsrc;
        rsrc.storageBuffers.push_back(jsr::StorageBufferResource(objectBuffers[frameIndex], true, 0));
        rsrc.storageBuffers.push_back(jsr::StorageBufferResource(model.m_matrixBuffer, true, 1));
        rsrc.storageBuffers.push_back(jsr::StorageBufferResource(model.m_materialBuffer, true, 2));
        rsrc.uniformBuffers.push_back(jsr::UniformBufferResource(lightsBuffer[frameIndex], 3));

        jsr::RenderPassResources rsrcZ;
        rsrcZ.storageBuffers.push_back(jsr::StorageBufferResource(objectBuffers[frameIndex], true, 0));
        rsrcZ.storageBuffers.push_back(jsr::StorageBufferResource(model.m_matrixBuffer, true, 1));

        jsr::GraphicsPassInfo passinfZ(depthPassPipe, { renderer.getZBuffer() }, rsrcZ);
        renderer.setGraphicsPass(passinfZ);
        renderer.setMergedMesh(model.m_backendMesh);
 
        jsr::GraphicsPassInfo passinf(forwardPipe, { renderer.getSwapchainImage(), renderer.getZBuffer()}, rsrc);
        renderer.setGraphicsPass(passinf);
        renderer.setMergedMesh(model.m_backendMesh);
        

        meshes.clear();
        //visibleSurfs.assign(256, { -1,-1 });
        std::atomic_int vc = 0;
        jsrlib::counting_semaphore counter;

        const mat4 V = view.GetViewMatrix();
        const jsr::Frustum viewFrustum(renderer.gpass_data.mProjection * V);
        using namespace jsr;

        std::function<void(int)> fnode = [&](int nodeIdx)
        {
            const auto& node = model.m_nodes[nodeIdx];

            const size_t childCount = node.children.size();
            for (size_t it(0); it < childCount; ++it)
            {
                fnode(node.children[it]);
            }
            

            if (node.object > -1 && node.nodeType == nodeType_Mesh)
            {
                const int meshIdx = node.object;
                const size_t primitiveCount = model.m_meshes[ meshIdx ].primitives.size();

//                jobsys.submitJob([primitiveCount,meshIdx,&model,nodeIdx,&viewFrustum,&visibleSurfs,&rbObjectIndices,&vc](int threadID) {
                    const glm::mat4 mModel = model.getWorldTransform(nodeIdx);
                    for (size_t i = 0; i < primitiveCount; ++i)
                    {
                        const auto& p = model.m_meshes[meshIdx].primitives[i];
                        const jsr::Bounds bbox(p.bbox[0], p.bbox[1]);
                        if (viewFrustum.Intersects2(bbox.Transform(mModel)))
                        {
                            const size_t index = vc.fetch_add(1, std::memory_order_relaxed);
                            visibleSurfs[index] = VisibleObject( nodeIdx, (int)i );
                            rbObjectIndices[index].matrixesIdx = nodeIdx;
                            rbObjectIndices[index].materialIdx = p.material;
                        }
                    }
//                    }, &counter);
            }
        };

        std::for_each(model.m_scene.begin(), model.m_scene.end(), [&](int n) { fnode(n); });

//        counter.wait();
        visibleSurfs[vc] = { -1,-1 };

        renderer.prepareDrawCommands();

        // depth pass
        if (vc > 0) {
            renderer.updateStorageBufferData(objectBuffers[frameIndex], vc * sizeof(S_OBJECT), rbObjectIndices.data());
            renderer.updateUniformBufferData(lightsBuffer[frameIndex], (uint32_t)(sizeof(Light)* lights.size()), lights.data());

            jsr::DrawMeshDescription md = {};
            md.mesh = model.m_backendMesh;
            md.mode = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            md.pipeline = depthPassPipe;
            md.instanceCount = 1;

            for (int i = 0; i < (int)visibleSurfs.size(); ++i)
            {
                const auto& it = visibleSurfs[i];
                if (it.object == -1) {
                    break;
                }

                const auto& p = model.m_meshes[ model.m_nodes[it.object].object ] .primitives[ it.primitive ];
                const auto& material = model.m_materials[ p.material ]; 
                if (material.alphaMode != ALPHA_MODE_OPAQUE) {
                    continue;
                }
                md.firstInstance = i;
                md.meshResources = material.shaderSet;
                md.vc.baseVertex = p.baseVertex;
                md.vc.firstIndex = p.firstIndex;
                md.vc.indexCount = p.indexCount;

                meshes.push_back(md);
            }
            renderer.drawMeshes(meshes, 0, nullptr);
        }
        meshes.clear();
        // forward pass
        if (vc > 0) {
            DrawMeshDescription md = {};
            md.mesh = model.m_backendMesh;
            md.mode = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            md.pipeline = forwardPipe;
            md.instanceCount = 1;
            int i = 0;
            for (; i < (int) visibleSurfs.size(); ++i)
            {
                const auto& it = visibleSurfs[i];
                if (it.object == -1) {
                    break;
                }

                ++lastNumDrawSurf;

                const auto& p = model.m_meshes[model.m_nodes[it.object].object].primitives[it.primitive];
                const auto& material = model.m_materials[p.material];

                if (material.alphaMode == ALPHA_MODE_BLEND) {
                    continue;
                }

                md.firstInstance = i;
                md.meshResources = material.shaderSet;
                md.vc.baseVertex = p.baseVertex;
                md.vc.firstIndex = p.firstIndex;
                md.vc.indexCount = p.indexCount;
                meshes.push_back(md);
            }
            renderer.drawMeshes(meshes, 0, nullptr);
        }

        renderer.renderFrame(true);
        std::for_each(lights.begin(), lights.end(), [&](jsr::Light& l) 
            {
                l.position.x += 0.0015f * dt;
                if (l.position.x > model.m_bounds.max().x) {
                    l.position.x = model.m_bounds.min().x;
                }
            });
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    model.cleanup();
}

int main(int argc, char* argv[])
{
    demo();

    return 0;
}