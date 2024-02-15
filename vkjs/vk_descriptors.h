// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vkjs.h"
#include "vk_types.h"

namespace vkutil {
	

	class DescriptorAllocator {
	public:
		static uint32_t allocatedDescriptorCount;

		struct PoolSizes {
			std::vector<std::pair<VkDescriptorType,float>> sizes =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
			};
		};

		void reset_pools();
		bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);
		
		void init(VkDevice newDevice);
		void set_pool_sizes(const PoolSizes& sizes);
		void cleanup();
		
		~DescriptorAllocator();

		VkDevice device;
	private:
		VkDescriptorPool grab_pool();

		VkDescriptorPool currentPool{VK_NULL_HANDLE};
		PoolSizes descriptorSizes;
		std::vector<VkDescriptorPool> usedPools;
		std::vector<VkDescriptorPool> freePools;
	};


	class DescriptorLayoutCache {
	public:
		void init(VkDevice newDevice);
		void cleanup();
		~DescriptorLayoutCache();

		VkDescriptorSetLayout create_descriptor_layout(const VkDescriptorSetLayoutCreateInfo* info);

		struct DescriptorLayoutInfo {
			//good idea to turn this into a inlined array
			std::vector<VkDescriptorSetLayoutBinding> bindings;
			bool operator==(const DescriptorLayoutInfo& other) const;

			size_t hash() const;
		};		
	private:

		struct DescriptorLayoutHash
		{

			std::size_t operator()(const DescriptorLayoutInfo& k) const
			{
				return k.hash();
			}
		};

		std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> layoutCache;
		VkDevice device;
	};

	class DescriptorManager;
	class DescriptorBuilder {
	public:

		static DescriptorBuilder begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator );
		static DescriptorBuilder begin(DescriptorManager* mgr);

		DescriptorBuilder& bind_buffer(uint32_t binding, const VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
		DescriptorBuilder& bind_image(uint32_t binding, const VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
		DescriptorBuilder& bind_buffers(uint32_t binding, uint32_t bufferCount, const VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
		DescriptorBuilder& bind_images(uint32_t binding, uint32_t imageCount, const VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);

		bool build(VkDescriptorSet& set, VkDescriptorSetLayout* layout);
		bool build(VkDescriptorSet& set, VkDescriptorSetLayout layout);
		bool build(VkDescriptorSet& set);
	private:
		
		std::vector<VkWriteDescriptorSet> writes;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		
		VkDescriptorSetLayout dsetLayout;
		DescriptorLayoutCache* cache;
		DescriptorAllocator* alloc;
		DescriptorManager* mgr;
	};

	class DescriptorManager {
	public:
		using DescriptorSetList = std::vector<VkDescriptorSet>;
		using DescriptorSetIt = DescriptorSetList::iterator;

		DescriptorManager() = default;
		~DescriptorManager() {};

		void init(jvk::Device* device);
		VkDescriptorSetLayout create_descriptor_layout(const VkDescriptorSetLayoutCreateInfo* info);
		void reset_pool(VkDescriptorSetLayout layout);
		DescriptorAllocator* get_allocator(VkDescriptorSetLayout layout);
		DescriptorBuilder builder();
		const DescriptorSetList* get_descriptors(VkDescriptorSetLayout layout);
	private:
		

		jvk::Device* _device;
		std::unique_ptr<DescriptorLayoutCache> _pLayoutCache;
		std::unordered_map<uint64_t, std::unique_ptr<DescriptorAllocator>> _pAllocators;
		std::unordered_map<uint64_t, std::unique_ptr<DescriptorSetList>> _descriptorMap;
	};
}

