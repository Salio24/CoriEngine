#pragma once
#include "AnimationPack.hpp"
#include "WorldSystem/Entity.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			using AnimationStopCallbackFn = std::function<void()>;

			using AnimationWithParams = std::pair<Graphics::Animation, Graphics::Animation::PlayParams>;

			template<typename T>
			concept IsAnimationWithParams = std::is_same_v<T, AnimationWithParams>;

			class QuadAnimatorNew {
			public:
				explicit QuadAnimatorNew(const Cori::Entity& entity) : m_Entity(entity) {
					auto& renderer = m_Entity.GetOrAddComponent<QuadRenderer>();
					renderer.m_AnimatorBound = true;

					SetStopCallback([]{});
				}

				~QuadAnimatorNew() {
					auto& renderer = m_Entity.GetComponents<QuadRenderer>();
					renderer.m_AnimatorBound = false;
				}

				void SetStopCallback(AnimationStopCallbackFn callback) {
					m_ClientCallBack = std::move(callback);
				}

				void Play(const IsAnimationWithParams auto&... sequence) {
					m_AnimationSequence.clear();
					m_AnimationSequence.reserve(sizeof...(sequence));
					(m_AnimationSequence.emplace_back(sequence), ...);
					m_CurrentLoopedSequenceIndex = 0;
					m_CurrentFrame = 0;
					m_CurrentFrameTick = 0;
					m_ActiveSequence = true;

					bool loopedFlags[] = { sequence.second.LoopedInSequence ... };
					m_LoopStartIndex = 0xFFFF;
					for (uint16_t i = 0; i < sizeof...(sequence); ++i) {
						if (loopedFlags[i]) {
							m_LoopStartIndex = i;
							break;
						}
					}

					OnTickUpdate();
				}

				void Stop(const bool abruptStop) {
					if (abruptStop) {
						m_ActiveSequence = false;
					}

					m_LoopStartIndex = 0xFFFF;
				}

				void OnTickUpdate() {
					if (m_ActiveSequence) {

						auto& [anim, params] = m_AnimationSequence[m_CurrentLoopedSequenceIndex];
						if (m_CurrentFrame == anim.m_Data.m_Frames.size() - 1 && m_CurrentFrameTick >= anim.m_Data.m_Frames[m_CurrentFrame].m_TickDuration) {
							if (m_CurrentLoopedSequenceIndex == m_AnimationSequence.size() - 1) {
								if (m_LoopStartIndex != 0xFFFF) {
									m_CurrentLoopedSequenceIndex = m_LoopStartIndex;
									m_CurrentFrame = 0;
									m_CurrentFrameTick = 1;
								}
								else {
									m_CurrentFrame = 0;
									m_CurrentFrameTick = 1;
									Stop(true);
									m_ClientCallBack();
									return;
								}
							}
							else {
								++m_CurrentLoopedSequenceIndex;
								m_CurrentFrame = 0;
								m_CurrentFrameTick = 1;
							}
						}
						else {
							if (m_CurrentFrameTick < anim.m_Data.m_Frames[m_CurrentFrame].m_TickDuration) {
								++m_CurrentFrameTick;
							}
							else {
								m_CurrentFrameTick = 1;
								++m_CurrentFrame;
							}
						}

						auto& renderer = m_Entity.GetComponents<QuadRenderer>();
						// animation with the final state for the rendering
						const auto& [data, texture, size] = m_AnimationSequence[m_CurrentLoopedSequenceIndex].first;
						const auto& m_UVs= data.m_Frames[m_CurrentFrame].m_UVs;

						if (size != glm::vec2{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}) {
							renderer.m_HalfSize = size * m_SizeScale / 2.0f;
						}

						renderer.m_Texture = texture;
						renderer.m_UVs = m_UVs;
					}
				}

				void SetSizeScale(const float scale) {
					m_SizeScale = scale;
				}

				float GetSizeScale() const {
					return m_SizeScale;
				}


			private:
				AnimationStopCallbackFn m_ClientCallBack;
				bool m_ActiveSequence{ false };
				float m_SizeScale{ 1.0f };
				uint16_t m_LoopStartIndex{ 0xFFFF };
				uint16_t m_CurrentLoopedSequenceIndex{ 0 };

				uint32_t m_CurrentFrame{ 0 };
				uint32_t m_CurrentFrameTick{ 0 };

				std::vector<AnimationWithParams> m_AnimationSequence;
				Cori::Entity m_Entity{};
			};
		}
	}
}
