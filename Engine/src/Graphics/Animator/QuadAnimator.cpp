#include "QuadAnimator.hpp"
#include "WorldSystem/Components.hpp"

namespace Cori {
	namespace World {
		namespace Components {
			namespace Entity {
				QuadAnimator::QuadAnimator() {
					m_ActiveSequence = false;

					SetStopCallback([]{});
					SetEngineStopCallback([]{});
				}

				QuadAnimator::~QuadAnimator() {
					if (m_Entity.HasComponents<QuadRenderer>()) {
						auto& renderer = m_Entity.GetComponents<QuadRenderer>();
						renderer.m_AnimatorBound = false;
					}
				}

				void QuadAnimator::SetStopCallback(StopCallbackFn callback) {
					m_StopCallBack = std::move(callback);
				}

				void QuadAnimator::Stop(const bool abruptStop) {
					if (abruptStop) {
						m_ActiveSequence = false;
						m_CurrentFrame = 0;
						m_CurrentFrameTick = 1;
						m_StopCallBack();
						m_EngineCallBack();
					}

					m_LoopStartIndex = 0xFFFF;
				}

				void QuadAnimator::OnTickUpdate() {
					if (m_ActiveSequence) {
						//++m_TicksElapsedSinceStart;
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
									m_EngineCallBack();
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
						const auto& [pack, id] = m_AnimationSequence[m_CurrentLoopedSequenceIndex].first;
						const auto& m_UVs= pack->m_Animations[id].m_Frames[m_CurrentFrame].m_UVs;

						const glm::vec2 animationFrameSize = pack->m_Animations[id].m_FrameSize;
						if (animationFrameSize != glm::vec2{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}) {
							renderer.m_HalfSize = animationFrameSize * m_SizeScale / 2.0f;
						}

						if (pack->m_Type != Graphics::AnimationPack::CORI_VARYING) {
							renderer.m_Texture = std::get<std::shared_ptr<Graphics::SpriteAtlas>>(pack->m_TextureOrAtlas)->GetTexture();
						} else {
							renderer.m_Texture = std::get<std::shared_ptr<Graphics::Texture2D>>(pack->m_TextureOrAtlas);
						}

						renderer.m_UVs = m_UVs;
					}
				}

				void QuadAnimator::SetSizeScale(const glm::vec2 scale) {
					m_SizeScale = scale;
				}

				glm::vec2 QuadAnimator::GetSizeScale() const {
					return m_SizeScale;
				}

				//uint64_t QuadAnimator::GetTicksElapsed() const {
				//	return m_TicksElapsedSinceStart;
				//}

				void QuadAnimator::SetEngineStopCallback(StopCallbackFn callback) {
					m_EngineCallBack = std::move(callback);
				}
			}
		}
	}
}
