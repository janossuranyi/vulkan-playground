#pragma once

#include "pch.h"
#include "renderer/Vulkan.h"
#include "renderer/ResourceDescriptions.h"

namespace jsr {

    struct Vertex {
		glm::vec3				xyz;
		glm::vec2				uv;
		glm::vec<4, uint8_t>	normal;
		glm::vec<4, uint8_t>	tangent;
		glm::vec<4, uint8_t>	color;

		void pack_normal(const glm::vec3& v);
		void pack_tangent(const glm::vec4& v);
		void pack_color(const glm::vec4& v);
		
		static VertexInputDescription vertex_input_description();
		static VertexInputDescription vertex_input_description_position_only();
	};

	inline void Vertex::pack_tangent(const glm::vec4& v) {
		tangent = round((0.5f + 0.5f * glm::clamp(v, -1.0f, 1.0f)) * 255.0f);
	}

	inline void Vertex::pack_normal(const glm::vec3& v)
	{
		const glm::vec3 tmp(round((0.5f + 0.5f * glm::clamp(v, -1.0f, 1.0f)) * 255.0f));
		normal = glm::vec4(tmp, 0.0f);
	}

	inline void Vertex::pack_color(const glm::vec4& v)
	{
		color = round(glm::clamp(v, 0.0f, 1.0f) * 255.0f);
	}

	
}