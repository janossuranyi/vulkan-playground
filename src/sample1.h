#pragma once

#include "vkjs/vkjs.h"
#include "vkjs/vk_descriptors.h"
#include "vkjs/VulkanInitializers.hpp"
#include "vkjs/appbase.h"
#include "vkjs/pipeline.h"
#include "glm/glm.hpp"
#include "bounds.h"
#include "imgui.h"
#include "world.h"
#include "light.h"

struct UniformBufferPool {
    jvk::Buffer* buffer{};
    std::atomic_uint32_t bytesAlloced = 0u;
    uint32_t offsetAligment = 64u;
    uint32_t internalOffset = 0u;
    size_t _size = 0ULL;
    VkDescriptorBufferInfo descriptor = {};

    UniformBufferPool() = delete;
    UniformBufferPool(jvk::Buffer* buffer, size_t size, uint32_t startOffset, uint32_t offsetAligment) :
        buffer(buffer), _size(size), offsetAligment(offsetAligment), bytesAlloced(0) {

        internalOffset = startOffset;
        descriptor.buffer = buffer->buffer;
        descriptor.offset = internalOffset;
        descriptor.range = size;
    }

    template<class T>
    VkDescriptorBufferInfo Allocate(uint32_t count, const T* data)
    {
        const uint32_t alignedSize = static_cast<uint32_t>(align_size(sizeof(T), offsetAligment));
        const uint32_t size = count * alignedSize;
        const uint32_t offset = bytesAlloced.fetch_add(size);


        assert((internalOffset + offset + size) < buffer->size);

        VkDescriptorBufferInfo out = {};
        out.buffer = buffer->buffer;
        out.offset = offset;
        out.range = count * sizeof(T);

        if (data)
        {
            size_t it_offset = internalOffset + offset;
            for (size_t it = 0; it < count; ++it)
            {
                buffer->copyTo(it_offset, sizeof(T), &data[it]);
                it_offset += alignedSize;
            }
        }
        return out;
    }
    void reset() { bytesAlloced = 0; }
};


class Sample1App : public jvk::AppBase {
private:
    //const VkFormat HDR_FMT = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    const VkFormat HDR_RT_FMT = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    const VkFormat NORMAL_RT_FMT = VK_FORMAT_R16G16_SFLOAT;
    bool fogEnabled = true;
    float fps = 0.f;
    float maxZ = 0.0f, minZ = 0.0f;
    VkDevice d;
    bool smaaChanged = false;
    bool firstRun = true;
    bool initLights = false;

    jvk::Image uvChecker;
    jvk::Image ssaoNoise;
    std::array<VkImageView, MAX_CONCURRENT_FRAMES> depthResolvedView{};
    std::array<jvk::Image, MAX_CONCURRENT_FRAMES> depthResolved{};
    std::array<jvk::Image, MAX_CONCURRENT_FRAMES> HDRImage_MS{};
    std::array<jvk::Image, MAX_CONCURRENT_FRAMES> HDRImage{};
    std::array<jvk::Image, MAX_CONCURRENT_FRAMES> HDR_NormalImage_MS{};
    std::array<jvk::Image, MAX_CONCURRENT_FRAMES> HDR_NormalImage{};
    std::array<VkFramebuffer, MAX_CONCURRENT_FRAMES> HDRFramebuffer{};
    std::array<VkDescriptorSet, MAX_CONCURRENT_FRAMES> HDRDescriptor{};
    std::array<UniformBufferPool*, MAX_CONCURRENT_FRAMES> drawPool{};

    VkSampler sampLinearRepeat;
    VkSampler sampNearestClampBorder;

    vkutil::DescriptorManager descMgr;

    std::unordered_map<std::string, jvk::Image> imageCache;

    jvk::Buffer vtxbuf;
    jvk::Buffer idxbuf;
    size_t minUboAlignment = 0;

    jvk::Buffer vtxStagingBuffer;
    std::array<jvk::Buffer, MAX_CONCURRENT_FRAMES> transientVtxBuf;

    std::array<jvk::Buffer, MAX_CONCURRENT_FRAMES> uboPassData;
    std::array<jvk::Buffer, MAX_CONCURRENT_FRAMES> uboPostProcessData;
    std::array<jvk::Buffer, MAX_CONCURRENT_FRAMES> uboLights;
    std::array<jsr::Light, 16> lights;

    size_t drawDataBufferSize = 0;

