#pragma once

#include "pch.h"

namespace common {
	struct Vertex {
		glm::vec3				xyz;
		glm::vec2				uv;
		glm::vec<4, uint8_t>	normal;
		glm::vec<4, uint8_t>	tangent;
		glm::vec<4, uint8_t>	color;

		void pack_normal(const glm::vec3& v);
		void pack_tangent(const glm::vec4& v);
		void pack_color(const glm::vec4& v);
	};

	inline void Vertex::pack_tangent(const glm::vec4& v) {
		tangent = glm::clamp(glm::vec4(1.0f) + v, 0.0f, 2.0f) * 127.5f;
	}

	inline void Vertex::pack_normal(const glm::vec3& v)
	{
		normal = glm::clamp(glm::vec4(1.0f + v, 0.0f), 0.0f, 2.0f) * 127.5f;
	}

	inline void Vertex::pack_color(const glm::vec4& v)
	{
		color = glm::clamp(255.0f * v, 0.0f, 255.0f);
	}
}