#include "spirv_utils.h"

namespace vkjs {
 
    std::vector<DescriptorSetLayoutData> vkjs::get_descriptor_set_layout_data(uint32_t size, const void* code)
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
            for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) {
                const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
                VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
                layout_binding.binding = refl_binding.binding;
                layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
                layout_binding.descriptorCount = 1;
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
        
        std::vector<DescriptorSetLayoutData> out;

        DescriptorSetLayoutData tmpsets[4] = {};
        
        for (size_t i(0); i < 4; ++i) tmpsets[i].set_number = ~0;

        for (const auto& it : layoutData)
        {
            assert(it.set_number < 4);
            tmpsets[it.set_number].set_number = it.set_number;
            for (const auto& it_bind : it.bindings)
            {
                bool skip = true;
                for (const auto& it_search : tmpsets[it.set_number].bindings) {
                    if (it_search.binding == it_bind.binding) break;
                    skip = false;
                }

                if (!skip) {
                    tmpsets[it.set_number].bindings.push_back(it_bind);
                }
            }
        }

        for (size_t i(0); i < 4; ++i) 
        {
            if (tmpsets[i].set_number == ~0)
            {
                continue;
            }
            
            out.emplace_back(DescriptorSetLayoutData{});
            auto& e = out.back();
            e.set_number = i;
            e.bindings = tmpsets[i].bindings;
            e.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            e.create_info.bindingCount = (uint32_t) e.bindings.size();
            e.create_info.pBindings = e.bindings.data();
        }

        std::sort(out.begin(), out.end(), [](const DescriptorSetLayoutData& a, const DescriptorSetLayoutData& b)
            {
                return a.set_number < b.set_number;
            });

        return out;
    }
}