#pragma once

#include <cinttypes>

namespace jsr {


	namespace FrameCounter {
		const int INFLIGHT_FRAMES = 2;

		uint32_t getFrameIndex();
		uint32_t getCurrentFrame();
		uint32_t getCurrentFrameMod2();
		uint32_t getCurrentFrameMod3();
		uint32_t getCurrentFrameMod4();
		uint32_t nextFrame();
	}
}