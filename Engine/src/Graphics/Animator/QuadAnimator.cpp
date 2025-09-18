#include "QuadAnimator.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				QuadAnimator::QuadAnimator(const World::Entity& entity): m_Entity(entity) {
					auto& renderer = m_Entity.GetOrAddComponent<QuadRenderer>();
					renderer.m_AnimatorBound = true;
					m_ActiveSequence = false;

					SetStopCallback([]{});
					SetNextTickCallback([]{});
				}

				QuadAnimator::~QuadAnimator() {
					auto& renderer = m_Entity.GetComponents<QuadRenderer>();
					renderer.m_AnimatorBound = false;
				}

				void QuadAnimator::SetStopCallback(AnimationStopCallbackFn callback) {
					m_StopCallBack = std::move(callback);
				}

				void QuadAnimator::SetNextTickCallback(AnimationStopCallbackFn callback) {
					m_NextTickCallBack = std::move(callback);
				}

				void QuadAnimator::Stop(const bool abruptStop) {
					if (abruptStop) {
						m_ActiveSequence = false;
					}

					m_LoopStartIndex = 0xFFFF;
				}

				void QuadAnimator::OnTickUpdate() {
					if (m_ActiveSequence) {
						if (m_TicksElapsedSinceStart == 1) {
							m_NextTickCallBack();
						}
						++m_TicksElapsedSinceStart;
						auto& [anim, params] = m_AnimationSequence[m_CurrentLoopedSequenceIndex];
						if (m_CurrentFrame == anim.m_Pack->m_Animations[anim.m_AnimationID].m_Frames.size() - 1 && m_CurrentFrameTick >= anim.m_Pack->m_Animations[anim.m_AnimationID].m_Frames[m_CurrentFrame].m_TickDuration) {
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
									m_StopCallBack();
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
							if (m_CurrentFrameTick < anim.m_Pack->m_Animations[anim.m_AnimationID].m_Frames[m_CurrentFrame].m_TickDuration) {
								++m_CurrentFrameTick;
							}
							else {
								m_CurrentFrameTick = 1;
								++m_CurrentFrame;
							}
						}

						auto& renderer = m_Entity.GetComponents<QuadRenderer>();
						// animation with the final state for the rendering
						const auto& [pack, id] = m_AnimationSequence[m_CurrentLoopedSequenceIndex].first;
						const auto& m_UVs= pack->m_Animations[id].m_Frames[m_CurrentFrame].m_UVs;

						const glm::vec2 size = pack->m_FrameSize;
						if (size != glm::vec2{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}) {
							renderer.m_HalfSize = size * m_SizeScale / 2.0f;
						}

						renderer.m_Texture = pack->m_SpriteAtlas->GetTexture();
						renderer.m_UVs = m_UVs;
					}
				}

				void QuadAnimator::SetSizeScale(const float scale) {
					m_SizeScale = scale;
				}

				float QuadAnimator::GetSizeScale() const {
					return m_SizeScale;
				}

				uint64_t QuadAnimator::GetTicksElapsed() const {
					return m_TicksElapsedSinceStart;
				}
			}
		}
	}
}
