#pragma once
#include "AnimationFrame.hpp"

namespace Cori {
	namespace Graphics {
		class AnimationData {
		public:
			explicit AnimationData(const glm::u16vec2 frameSize, std::vector<AnimationFrame> frames) : m_FrameSize(frameSize) ,m_Frames(std::move(frames)) {}

			glm::u16vec2 m_FrameSize;
			std::vector<AnimationFrame> m_Frames;
		};

		class AnimationPack;

		struct Animation {
			struct PlayParams {
				// TODO: implement the logic that uses all this parameters
				uint32_t Loops{ 0 };
				uint32_t MaxFrames{ 0 };
				uint32_t StartFrame{ 0 };
				uint32_t MaxTicks{ 0 };
				uint32_t StartTick{ 0 };
				bool LoopedInSequence{ false };
			};

			[[nodiscard]] glm::u16vec2 GetFrameSize() const;

			std::shared_ptr<AnimationPack> m_Pack;
			uint32_t m_AnimationID{ 0 };
		};

		using AnimationWithParams = std::pair<Animation, Animation::PlayParams>;

		template<typename T>
		concept IsAnimationWithParams = std::is_same_v<T, AnimationWithParams>;
	}
}