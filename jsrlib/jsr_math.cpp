#include "jsr_math.h"
#include <random>
#include <SDL.h>

namespace jsrlib {

	static std::default_random_engine default_generator;
	static std::uniform_real_distribution<float> uniform_distribution(0.0, 1.0);
	
	void random_init(uint64_t seed) {
		default_generator = std::default_random_engine(seed);
	}

	float randomf()
	{
		return uniform_distribution(default_generator);
	}

}
