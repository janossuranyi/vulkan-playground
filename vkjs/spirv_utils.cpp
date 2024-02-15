#include "spirv_utils.h"
#include "VulkanInitializers.hpp"

namespace jvk {
 
    std::vector<DescriptorSetLayoutData> jvk::get_descriptor_set_layout_data(uint32_t size, const void* code)
    {
        SpvReflectShaderModule module = {};
        SpvReflectResult result = spvReflectCreateShaderModule(size, code, &module);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        uint32_t count = 0;
        result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectDescriptorSet*> sets(count);
        result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        // Demonstrates how to generate all necessary data structures to create a
        // VkDescriptorSetLayout for each descriptor set in this shader.
        std::vector<DescriptorSetLayoutData> set_layouts(sets.size(), DescriptorSetLayoutData{});
        for (size_t i_set = 0; i_set < sets.size(); ++i_set) {
            const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);
            DescriptorSetLayoutData& layout = set_layouts[i_set];
            layout.bindings.resize(refl_set.binding_count);
            layout.binding_typename.resize(refl_set.binding_count);
            for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding)
            {
                const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
                VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
                layout_binding.binding = refl_binding.binding;
                layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
                layout_binding.descriptorCount = 1;
                
                if (refl_binding.type_description->type_name) {
                    layout.binding_typename[i_binding] = refl_binding.type_description->type_name;
                }

                for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
                    layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
                }
                layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
            }
            layout.set_number = refl_set.set;
            layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout.create_info.bindingCount = refl_set.binding_count;
            layout.create_info.pBindings = layout.bindings.data();
        }

        return set_layouts;
    }

    std::vector<DescriptorSetLayoutData> merge_descriptor_set_layout_data(const std::vector<DescriptorSetLayoutData>& layoutData)
    {
        struct bind_t {
            VkDescriptorSetLayoutBinding binding;
            std::string type_name;
        };

        std::vector<DescriptorSetLayoutData> out;
        std::map<size_t, bind_t> bindingMap;
        bool set_exists[16] = {};

        for (size_t i_layoutData(0); i_layoutData < layoutData.size(); ++i_layoutData)
        {
            const auto& layout = layoutData[i_layoutData];

            assert(layout.set_number < 16);
            if (!set_exists[layout.set_number])
            {
                set_exists[layout.set_number] = true;
                if (out.size() < (layout.set_number + 1)) {
                    out.resize(layout.set_number + 1);
                    out[layout.set_number] = {};
                    out[layout.set_number].set_number = layout.set_number;
                }
            }

            for (size_t i_binding(0); i_binding < layout.bindings.size(); ++i_binding)
            {
                const auto& srcBinding = layout.bindings[i_binding];
                const size_t bindKey = (layout.set_number << 16 | srcBinding.binding);

                if (bindingMap.find(bindKey) == bindingMap.end()) {
                    bindingMap[bindKey].binding = srcBinding;
                    bindingMap[bindKey].type_name = layout.binding_typename[i_binding];                   
                }
                else {
                    bindingMap[bindKey].binding.stageFlags |= srcBinding.stageFlags;
                }                    
            }
        }


        for (const auto& it : bindingMap)
        {
            size_t set = it.first >> 16;
            size_t binding = it.first & 0xffff;
            out[set].bindings.push_back(it.second.binding);
            out[set].binding_typename.push_back(it.second.type_name);
        }

        for (auto& it : out)
        {
            for (auto& bind : it.bindings) {
                if (bind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && it.binding_typename[bind.binding].substr(0, 3) == "dyn")
                {   
                    bind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                }
                else if (bind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER && it.binding_typename[bind.binding].substr(0, 3) == "dyn")
                {
                    bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                }
            }

            it.create_info = vks::initializers::descriptorSetLayoutCreateInfo(it.bindings);
            
        }

        return out;
    }
    std::vector<DescriptorSetLayoutData> extract_descriptor_set_layout_data(const std::vector<const ShaderModule*>& modules)
    {

        std::vector<DescriptorSetLayoutData> in;
        for (const auto& module : modules) {
            auto tmp = get_descriptor_set_layout_data(module->size(), module->data());
            in.insert(in.end(), tmp.begin(), tmp.end());
        }

        return merge_descriptor_set_layout_data(in);
    }
}