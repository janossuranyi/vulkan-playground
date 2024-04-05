#ifndef VKJS_VERTEX_H_
#define VKJS_VERTEX_H_

#include "vkjs/vkjs.h"
#include "glm/glm.hpp"

namespace jsr {

	struct Vertex {
		float				xyz[4];
		float				uv[2];
		uint16_t			normal[4];
		uint16_t			tangent[4];

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
			positionAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
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
			normalAttribute.format = VK_FORMAT_R16G16B16A16_UNORM;
			normalAttribute.offset = offsetof(Vertex, normal);

			//Tangent will be stored at Location 3
			VkVertexInputAttributeDescription tangentAttribute = {};
			tangentAttribute.binding = 0;
			tangentAttribute.location = 3;
			tangentAttribute.format = VK_FORMAT_R16G16B16A16_UNORM;
			tangentAttribute.offset = offsetof(Vertex, tangent);

			//Color will be stored at Location 4
			/*
			VkVertexInputAttributeDescription colorAttribute = {};
			colorAttribute.binding = 0;
			colorAttribute.location = 4;
			colorAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
			colorAttribute.offset = offsetof(Vertex, color);
			*/
			ret.attributes.push_back(positionAttribute);
			ret.attributes.push_back(uvAttribute);
			ret.attributes.push_back(normalAttribute);
			ret.attributes.push_back(tangentAttribute);
			//ret.attributes.push_back(colorAttribute);

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

		inline void set_position(const float* v) {
			for (size_t i = 0; i < 3; ++i) { xyz[i] = v[i]; }
		}

		inline void set_uv(const float* v) {
			for (size_t i = 0; i < 2; ++i) { uv[i] = v[i]; }
		}

		inline void pack_tangent(const float* v) {
			const glm::vec4 t{ v[0],v[1],v[2],v[3] };
			auto res = round((0.5f + 0.5f * glm::clamp(t, -1.0f, 1.0f)) * 65535.0f);
			tangent[0] = static_cast<uint16_t>(res[0]);
			tangent[1] = static_cast<uint16_t>(res[1]);
			tangent[2] = static_cast<uint16_t>(res[2]);
			tangent[3] = static_cast<uint16_t>(res[3]);
		}

		inline void pack_normal(const float* v)
		{
			const glm::vec3 t{ v[0],v[1],v[2] };
			const glm::vec3 tmp(round((0.5f + 0.5f * glm::clamp(t, -1.0f, 1.0f)) * 65535.0f));
			normal[0] = static_cast<uint16_t>(tmp[0]);
			normal[1] = static_cast<uint16_t>(tmp[1]);
			normal[2] = static_cast<uint16_t>(tmp[2]);
			normal[3] = static_cast<uint16_t>(0);
		}

		inline void pack_color(const float* v)
		{/*
			const glm::vec4 t{ v[0],v[1],v[2],v[3] };
			auto c = round(glm::clamp(t, 0.0f, 1.0f) * 255.0f);
			color[0] = static_cast<uint8_t>(c[0]);
			color[1] = static_cast<uint8_t>(c[1]);
			color[2] = static_cast<uint8_t>(c[2]);
			color[3] = static_cast<uint8_t>(c[3]);*/
		}

	};
	static_assert(sizeof(Vertex) == 40);
}
#endif
