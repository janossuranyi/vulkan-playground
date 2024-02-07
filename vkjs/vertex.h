#ifndef VKJS_VERTEX_H_
#define VKJS_VERTEX_H_

#include "vkjs.h"

namespace vkjs {

	struct VertexInputDescription {

		std::vector<VkVertexInputBindingDescription> bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;

		VkPipelineVertexInputStateCreateFlags flags = 0;
	};

	struct Vertex {
		float				xyz[3];
		glm::vec2				uv;
		glm::vec<4, uint8_t>	normal;
		glm::vec<4, uint8_t>	tangent;
		glm::vec<4, uint8_t>	color;

		static inline VertexInputDescription vertex_input_description() {

			VertexInputDescription ret{};

			//we will have just 1 vertex buffer binding, with a per-vertex rate
			VkVertexInputBindingDescription mainBinding = {};
			mainBinding.binding = 0;
			mainBinding.stride = sizeof(Vertex);
			mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			ret.bindings.push_back(mainBinding);

			//Position will be stored at Location 0
			VkVertexInputAttributeDescription positionAttribute = {};
			positionAttribute.binding = 0;
			positionAttribute.location = 0;
			positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			positionAttribute.offset = offsetof(Vertex, xyz);

			//UV will be stored at Location 1
			VkVertexInputAttributeDescription uvAttribute = {};
			uvAttribute.binding = 0;
			uvAttribute.location = 1;
			uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
			uvAttribute.offset = offsetof(Vertex, uv);

			//Normal will be stored at Location 2
			VkVertexInputAttributeDescription normalAttribute = {};
			normalAttribute.binding = 0;
			normalAttribute.location = 2;
			normalAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
			normalAttribute.offset = offsetof(Vertex, normal);

			//Tangent will be stored at Location 3
			VkVertexInputAttributeDescription tangentAttribute = {};
			tangentAttribute.binding = 0;
			tangentAttribute.location = 3;
			tangentAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
			tangentAttribute.offset = offsetof(Vertex, tangent);

			//Color will be stored at Location 4
			VkVertexInputAttributeDescription colorAttribute = {};
			colorAttribute.binding = 0;
			colorAttribute.location = 4;
			colorAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
			colorAttribute.offset = offsetof(Vertex, color);

			ret.attributes.push_back(positionAttribute);
			ret.attributes.push_back(uvAttribute);
			ret.attributes.push_back(normalAttribute);
			ret.attributes.push_back(tangentAttribute);
			ret.attributes.push_back(colorAttribute);

			return ret;
		}
		static inline VertexInputDescription vertex_input_description_position_only()
		{
			VertexInputDescription ret{};

			//we will have just 1 vertex buffer binding, with a per-vertex rate
			VkVertexInputBindingDescription mainBinding = {};
			mainBinding.binding = 0;
			mainBinding.stride = sizeof(Vertex);
			mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			ret.bindings.push_back(mainBinding);

			//Position will be stored at Location 0
			VkVertexInputAttributeDescription positionAttribute = {};
			positionAttribute.binding = 0;
			positionAttribute.location = 0;
			positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
			positionAttribute.offset = offsetof(Vertex, xyz);

			ret.attributes.push_back(positionAttribute);

			return ret;
		}

		inline void pack_tangent(const glm::vec4& v) {
			tangent = round((0.5f + 0.5f * glm::clamp(v, -1.0f, 1.0f)) * 255.0f);
		}

		inline void pack_normal(const glm::vec3& v)
		{
			const glm::vec3 tmp(round((0.5f + 0.5f * glm::clamp(v, -1.0f, 1.0f)) * 255.0f));
			normal = glm::vec4(tmp, 0.0f);
		}

		inline void pack_color(const glm::vec4& v)
		{
			color = round(glm::clamp(v, 0.0f, 1.0f) * 255.0f);
		}

	};
}
#endif
