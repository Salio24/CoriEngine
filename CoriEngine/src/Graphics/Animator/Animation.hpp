#pragma once
#include "AnimationFrame.hpp"

namespace Cori {
	class AnimationData {
	public:
		explicit AnimationData(std::vector<AnimationFrame> frames) : m_Frames(std::move(frames)) {}

		std::vector<AnimationFrame> m_Frames;
		uint32_t m_CurrentFrame{ 0 };
		uint32_t m_CurrentFrameTick{ 0 };
	};
}