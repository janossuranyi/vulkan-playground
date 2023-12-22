#ifndef RESOURCE_DESCRIPTIONS_H_
#define RESOURCE_DESCRIPTIONS_H_

#include "pch.h"
#include "common/resource_handles.h"
#include "vk.h"

#define INFLIGHT_FRAMES 2

struct ShaderLayout {
	std::vector<uint32_t> samplerBindings;
	std::vector<uint32_t> sampledImageBindings;
	std::vector<uint32_t> storageImageBindings;
	std::vector<uint32_t> uniformBufferBindings;
	std::vector<uint32_t> storageBufferBindings;
};

struct ImageResource {
	ImageResource(ImageHandle image, uint32_t binding, uint32_t level) :
		image(image), binding(binding), level(level) {}
	ImageHandle image;
	uint32_t binding;
	uint32_t level;
};

struct SamplerResource {
	SamplerResource(SamplerHandle sampler, uint32_t binding) :
		sampler(sampler), binding(binding) {}
	SamplerHandle sampler;
	uint32_t binding;
};

struct StorageBufferResource {
	StorageBufferResource(
		const StorageBufferHandle   buffer,
		const bool                  readOnly,
		const uint32_t              binding) : buffer(buffer), readOnly(readOnly), binding(binding) {};
	StorageBufferHandle buffer;
	bool                readOnly;
	uint32_t            binding;
};

struct UniformBufferResource {
	UniformBufferResource(
		const UniformBufferHandle   buffer,
		const uint32_t              binding) : buffer(buffer), binding(binding) {};
	UniformBufferHandle buffer;
	uint32_t            binding;
};

struct UniformBufferDescription {
	UniformBufferDescription(uint32_t size, const void* initialData) :
		size(size), initialData(initialData){}

	size_t size;
	const void* initialData;
};

struct StorageBufferDescription {
	StorageBufferDescription(uint32_t size, const void* initialData) :
		size(size), initialData(initialData) {}

	size_t size;
	const void* initialData;
};

struct RenderPassResources {
	std::vector<SamplerResource>        samplers;
	std::vector<StorageBufferResource>  storageBuffers;
	std::vector<UniformBufferResource>  uniformBuffers;
	std::vector<ImageResource>          sampledImages;
	std::vector<ImageResource>          storageImages;
};

//generic info for a renderpass
struct RenderPassExecution {
	RenderPassHandle                handle;
	RenderPassResources             resources;
};

struct RenderTarget {
	ImageHandle image;
	uint32_t mipLevel = 0;
};

//contains RenderPassExecution and additional info for graphic pass
struct GraphicPassExecution {
	RenderPassExecution genericInfo;
	std::vector<RenderTarget> targets;
};

//contains RenderPassExecution and additional info for compute pass
struct ComputePassExecution {
	RenderPassExecution genericInfo;
	std::vector<char> pushConstants;
	uint32_t dispatchCount[3] = { 1, 1, 1 };
};

struct MeshDescription {
	uint32_t vertexCount;
	uint32_t indexCount;
	std::vector<char> vertexBuffer;
	std::vector<uint16_t> indexBuffer;
};

struct GraphichPipelineShaderBinary {
	std::vector<uint8_t> vertexSPIRV;
	std::vector<uint8_t> fragmentSPIRV;
	std::vector<uint8_t> geometrySPIRV;
};

struct ComputePipelineShaderBinary {
	std::vector<uint8_t> computeSPIRV;
};

enum class BlendOp { None, Add, Subtract };
enum class BlendFactor { One, OneMinusSrcAlpha };
enum class PolygonMode { Fill, Line };
enum class CullMode { None, Front, Back, FrontAndBack };
enum class FrontFace { Clockwise, CounterClockwise };
enum class CompareOp { None, Always, Never, Less, LessThanOrEqual, Greater, GreaterThanOrEqual };
enum class VertexAttribFormat { Full, PositionOnly };

enum class ImageUsageFlags : uint32_t {
	Storage = 0x00000001,
	Sampled = 0x00000002,
	Attachment = 0x00000004,
};

inline ImageUsageFlags operator&(const ImageUsageFlags l, const ImageUsageFlags r) {
	return ImageUsageFlags(uint32_t(l) & uint32_t(r));
}

