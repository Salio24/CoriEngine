#pragma once
#include <nlohmann/json.hpp>
#include "Animation.hpp"
#include "AnimationHandle.hpp"
#include "SceneSystem/Entity.hpp"
#include "SceneSystem/Components.hpp"
#include "AssetManager/AssetManager.hpp"

using json = nlohmann::json;

namespace Cori {
	namespace Components {
		namespace Entity {

			class Animator {
			public:
				Animator(std::filesystem::path jsonPath, Cori::Entity entity, const float timeStep, const char* animatorName);
				void UpdateSequence();

				template<IsAnimationHandle Handle>
				void StartSingle() {
					m_Animations[Handle::m_Index].m_CurrentFrame = 0;
					UpdateSingle<Handle>();
				}

				template<IsAnimationHandle Handle>
				void UpdateSingle() {
					Animation& anim = m_Animations[Handle::m_Index];
					auto& sprite = m_Entity.GetComponents<Components::Entity::Sprite>();
					sprite.m_Texture = m_Atlas;
					sprite.m_UVs = anim.m_Frames[anim.m_CurrentFrame].m_UVs;
					if ((anim.m_CurrentFrame == anim.m_Frames.size() - 1) && anim.m_CurrentFrameTick == anim.m_Frames[anim.m_CurrentFrame].m_TickDuration) {
						if (Handle::m_Looped) {
							anim.m_CurrentFrame = 0;
							anim.m_CurrentFrameTick = 1;
						}
					}
					else {
						if (anim.m_CurrentFrameTick == anim.m_Frames[anim.m_CurrentFrame].m_TickDuration) {
							anim.m_CurrentFrameTick = 1;
							anim.m_CurrentFrame++;
						}
						else {
							anim.m_CurrentFrameTick++;
						}
					}
				}

				template<IsAnimationHandle... Handles>
				void StartSequence() {
					m_AnimationQueue.clear();
					m_CurrentAnimationIndex = 0;

					(ResetAnimationState<Handles>(), ...);

					constexpr bool loopedFlags[] = { Handles::m_Looped... };
					m_LoopStartIndex = -1;
					for (size_t i = 0; i < sizeof...(Handles); ++i) {
						if (loopedFlags[i]) {
							m_LoopStartIndex = i;
							break;
						}
					}

					(m_AnimationQueue.push_back({ GetAnimationPlayer<Handles>(), Handles::m_Index }), ...);
				}

				template<IsAnimationHandle Handle>
				std::function<void()> GetAnimationPlayer() {
					return [this]() {
						uint16_t handleIndex = m_AnimationQueue[m_CurrentAnimationIndex].HandleIndex;
						Animation& anim = m_Animations[handleIndex];

						auto& sprite = m_Entity.GetComponents<Components::Entity::Sprite>();
						sprite.m_Texture = m_Atlas;
						sprite.m_UVs = anim.m_Frames[anim.m_CurrentFrame].m_UVs;

						bool isFinished = (anim.m_CurrentFrame == anim.m_Frames.size() - 1) &&
							(anim.m_CurrentFrameTick >= anim.m_Frames[anim.m_CurrentFrame].m_TickDuration);

						if (isFinished) {
							size_t nextIndex = m_CurrentAnimationIndex + 1;
							bool willLoop = false;

							if (nextIndex < m_AnimationQueue.size()) {
								m_CurrentAnimationIndex = nextIndex;
							}
							else if (m_LoopStartIndex != -1) {
								m_CurrentAnimationIndex = m_LoopStartIndex;
								willLoop = true;
							}
							else {
								return;
							}

							uint16_t nextHandleIndex = m_AnimationQueue[m_CurrentAnimationIndex].HandleIndex;
							Animation& nextAnim = m_Animations[nextHandleIndex];
							nextAnim.m_CurrentFrame = 0;
							nextAnim.m_CurrentFrameTick = 1;
						}
						else {
							if (anim.m_CurrentFrameTick >= anim.m_Frames[anim.m_CurrentFrame].m_TickDuration) {
								anim.m_CurrentFrameTick = 1;
								anim.m_CurrentFrame++;
							}
							else {
								anim.m_CurrentFrameTick++;
							}
						}
						};
				}

				size_t m_CurrentAnimationIndex = 0;
				size_t m_LoopStartIndex = -1;
				glm::vec2 m_FrameSize{ 0.0f, 0.0f };

			private:
				template<IsAnimationHandle Handle>
				void ResetAnimationState() {
					Animation& anim = m_Animations[Handle::m_Index];
					anim.m_CurrentFrame = 0;
					anim.m_CurrentFrameTick = 1;
				}

				int ExtractFrameNumber(const std::string& key_str);

				struct QueuedAnimation {
					std::function<void()> Player;
					uint16_t HandleIndex;
				};
				std::vector<QueuedAnimation> m_AnimationQueue;

				const char* m_AnimatorName;
				std::shared_ptr<Texture2D> m_Atlas;
				std::vector<Animation> m_Animations;
				Cori::Entity m_Entity{};
			};
		}
	}

}