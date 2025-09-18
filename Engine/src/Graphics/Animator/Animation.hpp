#pragma once
#include "AnimationFrame.hpp"

namespace Cori {
	namespace Graphics {
		class AnimationData {
		public:
			explicit AnimationData(std::vector<AnimationFrame> frames) : m_Frames(std::move(frames)) {}

			std::vector<AnimationFrame> m_Frames;
		};
	}
}