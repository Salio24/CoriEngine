#pragma once
#include "AnimationPack.hpp"
#include "WorldSystem/Entity.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				using AnimationStopCallbackFn = std::function<void()>;

				using AnimationWithParams = std::pair<Graphics::Animation, Graphics::Animation::PlayParams>;

				template<typename T>
				concept IsAnimationWithParams = std::is_same_v<T, AnimationWithParams>;

				class QuadAnimatorNew {
				public:
					explicit QuadAnimatorNew(const World::Entity& entity);

					~QuadAnimatorNew();

					void SetStopCallback(AnimationStopCallbackFn callback);
					void SetNextTickCallback(AnimationStopCallbackFn callback);


					void Play(const IsAnimationWithParams auto&... sequence) {
						m_AnimationSequence.clear();
						m_AnimationSequence.reserve(sizeof...(sequence));
						(m_AnimationSequence.emplace_back(sequence), ...);
						m_CurrentLoopedSequenceIndex = 0;
						m_CurrentFrame = 0;
						m_CurrentFrameTick = 0;
						m_TicksElapsedSinceStart = 0;
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

					void Stop(const bool abruptStop);

					void OnTickUpdate();

					void SetSizeScale(const float scale);

					[[nodiscard]] float GetSizeScale() const;

					[[nodiscard]] uint64_t GetTicksElapsed() const;

				private:
					AnimationStopCallbackFn m_StopCallBack;
					AnimationStopCallbackFn m_NextTickCallBack;
					bool m_ActiveSequence;
					float m_SizeScale{ 1.0f };
					uint16_t m_LoopStartIndex{ 0xFFFF };
					uint16_t m_CurrentLoopedSequenceIndex{ 0 };

					uint32_t m_CurrentFrame{ 0 };
					uint32_t m_CurrentFrameTick{ 0 };
					uint64_t m_TicksElapsedSinceStart{ 0 };

					std::vector<AnimationWithParams> m_AnimationSequence;
					World::Entity m_Entity{};
				};
			}
		}
	}
}
