#include "FrameCounter.h"

namespace jsr {
	namespace FrameCounter {

		static uint32_t current_frame = 0;
		static uint32_t current_frame_mod2 = 0;
		static uint32_t current_frame_mod3 = 0;
		static uint32_t current_frame_mod4 = 0;
		static uint32_t frame_index = 0;

		uint32_t getFrameIndex()
		{
			return frame_index;
		}

		uint32_t getCurrentFrame()
		{
			return current_frame;
		}
		uint32_t getCurrentFrameMod2()
		{
			return current_frame_mod2;
		}
		uint32_t getCurrentFrameMod3()
		{
			return current_frame_mod3;
		}
		uint32_t getCurrentFrameMod4()
		{
			return current_frame_mod4;
		}
		uint32_t nextFrame()
		{
			++current_frame;
			frame_index = current_frame % INFLIGHT_FRAMES;
			current_frame_mod2 = current_frame % 2;
			current_frame_mod3 = current_frame % 3;
			current_frame_mod4 = current_frame % 4;

			return current_frame;
		}
	}
}