    jvk::Buffer uboDrawData;

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
        glm::mat4 mtxModel;
        jsr::Bounds aabb;
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
        glm::vec4 vParams;
    } passData;


    struct PostProcessData {
        glm::mat4 mtxInvProj;
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
        float fHDRLuminance;
        float fHDRbias;
        bool bHDR;
        int pad1;
        int pad2;
    } postProcessData{};

    static_assert((sizeof(PostProcessData) & 15) == 0);

    struct RenderPass {
        VkRenderPass pass;
        jvk::GraphicsPipeline* pPipeline;
    };

    VkDescriptorSet triangleDescriptors[MAX_CONCURRENT_FRAMES];
    VkDescriptorSetLayout tonemapLayout = VK_NULL_HANDLE;

    VkPipelineLayout debugPipelineLayout = VK_NULL_HANDLE;
    VkPipeline debugPipeline = VK_NULL_HANDLE;

    VkFramebuffer fb[MAX_CONCURRENT_FRAMES] = {};
    std::filesystem::path basePath = std::filesystem::path("../..");

    std::unique_ptr<jsr::World> world;

    struct {
        RenderPass tonemap = {};
        RenderPass preZ = {};
        RenderPass triangle = {};
    } passes;
    uint32_t visibleObjectCount{};

    static const uint32_t TRIANGLE_DESCRIPTOR_ID = 100;
    void init_lights();

public:

    glm::mat4 perspective_revZ(float fovy, float aspect, float zNear, float zFar)
    {

        glm::mat4 Result = glm::perspective(fovy, aspect, zFar, zNear);

        return Result;
    }

    virtual void on_update_gui() override
    {
        if (settings.overlay == false) return;

        static const char* items[] = { "Off","MSAAx2","MSAAx4","MSAAx8" };
        static const char* current_msaa_item = items[static_cast<size_t>(settings.msaaSamples) - 1];

        static const VkSampleCountFlagBits smaaBits[] = { VK_SAMPLE_COUNT_1_BIT,VK_SAMPLE_COUNT_2_BIT,VK_SAMPLE_COUNT_4_BIT,VK_SAMPLE_COUNT_8_BIT };

        ImGui::Text("Fps: %.2f", fps);
        ImGui::Text("ViewOrg: X: %.3f  Y: %.3f  Z: %.3f", camera.Position.x, camera.Position.y, camera.Position.z);
        ImGui::Text("obj in frustum: %d", visibleObjectCount);
        ImGui::Text("maxZ: %.2f, minZ: %.2f", maxZ, minZ);
        //ImGui::DragFloat3("Light pos", &passData.vLightPos[0], 0.05f, -20.0f, 20.0f);
        //ImGui::ColorPicker3("LightColor", &passData.vLightColor[0]);
        ImGui::DragFloat("Light intensity", &passData.vLightColor[3], 0.1f, 0.0f, 10000.0f);
        ImGui::DragFloat("Light range", &passData.vLightPos[3], 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Exposure", &postProcessData.fExposure, 0.01f, 1.0f, 50.0f);
        ImGui::Checkbox("Init lights", &initLights);
        ImGui::Checkbox("Fog On/Off", &fogEnabled);
        ImGui::Checkbox("HDR On/Off", (bool*)(&postProcessData.bHDR));
        ImGui::DragFloat("HDR Luminance", &postProcessData.fHDRLuminance, 0.5f, 1.0f, 1000.0f, "%.1f");
        ImGui::DragFloat("HDR Bias", &postProcessData.fHDRbias, 0.001f, -2.0f, 2.0f, "%.3f");
        ImGui::DragFloat("Fog density", &postProcessData.vFogParams.x, 0.001f, 0.0f, 10.0f, "%.4f");
        ImGui::DragFloat("Fog scale", &postProcessData.vFogParams.y, 0.001f, 0.001f, 1.0f, "%.4f");
        ImGui::DragFloat3("Sun dir", &postProcessData.vSunPos[0], 1.0f);
        ImGui::DragFloat("AO", &passData.vParams[0], 0.001f, 0.001f, 1.0f);

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


    Sample1App(bool b) : AppBase(b) {
        depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        settings.validation = b;
    }

    virtual ~Sample1App();

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
        if (initLights)
        {
            initLights = false;
            init_lights();
        }
    }

    virtual void prepare() override;

    virtual void get_enabled_features() override;

    virtual void get_enabled_extensions() override;

    void create_material_texture(const std::string& filename);
    bool load_texture2d(std::string filename, jvk::Image* dest, bool autoMipmap, int& w, int& h, int& nchannel);
};
