#pragma once

namespace jsrlib {

    float fadd(float a, float b);

	inline float lerp(float v0, float v1, float t) {
		return (1 - t) * v0 + t * v1;
	}

}