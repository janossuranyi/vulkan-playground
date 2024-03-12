#pragma once

namespace jsrlib {

	inline float lerp(float v0, float v1, float t) {
		return (1 - t) * v0 + t * v1;
	}

	void random_init(uint64_t seed);
	float randomf();

}