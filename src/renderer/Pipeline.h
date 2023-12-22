#pragma once
#include "pch.h"

namespace jsr {

	enum CompareOp { CompareOp_Never, CompareOp_Always, CompareOp_LT, CompareOp_LTE, CompareOp_GT, CompareOp_GTE, CompareOp_EQ };
	enum CullMode { CullMode_Back, CullMode_Front, CullMode_None, CullMode_FrontAndBack };
	enum FrontFace { FrontFace_CW, FrontFace_CCW };
	enum BlendOp { BlendOp_Add, BlendOp_Subtract, BlendOp_RevSubtract, BlendOp_Min, BlendOp_Max};
	enum BlendFactor { BlendFactor_Zero, BlendFactor_One, BlendFactor_SrcAlpha, BlendFactor_OneMinusSrcAlpha };
	enum VertexLayout { VertexLayout_Full, VertexLayout_PositionOnly };
	enum Format { Format_R8, Format_R32F, Format_RGBA8, Format_RG16F, Format_RGBA16F, Format_D24S8, Format_D32F, Format_R11G11B10F, Format_RGBA32F };
	enum LoadOp { LoadOp_DontCare, LoadOp_Load, LoadOp_Clear };
	enum StoreOp { StoreOp_DontCare, StoreOp_Store };
	enum PolygonMode { PolygonMode_Fill, PolygonMode_Line, PolygonMode_Point };
	enum PipelineType { PipelineType_Graphics, PipelineType_Compute };
	enum ResourceType {
		ResourceType_UBO,
		ResourceType_UBO_Dyn,
		ResourceType_SSBO,
		ResourceType_SSBO_Dyn,
		ResourceType_Image,
		ResourceType_Texture,
		ResourceType_StorageImage
	};
	enum StageType { StageType_Vertex, StageType_Fragment, StageType_Compute };

	struct BlendState {
		BlendOp blendOp = BlendOp_Add;
		BlendFactor srcFactor = BlendFactor_Zero;
		BlendFactor dstFactor = BlendFactor_Zero;
		operator bool() const { return srcFactor != BlendFactor_Zero || dstFactor != BlendFactor_Zero; }
	};

	struct Attachment {
		Format format = Format_RGBA8;
		LoadOp loadOp = LoadOp_DontCare;
		StoreOp storeOp = StoreOp_Store;
		BlendState blendState = {};
		glm::bvec4 writeMask = glm::bvec4(true);
		bool isDepth = false;
	};

	struct RasterizationState {
		CullMode cullMode = CullMode_None;
		FrontFace frontFace = FrontFace_CCW;
		PolygonMode polygonMode = PolygonMode_Fill;
	};

	struct ResourceBinding {
		uint32_t binding;
		ResourceType type;
	};

	struct ResourceSet {
		uint32_t index;
		std::vector<ResourceBinding> bindings;
	};

	struct ShaderStage {
		StageType type;
		std::string source;
	};

	struct GraphicsStages {
		ShaderStage vert;
		ShaderStage frag;
	};
	
	struct ComputeStages {
		ShaderStage comp;
	};

	struct PipelineBase {
		std::vector<ResourceSet> resources;
	};

	struct IGraphicsPipeline {
		PipelineBase base;
		std::vector<Attachment> attachments;
		RasterizationState rasterizerState;
		CompareOp depthCompareOp;
		bool depthWrite = false;
		bool depthTest = false;
		GraphicsStages stages;
		void cullMode(CullMode x) { rasterizerState.cullMode = x; }
		void frontFace(FrontFace x) { rasterizerState.frontFace = x; }
		void polygonMode(PolygonMode x) { rasterizerState.polygonMode = x; }
		void depthFunc(CompareOp x) { depthCompareOp = x; }
		virtual bool compile() = 0;
		virtual ~IGraphicsPipeline() {};

	};

	struct IComputePipeline {
		PipelineBase base;
		ComputeStages stages;
		uint32_t workGroupSizeX = 1;
		uint32_t workGroupSizeY = 1;
		uint32_t workGroupSizeZ = 1;
		virtual bool compile() = 0;
		virtual ~IComputePipeline() {};
	};
}