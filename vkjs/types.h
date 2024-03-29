#ifndef VKJS_TYPES_H_
#define VKJS_TYPES_H_

#include <cinttypes>

namespace jvk {

	typedef uint64_t	u64;
	typedef int64_t		s64;
	typedef uint32_t	u32;
	typedef int32_t		s32;
	typedef uint16_t	u16;
	typedef int16_t		s16;
	typedef uint8_t		u8;
	typedef int8_t		s8;
	typedef uint32_t	uint;
}

struct VertexInputDescription {

	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

#endif // !VKJS_TYPES_H_
