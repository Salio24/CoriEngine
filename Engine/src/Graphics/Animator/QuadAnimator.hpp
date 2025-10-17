#pragma once
#include "AnimationPack.hpp"
#include "WorldSystem/Entity.hpp"

namespace Cori {
	namespace World {
		namespace Systems {
			class Animation;
		}

		namespace Components {
			namespace Entity {

				/**
				 * @brief Responsible for playing animations when attached to an entity.
				 * @details QuadRenderer of the entity will be somewhat locked when QuadAnimator is attached to the entity, the only thing you can do is change tint color.
				 * If an entity doesn't have QuadRenderer upon addition of QuadAnimator, it will be added.
				 */
				class QuadAnimator {
				public:
					using StopCallbackFn = std::function<void()>;

					QuadAnimator();
					~QuadAnimator();

					/**
					 * @brief Sets a callback that will be fired when any animation sequence stops.
					 * @param callback Functor to use as a callback.
					 */
					void SetStopCallback(StopCallbackFn callback);

					/**
					 * @brief Plays a sequence of animations.
					 * @detials Animation sequence can be either looped or no, when you pass AnimationWithParams to it,
					 * it will loop through the whole sequence looking if any AnimationWithParams that has LoopedInSequence set to true, if it finds it that animation becomes a "Looping Point",
					 * all animations before the looping point will be played once, like an intro, then all animations after (and including) the looping point will be looped until you call Stop.
					 * \n It doesn't care if some animation after the looping point has LoopedInSequence set to false, it will be looped regardless, it only cares about the firs occurrence of LoopedInSequence = true.
					 * \n If it doesn't find the looping point, it will just play every animation once and stop.
					 * @param sequence
					 */
					void Play(const Graphics::IsAnimationWithParams auto&... sequence) {
						m_AnimationSequence.clear();
						m_AnimationSequence.reserve(sizeof...(sequence));
						(m_AnimationSequence.emplace_back(sequence), ...);
						m_CurrentLoopedSequenceIndex = 0;
						m_CurrentFrame = 0;
						m_CurrentFrameTick = 0;
						//m_TicksElapsedSinceStart = 0;
						m_ActiveSequence = true;

						bool loopedFlags[] = { sequence.second.LoopedInSequence ... };
						m_LoopStartIndex = 0xFFFF;
						for (uint32_t i = 0; i < sizeof...(sequence); ++i) {
							if (loopedFlags[i]) {
								m_LoopStartIndex = i;
								break;
							}
						}

						OnTickUpdate();
					}

					/**
					 * @brief Stops the current animation.
					 * @param abruptStop If set to true, the currently playing animation sequence will be abruptly stopped, it will not let the currently playing animation to finish,
					 * if set to false it will let the currently playing animation to finish and then stop.
					 */
					void Stop(const bool abruptStop);

					/**
					 * @brief Changes the size scale.
					 * @details Since you cannot control the size of the sprite that will be rendered from the QuadRenderer, you can change the size scale here, it's a modifier.
					 * \n Final half size is calculated as follows: ```renderer.m_HalfSize = animationFrameSize * m_SizeScale / 2.0f;```
					 * @param scale Scale modifier.
					 */
					void SetSizeScale(const glm::vec2 scale);

					/**
					 * @brief Retrieves the current scale modifier.
					 * @return Scale modifier.
					 */
					[[nodiscard]] glm::vec2 GetSizeScale() const;

					//[[nodiscard]] uint64_t GetTicksElapsed() const;

				protected:
					friend Systems::Animation;

					void OnTickUpdate();

					void SetEngineStopCallback(StopCallbackFn callback);

				private:
					StopCallbackFn m_StopCallBack;
					StopCallbackFn m_EngineCallBack;
					bool m_ActiveSequence;
					glm::vec2 m_SizeScale{ 1.0f, 1.0f };
					uint16_t m_LoopStartIndex{ 0xFFFF };
					uint16_t m_CurrentLoopedSequenceIndex{ 0 };

					uint32_t m_CurrentFrame{ 0 };
					uint32_t m_CurrentFrameTick{ 0 };
					//uint64_t m_TicksElapsedSinceStart{ 0 };

					std::vector<Graphics::AnimationWithParams> m_AnimationSequence;
					World::Entity m_Entity{};
				};
			}
		}
	}
}
