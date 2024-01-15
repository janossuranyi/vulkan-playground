#ifndef VKJS_SPIRV_UTILS_H_
#define VKJS_SPIRV_UTILS_H_

#include "vkjs.h"
#include "spirv_reflect.h"

namespace vkjs {
	struct DescriptorSetLayoutData {
		uint32_t set_number;
		VkDescriptorSetLayoutCreateInfo create_info;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
	};

	std::vector<DescriptorSetLayoutData> get_descriptor_set_layout_data(uint32_t size, const void* code);
	std::vector<DescriptorSetLayoutData> merge_descriptor_set_layout_data(const std::vector<DescriptorSetLayoutData>& layoutData);

}

#endif // !VKJS_SPIRV_UTILS_H_
