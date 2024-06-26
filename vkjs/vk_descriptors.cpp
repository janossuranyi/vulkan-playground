﻿#include "vkjs.h"
#include "vk_descriptors.h"
#include <algorithm>

namespace vkutil {
	uint32_t DescriptorAllocator::allocatedDescriptorCount = 0;

	VkDescriptorPool createPool(VkDevice device, const DescriptorAllocator::PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags)
	{
		std::vector<VkDescriptorPoolSize> sizes;
		sizes.reserve(poolSizes.sizes.size());
		for (auto sz : poolSizes.sizes) {
			sizes.push_back({ sz.first, uint32_t(sz.second * count) });
		}
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = flags;
		pool_info.maxSets = count;
		pool_info.poolSizeCount = (uint32_t)sizes.size();
		pool_info.pPoolSizes = sizes.data();

		VkDescriptorPool descriptorPool;
		vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);

		return descriptorPool;
	}

	void DescriptorAllocator::reset_pools()
	{
		for (auto p : usedPools)
		{
			vkResetDescriptorPool(device, p, 0);
		}

		freePools = std::move(usedPools);
		currentPool = VK_NULL_HANDLE;
	}

	bool DescriptorAllocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
	{
		if (currentPool == VK_NULL_HANDLE)
		{
			currentPool = grab_pool();
			usedPools.push_back(currentPool);
		}

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;

		allocInfo.pSetLayouts = &layout;
		allocInfo.descriptorPool = currentPool;
		allocInfo.descriptorSetCount = 1;		
		

		VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
		bool needReallocate = false;

		switch (allocResult) {
		case VK_SUCCESS:
			//all good, return
			return true;

			break;
		case VK_ERROR_FRAGMENTED_POOL:
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			//reallocate pool
			needReallocate = true;
			break;
		default:
			//unrecoverable error
			return false;
		}
		
		if (needReallocate)
		{
			//allocate a new pool and retry
			currentPool = grab_pool();
			usedPools.push_back(currentPool);
			allocInfo.descriptorPool = currentPool;
			allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);

			//if it still fails then we have big issues
			if (allocResult == VK_SUCCESS)
			{
				return true;
			}
		}

		return false;
	}

	void DescriptorAllocator::init(VkDevice newDevice)
	{
		device = newDevice;
	}

	void DescriptorAllocator::set_pool_sizes(const PoolSizes& sizes)
	{
		descriptorSizes = sizes;
	}

	void DescriptorAllocator::cleanup()
	{
		//delete every pool held
		for (auto p : freePools)
		{
			vkDestroyDescriptorPool(device, p, nullptr);
		}
		for (auto p : usedPools)
		{
			vkDestroyDescriptorPool(device, p, nullptr);
		}
		freePools.clear();
		usedPools.clear();
	}

	DescriptorAllocator::~DescriptorAllocator()
	{
		cleanup();
	}

	VkDescriptorPool DescriptorAllocator::grab_pool()
	{
		if ( ! freePools.empty() )
		{
			VkDescriptorPool pool = freePools.back();
			freePools.pop_back();
			return pool;
		}
		else {
			return createPool(device, descriptorSizes, 1000, 0);
		}
	}


	void DescriptorLayoutCache::init(VkDevice newDevice)
	{
		device = newDevice;
	}

	VkDescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(const VkDescriptorSetLayoutCreateInfo* info)
	{
		DescriptorLayoutInfo layoutinfo;
		layoutinfo.bindings.reserve(info->bindingCount);

		bool isSorted = true;
		int32_t lastBinding = -1;
		for (uint32_t i = 0; i < info->bindingCount; i++) {
			layoutinfo.bindings.push_back(info->pBindings[i]);

			//check that the bindings are in strict increasing order
			if (static_cast<int32_t>(info->pBindings[i].binding) > lastBinding)
			{
				lastBinding = info->pBindings[i].binding;
			}
			else{
				isSorted = false;
			}
		}
		if (!isSorted)
		{
			std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(), 
				[](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b )
				{
					return a.binding < b.binding;
				});
		}

		auto it = layoutCache.find(layoutinfo);
		if (it != layoutCache.end())
		{
			return it->second;
		}
		else
		{
			VkDescriptorSetLayout layout;
			vkCreateDescriptorSetLayout(device, info, nullptr, &layout);

			//layoutCache.emplace()
			//add to cache
			layoutCache[layoutinfo] = layout;
			return layout;
		}
	}

	void DescriptorLayoutCache::cleanup()
	{
		//delete every descriptor layout held
		for (auto pair : layoutCache)
		{
			vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
		}
		layoutCache.clear();
	}

	DescriptorLayoutCache::~DescriptorLayoutCache()
	{
		cleanup();
	}

	DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator)
	{
		DescriptorBuilder builder;
		
		builder.m_pCache = layoutCache;
		builder.m_pAlloc = allocator;
		return builder;
	}

	DescriptorBuilder DescriptorBuilder::begin(DescriptorManager* mgr)
	{
		DescriptorBuilder builder;

		builder.m_pMgr = mgr;
		builder.m_pAlloc = nullptr;
		builder.m_pCache = nullptr;

		return builder;
	}


	DescriptorBuilder& DescriptorBuilder::bind_buffer(uint32_t binding, const VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t arrayIndex)
	{
		return bind_buffers(binding, 1, bufferInfo, type, stageFlags, arrayIndex);
	}

	DescriptorBuilder& DescriptorBuilder::bind_image(uint32_t binding, const VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t arrayIndex)
	{
		return bind_images(binding, 1, imageInfo, type, stageFlags, arrayIndex);
	}

	DescriptorBuilder& DescriptorBuilder::bind_buffers(uint32_t binding, uint32_t bufferCount, const VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t arrayIndex)
	{
		VkDescriptorSetLayoutBinding newBinding{};

		newBinding.descriptorCount = bufferCount;
		newBinding.descriptorType = type;
		newBinding.pImmutableSamplers = nullptr;
		newBinding.stageFlags = stageFlags;
		newBinding.binding = binding;

		m_bindings.push_back(newBinding);

		VkWriteDescriptorSet newWrite{};
		newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		newWrite.pNext = nullptr;

		newWrite.descriptorCount = bufferCount;
		newWrite.descriptorType = type;
		newWrite.pBufferInfo = bufferInfo;
		newWrite.dstBinding = binding;
		newWrite.dstArrayElement = arrayIndex;

		m_writes.push_back(newWrite);

		return *this;
	}

	DescriptorBuilder& DescriptorBuilder::bind_images(uint32_t binding, uint32_t imageCount, const VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t arrayIndex)
	{
		VkDescriptorSetLayoutBinding newBinding{};

		newBinding.descriptorCount = imageCount;
		newBinding.descriptorType = type;
		newBinding.pImmutableSamplers = nullptr;
		newBinding.stageFlags = stageFlags;
		newBinding.binding = binding;

		m_bindings.push_back(newBinding);

		VkWriteDescriptorSet newWrite{};
		newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		newWrite.pNext = nullptr;

		newWrite.descriptorCount = imageCount;
		newWrite.descriptorType = type;
		newWrite.pImageInfo = imageInfo;
		newWrite.dstBinding = binding;
		newWrite.dstArrayElement = arrayIndex;

		m_writes.push_back(newWrite);
		return *this;
	}

	bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout* layout)
	{
		//build layout first
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = nullptr;

		layoutInfo.pBindings = m_bindings.data();
		layoutInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());

		if (m_pMgr) {
			*layout = m_pMgr->create_descriptor_layout(&layoutInfo);
		}
		else
		{
			*layout = m_pCache->create_descriptor_layout(&layoutInfo);
		}

		return build(set, *layout);
	}

	bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout layout)
	{
		//allocate descriptor
		bool success;
		
		if (m_pMgr)
		{
			m_pAlloc = m_pMgr->get_allocator(layout);
		}
		
		assert(m_pAlloc);
		success = m_pAlloc->allocate(&set, layout);

		if (!success) { return false; };

		//write descriptor

		for (VkWriteDescriptorSet& w : m_writes) {
			w.dstSet = set;
		}

		vkUpdateDescriptorSets(m_pAlloc->device, static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);

		return true;
	}


	bool DescriptorBuilder::build(VkDescriptorSet& set)
	{
		VkDescriptorSetLayout layout;
		return build(set, &layout);
	}


	bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
	{
		if (other.bindings.size() != bindings.size())
		{
			return false;
		}
		else {
			//compare each of the bindings is the same. Bindings are sorted so they will match
			for (int i = 0; i < bindings.size(); i++) {
				if (other.bindings[i].binding != bindings[i].binding)
				{
					return false;
				}
				if (other.bindings[i].descriptorType != bindings[i].descriptorType)
				{
					return false;
				}
				if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
				{
					return false;
				}
				if (other.bindings[i].stageFlags != bindings[i].stageFlags)
				{
					return false;
				}
			}
			return true;
		}
	}

	size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const
	{
		using std::size_t;
		using std::hash;

		size_t result = hash<size_t>()(bindings.size());

		for (const VkDescriptorSetLayoutBinding& b : bindings)
		{
			//pack the binding data into a single int64. Not fully correct but its ok
			size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

			//shuffle the packed binding data and xor it with the main hash
			result ^= hash<size_t>()(binding_hash);
		}

		return result;
	}

	void DescriptorManager::init(jvk::Device* device)
	{
		assert(device);
		_device = device;
		_pLayoutCache = std::make_unique<DescriptorLayoutCache>();
		_pLayoutCache->init(*_device);
	}

	VkDescriptorSetLayout DescriptorManager::create_descriptor_layout(const VkDescriptorSetLayoutCreateInfo* info)
	{
		auto res = _pLayoutCache->create_descriptor_layout(info);

		if (res != VK_NULL_HANDLE && _pAllocators.find((size_t)res) == _pAllocators.end())
		{

			DescriptorAllocator::PoolSizes ps;
			ps.sizes.clear();

			std::unordered_map<VkDescriptorType, uint32_t> typeCounter;
			for (uint32_t i = 0; i < info->bindingCount; ++i)
			{
				typeCounter[info->pBindings[i].descriptorType] += info->pBindings[i].descriptorCount;
			}

			for (const auto& it : typeCounter)
			{
				ps.sizes.emplace_back(it.first, (float)it.second);
			}

			// Create an allocator
			DescriptorAllocator* p = new DescriptorAllocator();
			p->init(*_device);
			p->set_pool_sizes(ps);
			_pAllocators.emplace((uint64_t)res, p);
			_descriptorMap.emplace((uint64_t)res, new DescriptorSetList());
		}
		return res;
	}

	void DescriptorManager::reset_pool(VkDescriptorSetLayout layout)
	{
		auto it = _pAllocators.find((size_t)layout);
		if (it != _pAllocators.end())
		{
			it->second->reset_pools();
		}
	}

	DescriptorAllocator* DescriptorManager::get_allocator(VkDescriptorSetLayout layout)
	{
		auto it = _pAllocators.find((size_t)layout);
		if (it != _pAllocators.end())
		{
			return it->second.get();
		}

		return nullptr;
	}

	DescriptorBuilder DescriptorManager::builder()
	{
		return DescriptorBuilder::begin(this);
	}

	const DescriptorManager::DescriptorSetList* DescriptorManager::get_descriptors(VkDescriptorSetLayout layout)
	{
		DescriptorManager::DescriptorSetList* res = nullptr;
		auto it = _descriptorMap.find((uint64_t)layout);
		if (it != _descriptorMap.end())
		{
			res = it->second.get();
		}

		return res;
	}

}