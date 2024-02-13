#pragma once

#include "pch.h"
#include "vkjs/vkjs.h"

namespace jsr {

	enum class LightType { Point, Spot, Directional };

	const int LightType_Directional = 0;
	const int LightType_Point = 1;
	const int LightType_Spot = 2;

	struct Light
	{
		float direction[3];
		float range;

		float color[3];
		float intensity;

		float position[3];
		float innerConeCos;

		float outerConeCos;
		int type;
		int dummy0;
		int dummy1;

		void set_direction(const glm::vec3& v);
		void set_position(const glm::vec3& v);
		void set_color(const glm::vec3& v);
	};
	static_assert((sizeof(Light) & 15) == 0);

	inline void Light::set_direction(const glm::vec3& v)
	{		
		direction[0] = v[0];
		direction[1] = v[1];
		direction[2] = v[2];
	}

	inline void Light::set_position(const glm::vec3& v)
	{
		position[0] = v[0];
		position[1] = v[1];
		position[2] = v[2];
	}

	inline void Light::set_color(const glm::vec3& v)
	{
		color[0] = v[0];
		color[1] = v[1];
		color[2] = v[2];
	}
}