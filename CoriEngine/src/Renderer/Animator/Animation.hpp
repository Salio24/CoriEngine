#pragma once
#include "AnimationFrame.hpp"

namespace Cori {
	class Animation {
	public:
		Animation( const std::vector<AnimationFrame>& frames) : m_Frames(std::move(frames)) {}

		std::vector<AnimationFrame> m_Frames;
		uint8_t m_CurrentFrame{ 0 };
		uint8_t m_CurrentFrameTick{ 0 };
	};
}