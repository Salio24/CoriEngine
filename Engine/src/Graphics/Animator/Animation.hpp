#pragma once
//legacy from old 2d renderer, need to rewire
#if 0
namespace Cori {
	namespace Graphics {
		class AnimationPack;

		/**
		 * @brief Owning handle to the animation inside the AnimationPack, when paired with Animation::PlayParams can be passed to QuadAnimator.
		 */
		struct Animation {
			/**
			 * @brief Tells the QuadAnimator how to play the animation. More details in the QuadAnimator docs.
			 * @note Only LoopedInSequence is implemented for now.
			 */
			struct PlayParams {
				// TODO: implement the logic that uses all this parameters, later
				//uint32_t Loops{ 0 };
				//uint32_t MaxFrames{ 0 };
				//uint32_t StartFrame{ 0 };
				//uint32_t MaxTicks{ 0 };
				//uint32_t StartTick{ 0 };
				bool LoopedInSequence{ false };
			};

			[[nodiscard]] glm::u16vec2 GetFrameSize() const;

			std::shared_ptr<AnimationPack> m_Pack;
			uint32_t m_AnimationID{ 0 };
		};

		/**
		 * @brief Pair of Animation and Animation::PlayParams.
		 */
		using AnimationWithParams = std::pair<Animation, Animation::PlayParams>;

		template<typename T>
		concept IsAnimationWithParams = std::is_same_v<T, AnimationWithParams>;
	}
}
#endif