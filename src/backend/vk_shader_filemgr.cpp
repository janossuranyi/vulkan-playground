#include "vk_shader_filemgr.h"
#include <jsrlib/jsr_resources.h>

namespace vks {
    GraphicShadersHandle ShaderFileManager::createGraphicsShader(const GraphicPassShaderDescriptions& desc)
    {
        GraphicShadersHandle res = {};
        std::vector<uint8_t> spirv_vert = jsrlib::Filesystem::root.ReadFile(desc.vertex.path.u8string());
        std::vector<uint8_t> spirv_frag = jsrlib::Filesystem::root.ReadFile(desc.fragment.path.u8string());

        if (spirv_vert.empty() || spirv_frag.empty()) return res;

        GraphicsShaderFileInfo inf;
        inf.vertexShader = std::move(spirv_vert);
        inf.fragmentShader = std::move(spirv_frag);

        if (desc.geometry.has_value())
        {
            inf.geometryShader = jsrlib::Filesystem::root.ReadFile(desc.geometry.value().path.u8string());
        }

        res.index = m_graphicsShaders.size();
        m_graphicsShaders.push_back(inf);

        return res;
    }
    ComputeShaderHandle ShaderFileManager::createComputeShader(const ComputePassShaderDescriptions& desc)
    {
        ComputeShaderHandle res = {};
        std::vector<uint8_t> spirv_comp = jsrlib::Filesystem::root.ReadFile(desc.compute.path.u8string());
        if (spirv_comp.empty()) return res;
        ComputeShaderFileInfo inf;
        inf.computeShader = spirv_comp;

        res.index = m_computeShaders.size();
        m_computeShaders.push_back(inf);

        return res;
    }
    bool ShaderFileManager::getGraphicsShaders(GraphicShadersHandle handle, GraphichPipelineShaderBinary* out)
    {
        if (handle.index == invalidIndex || handle.index >= m_graphicsShaders.size())
        {
            return false;
        }

        const auto& data = m_graphicsShaders[handle.index];
        out->vertexSPIRV = data.vertexShader;
        out->fragmentSPIRV = data.fragmentShader;
        out->geometrySPIRV = data.geometryShader;

        return true;
    }
    bool ShaderFileManager::getComputeShader(ComputeShaderHandle handle, ComputePipelineShaderBinary* out)
    {
        if (handle.index == invalidIndex || handle.index >= m_graphicsShaders.size())
        {
            return false;
        }

        const auto& data = m_computeShaders[handle.index];
        out->computeSPIRV = data.computeShader;

        return true;
    }
}