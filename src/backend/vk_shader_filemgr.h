#pragma once

#include "pch.h"
#include "common/resource_handles.h"
#include "common/resource_descriptions.h"

namespace vks {

	struct GraphicsShaderFileInfo {
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> fragmentShader;
		std::vector<uint8_t> geometryShader;
	};

	struct ComputeShaderFileInfo {
		std::vector<uint8_t> computeShader;
	};

	class ShaderFileManager {
	public:
		ShaderFileManager() {}
		GraphicShadersHandle createGraphicsShader(const GraphicPassShaderDescriptions& desc);
		ComputeShaderHandle createComputeShader(const ComputePassShaderDescriptions& desc);
		bool getGraphicsShaders(GraphicShadersHandle handle, GraphichPipelineShaderBinary* out);
		bool getComputeShader(ComputeShaderHandle handle, ComputePipelineShaderBinary* out);
	private:
		std::vector<GraphicsShaderFileInfo> m_graphicsShaders;
		std::vector<ComputeShaderFileInfo> m_computeShaders;
	};
}