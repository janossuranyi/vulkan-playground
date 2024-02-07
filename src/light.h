#pragma once

#include "pch.h"

namespace jsr {

	enum class LightType { Point, Spot, Directional };

	const int LightType_Directional = 0;
	const int LightType_Point = 1;
	const int LightType_Spot = 2;

	struct Light
	{
		glm::vec3 direction;
		float range;

		glm::vec3 color;
		float intensity;

		glm::vec3 position;
		float innerConeCos;

		float outerConeCos;
		int type;
		int dummy0;
		int dummy1;
	};
	static_assert((sizeof(Light) & 15) == 0);
}