inline ImageUsageFlags operator|(const ImageUsageFlags l, const ImageUsageFlags r) {
	return ImageUsageFlags(uint32_t(l) | uint32_t(r));
}

enum class MipCount { One, FullChain, Manual, FullChainAlreadyInData };

struct ImageDescription {
	uint32_t width = 1;
	uint32_t height = 0;
	uint32_t depth = 0;

	VkImageType		type = VK_IMAGE_TYPE_1D;
	VkFormat		format = VK_FORMAT_R8_UNORM;
	ImageUsageFlags usageFlags = (ImageUsageFlags)0;

	MipCount    mipCount = MipCount::One;
	uint32_t    manualMipCount = 1; //only used if mipCount is Manual
	bool        autoCreateMips = false;
	bool		cubeMap = false;
	std::string	name;
};

struct RasterizationConfig {
	PolygonMode mode = PolygonMode::Fill;
	CullMode cullMode = CullMode::None;
	FrontFace frontFace = FrontFace::CounterClockwise;
};

enum PrimitiveTopology { TriangleList, LineList };

struct DepthStencilTest {
	bool depthWrite = false;
	CompareOp depthFunction = CompareOp::None;
};

struct InputAssemblyInfo {
	PrimitiveTopology topology = PrimitiveTopology::TriangleList;
	bool primitiveRestartEnable = false;
};

struct BlendingConfig {
	bool writeMask[4] = { true,true,true,true };
	BlendOp blendOp = BlendOp::None;
	BlendFactor srcBlendFactor = BlendFactor::One;
	BlendFactor dstBlendFactor = BlendFactor::One;
};

enum ResourceType { SampledImage, Sampler, CombinedImage, UniformBuffer, UniformBufferDyn, StorageBuffer, StorageBufferDyn };

struct ResourceBinding {
	uint32_t binding;
	ResourceType resourceType;
};

enum class AttachmentType { Color, DepthStencil };

struct Attachment {
	Attachment(
		const VkFormat       format,
		const VkAttachmentLoadOp  loadOp
	) : format(format), loadOp(loadOp) {};

	VkFormat format = VK_FORMAT_R8_UNORM;
	VkAttachmentLoadOp    loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
};

enum class SamplerInterpolation { Nearest, Linear };
enum class SamplerWrapping { Clamp, Color, Repeat };
enum class SamplerBorderColor { White, Black };

struct SamplerDescription {
	SamplerInterpolation    interpolation = SamplerInterpolation::Nearest;
	SamplerWrapping         wrapping = SamplerWrapping::Repeat;
	bool                    useAnisotropy = false;
	float                   maxAnisotropy = 8;                            //only used if useAnisotropy is true
	SamplerBorderColor      borderColor = SamplerBorderColor::Black;    //only used if wrapping is Color
	uint32_t                maxMip = 1;
};

struct AttachmentInfo {
	uint32_t location;
	AttachmentType type;
	ImageHandle image;
};

struct SpecialisationConstant {
	uint32_t location;
	std::vector<char> data;
};

struct ShaderDescription {
	std::filesystem::path path;
	std::vector<SpecialisationConstant> specialisationConstants;
};

struct GraphicPassShaderDescriptions {
	ShaderDescription                 vertex;
	ShaderDescription                 fragment;
	std::optional<ShaderDescription>  geometry;
	std::optional<ShaderDescription>  tessCtrl;
	std::optional<ShaderDescription>  tessEval;
};

struct ComputePassShaderDescriptions {
	ShaderDescription                 compute;
};

struct GraphicsPassDescription {
	GraphicPassShaderDescriptions shaderDescriptions;
	ShaderLayout layout;
	std::vector<Attachment> attachments;
	VertexAttribFormat vertexAttribFormat = VertexAttribFormat::Full;
	BlendingConfig blend;
	RasterizationConfig rasterization;
	DepthStencilTest depthtest;
	InputAssemblyInfo inputAssembly;
	uint32_t pushConstantsSize = 0;
	std::string name;
};

struct ComputePassDescription {
	ShaderDescription compute;
	ShaderLayout layout;
	std::string name;
};

#endif // !JS_RESOURCE_DESCRIPTIONS_H_
