#ifndef VKJS_PIPELINE_H_
#define VKJS_PIPELINE_H_

#include "vkjs.h"
#include "span.h"
#include "vk_descriptors.h"
#include "vertex.h"

namespace jvk {

	class ShaderModule;

	struct PipelineBuilder {

		std::vector<VkPipelineShaderStageCreateInfo>		_shaderStages;
		VkPipelineVertexInputStateCreateInfo				_vertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo				_inputAssembly;
		VkPipelineRasterizationStateCreateInfo				_rasterizer;
		std::vector<VkPipelineColorBlendAttachmentState>	_colorBlendAttachments;
		VkPipelineMultisampleStateCreateInfo				_multisampling;
		VkPipelineLayout									_pipelineLayout;
		VkPipelineDepthStencilStateCreateInfo				_depthStencil;
		VkPipelineDynamicStateCreateInfo					_dynamicStates;
		const void*											_pNext;
		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
		VkResult result = VK_ERROR_UNKNOWN;
	};

	struct PipelineCommandBuffer {
		VkCommandBuffer cmd;
		VkCommandPool pool;
	};

	struct GraphicsShaderInfo {
		ShaderModule* vert;
		ShaderModule* frag;
		ShaderModule* geom;
	};


	class VulkanPipeline {
	public:
		static const uint32_t MAX_DESCRIPTOR_SET_COUNT = 4;
		static const uint32_t MAX_BINDING_COUNT = 32;

		enum class BindingType { Image, Sampler, Buffer, Empty };

		struct ShaderResource {
			uint32_t binding = ~0;
			BindingType type = BindingType::Empty;
			uint32_t descriptorCount = 0;
			union {
				const VkDescriptorImageInfo* image;
				const VkDescriptorBufferInfo* buffer;
			};
		};

		typedef std::array<ShaderResource, MAX_BINDING_COUNT> ResourceBindings;

		virtual ~VulkanPipeline() {}
		virtual VulkanPipeline& begin();
		virtual VkPipelineBindPoint pipeline_bind_point() const = 0;
		virtual void bind(VkCommandBuffer cmd) = 0;
		virtual VkResult build_pipeline() = 0;
		VulkanPipeline& bind_image(uint32_t index, uint32_t binding, const span<VkDescriptorImageInfo> inf);
		VulkanPipeline& bind_buffer(uint32_t index, uint32_t binding, const span<VkDescriptorBufferInfo> inf);
		VulkanPipeline& bind_sampler(uint32_t index, uint32_t binding, const span<VkDescriptorImageInfo> inf);
		void set_name(const std::string& name);
		VkPipeline pipeline() const;
		VkPipelineLayout pipeline_layout() const;
		VkDescriptorPool descriptor_pool() const;
		void bind_descriptor_sets(VkCommandBuffer cmd, uint32_t count, const VkDescriptorSet* sets, uint32_t dynamicOffsetCount = 0, const uint32_t* dynamicOffsets = nullptr);
	protected:

		void prepare();

		struct Bindings {
			uint32_t bindingCount;
			std::array<VkDescriptorSetLayoutBinding, MAX_BINDING_COUNT> bindings;
		};

		VkPipeline _pipeline;
		VkPipelineLayout _pipelineLayout;
		VkDescriptorPool _descriptorPool;

		uint32_t _descriptorSetCount = 0;
		std::string _name;
		std::array<Bindings, MAX_DESCRIPTOR_SET_COUNT> _descriptorSetsInfo;
		std::array<VkDescriptorSetLayout, MAX_DESCRIPTOR_SET_COUNT> _descriptorLayouts;
		std::array<ResourceBindings, MAX_DESCRIPTOR_SET_COUNT> _resourceBindings;
	};

	/**
	 ** Graphics Pipeline
	 **/
	class GraphicsPipeline : public VulkanPipeline {
	
	public:		
		static const uint32_t MAX_PARALELL_CMD_BUFFER_COUNT = 16;

		GraphicsPipeline(Device* dev, const GraphicsShaderInfo& shaderInfo, vkutil::DescriptorManager* descMgr);

		~GraphicsPipeline() override;

		// Inherited via VulkanPipeline
		VkPipelineBindPoint pipeline_bind_point() const override;

		void bind(VkCommandBuffer cmd) override;
		GraphicsPipeline& set_polygon_mode(VkPolygonMode polygonMode);
		GraphicsPipeline& set_cull_mode(VkCullModeFlags flags);
		GraphicsPipeline& set_front_face(VkFrontFace face);
		GraphicsPipeline& set_depth_test(bool b);
		GraphicsPipeline& set_depth_mask(bool b);
		GraphicsPipeline& set_depth_func(VkCompareOp f);
		GraphicsPipeline& set_draw_mode(VkPrimitiveTopology t);
		GraphicsPipeline& set_sample_count(VkSampleCountFlagBits flags);
		GraphicsPipeline& set_sample_shading(bool enable, float f);
		GraphicsPipeline& set_vertex_input_description(const VertexInputDescription& vinp);
		GraphicsPipeline& set_render_pass(VkRenderPass p);
		GraphicsPipeline& set_next_chain(const void* ptr);
		GraphicsPipeline& add_dynamic_state(VkDynamicState s);
		GraphicsPipeline& add_attachment_blend_state(const VkPipelineColorBlendAttachmentState& r);

		VkResult build_pipeline() override;
	private:
		Device* _device;
		uint32_t _paralellCmdBufferCount = 0;
		std::array<PipelineCommandBuffer, MAX_PARALELL_CMD_BUFFER_COUNT> _paralellCmdBuffers;
		struct {
			VkShaderModule vert;
			VkShaderModule frag;
		} _shaders;
		PipelineBuilder _builder{};
		vkutil::DescriptorManager* _descMgr;
		VkPolygonMode _polygonMode = VK_POLYGON_MODE_FILL;
		VkCullModeFlags _cullMode = VK_CULL_MODE_BACK_BIT;
		VkFrontFace _frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		VkPrimitiveTopology _drawMode = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		VkRenderPass _renderPass = VK_NULL_HANDLE;
		bool _depthTest = false;
		bool _depthMask = false;
		bool _sampleShading = false;
		float _minSampleShading = 0.0f;

		VkCompareOp _depthCompareOp = VK_COMPARE_OP_ALWAYS;
		VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT;
		std::vector<VkDynamicState> _dynamicStates;
	};


	class ComputePipeline : public VulkanPipeline {};

	inline VkPipeline VulkanPipeline::pipeline() const
	{
		return _pipeline;
	}
	inline VkPipelineLayout VulkanPipeline::pipeline_layout() const
	{
		return _pipelineLayout;
	}
	inline VkDescriptorPool VulkanPipeline::descriptor_pool() const
	{
		return _descriptorPool;
	}
	inline void VulkanPipeline::bind_descriptor_sets(VkCommandBuffer cmd, uint32_t count, const VkDescriptorSet* sets, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
	{
		vkCmdBindDescriptorSets(cmd, pipeline_bind_point(), _pipelineLayout, 0, count, sets, dynamicOffsetCount, dynamicOffsets);
	}
}
#endif // !VKJS_PIPELINE_H_
