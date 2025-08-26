#include "QuadAnimator.hpp"
#include "AssetManager/AssetManager.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			QuadAnimator::QuadAnimator(std::filesystem::path jsonPath, const Cori::Entity& entity, const float timeStep, const char* animatorName) : m_Entity(entity) {
				std::ifstream f(jsonPath);
				m_AnimatorName = animatorName;

				if (!m_Entity.HasComponents<QuadRenderer>()) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Adding a QuadAnimator to an Entity '{}', that doesn't have QuadRenderer, adding it now.", m_Entity.GetDebugData());
					m_Entity.AddComponent<QuadRenderer>();
				}

				CORI_CORE_ASSERT(f.is_open(), "Failed to open JSON file '{}'", jsonPath.string());

				json data;
				try {
					data = json::parse(f);
				}
				catch (json::parse_error& e) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Failed to parse JSON file '{}', this will lead to Quad Animator not working at all. Error: {}", jsonPath.string(), e.what());
					return;
				}
				f.close();

				if (!data.contains("frames") || !data["frames"].is_object()) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "JSON missing 'frames' object or it's not an object in '{}', this will lead to Quad Animator not working at all.", jsonPath.string());
					return;
				}

				if (!data.contains("meta")) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "JSON missing 'meta' object. This will lead to Quad Animator not working at all.", jsonPath.string());
					return;
				}

				std::filesystem::path atlasPath = (jsonPath.parent_path() / std::string(data["meta"]["image"])).string();

				m_Atlas = Texture2D::Create(atlasPath);

				glm::vec2 atlasSize;
				glm::vec2 frameUVSize{ 0.0f, 0.0f };

				atlasSize.x = data["meta"]["size"]["w"];
				atlasSize.y = data["meta"]["size"]["h"];

				const json& framesObject = data["frames"];

				std::vector<json> frameValuesArray;
				for (const auto& [key, frameData] : framesObject.items()) {
					frameValuesArray.push_back(frameData);
				}

				std::vector<std::pair<uint32_t, json>> sortedFrameItems;

				for (const auto& [keyStr, frameData] : framesObject.items()) {
					int32_t frameNum = ExtractFrameNumber(keyStr);
					if (frameNum != -1) {
						sortedFrameItems.push_back({ frameNum, frameData });
					}
					else {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Could not parse frame number from key: {}", keyStr);
					}
				}

				std::ranges::sort(sortedFrameItems, [](const auto& a, const auto& b) {
					return a.first < b.first;
				});

				std::vector<AnimationFrame> frames;

				glm::vec2 oldPos;
				glm::vec2 pos{ 0.0f, 0.0f };

				for (const auto& [frameNum, frameData] : sortedFrameItems) {
					if (frameData.contains("frame") && frameData["frame"].is_object()) {
						if (m_FrameSize.x == 0.0f && m_FrameSize.y == 0.0f) {
							m_FrameSize.x = frameData["frame"]["w"];
							m_FrameSize.y = frameData["frame"]["h"];
							frameUVSize.x = m_FrameSize.x / atlasSize.x;
							frameUVSize.y = m_FrameSize.y / atlasSize.y;
							m_Animations.reserve(atlasSize.y / m_FrameSize.y);
						}

						oldPos = pos;
						pos = { frameData["frame"]["x"], frameData["frame"]["y"] };
						if (pos.y - oldPos.y == m_FrameSize.y) {
							Animation anim(frames);
							m_Animations.push_back(anim);
							frames.clear();
						}

						AnimationFrame frame;
						frame.m_UVs.UVmin = { pos.x / m_FrameSize.x * frameUVSize.x, 1.0f - (pos.y / m_FrameSize.y * frameUVSize.y + frameUVSize.y) };
						frame.m_UVs.UVmax = { pos.x / m_FrameSize.x * frameUVSize.x + frameUVSize.x, 1.0f - pos.y / m_FrameSize.y * frameUVSize.y };
						frame.m_TickDuration = std::round(static_cast<float>(frameData["duration"]) / (timeStep * 1000.0f));


						frames.push_back(frame);

						if (frameNum + 1 == sortedFrameItems.size()) {
							Animation anim(frames);
							m_Animations.push_back(anim);
						}
					}
				}
				auto& renderer = m_Entity.GetComponents<QuadRenderer>();
				renderer.m_HalfSize = m_FrameSize / 2.0f;
				static_cast<void>(renderer.SetTexture(m_Atlas));
				renderer.m_AnimatorBound = true;
				m_Valid = true;
			}

			QuadAnimator::~QuadAnimator() {
				auto& renderer = m_Entity.GetComponents<QuadRenderer>();
				renderer.m_AnimatorBound = false;
			}

			void QuadAnimator::StartSingle(const AnimationDescriptor& descriptor) {
				if (!m_Valid) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Trying to use an invalid Quad Animator, name: {}", m_AnimatorName);
					return;
				}

				if (!CheckDescriptorValidity(descriptor)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "StartSingle failed, Quad Animator for Entity: '{}'", m_Entity.GetDebugData());
					return;
				}

				m_Animations[descriptor.m_Index].m_CurrentFrame = 0;
				UpdateSingle(descriptor);
			}

			bool QuadAnimator::UpdateSingle(const AnimationDescriptor& descriptor) {
				if (!m_Valid) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Trying to use an invalid Quad Animator, name: {}", m_AnimatorName);
					return false;
				}

				if (!CheckDescriptorValidity(descriptor)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "UpdateSingle failed, Quad Animator for Entity: '{}'", m_Entity.GetDebugData());
					return false;
				}

				Animation& anim = m_Animations[descriptor.m_Index];
				auto& quad = m_Entity.GetComponents<QuadRenderer>();
				quad.m_Texture = m_Atlas;
				quad.m_UVs = anim.m_Frames[anim.m_CurrentFrame].m_UVs;
				if (anim.m_CurrentFrame == anim.m_Frames.size() - 1 && anim.m_CurrentFrameTick == anim.m_Frames[anim.m_CurrentFrame].m_TickDuration) {
					if (descriptor.m_Looped) {
						anim.m_CurrentFrame = 0;
						anim.m_CurrentFrameTick = 1;
						return true;
					}
				}
				else {
					if (anim.m_CurrentFrameTick == anim.m_Frames[anim.m_CurrentFrame].m_TickDuration) {
						anim.m_CurrentFrameTick = 1;
						++anim.m_CurrentFrame;
						return true;
					}
					if (anim.m_CurrentFrameTick < anim.m_Frames[anim.m_CurrentFrame].m_TickDuration) {
						++anim.m_CurrentFrameTick;
						return true;
					}
				}

				return false;
			}

			void QuadAnimator::UpdateSequence() {
				if (!m_Valid) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Trying to use an invalid Quad Animator, name: {}", m_AnimatorName);
					return;
				}

				if (m_AnimationQueue.empty() || m_CurrentAnimationQueueIndex >= m_AnimationQueue.size()) {
					return;
				}

				//m_AnimationQueue[m_CurrentAnimationQueueIndex].Player();
				GetAnimationPlayer()();
			}

			// dafuq is that?
			std::function<void()> QuadAnimator::GetAnimationPlayer() {
				return [this] {
					const uint16_t index = m_AnimationQueue[m_CurrentAnimationQueueIndex].Index;
					Animation& anim = m_Animations[index];

					auto& quad = m_Entity.GetComponents<QuadRenderer>();
					quad.m_Texture = m_Atlas;
					quad.m_UVs = anim.m_Frames[anim.m_CurrentFrame].m_UVs;

					const bool isFinished = anim.m_CurrentFrame == anim.m_Frames.size() - 1 && anim.m_CurrentFrameTick >= anim.m_Frames[anim.m_CurrentFrame].m_TickDuration;

					if (isFinished) {
						const uint16_t queueIndex = m_CurrentAnimationQueueIndex + 1;

						if (queueIndex < m_AnimationQueue.size()) {
							m_CurrentAnimationQueueIndex = queueIndex;
						}
						else if (m_LoopStartIndex != 0xFFFF) {
							m_CurrentAnimationQueueIndex = m_LoopStartIndex;
						}
						else {
							return;
						}

						const uint16_t nextIndex = m_AnimationQueue[m_CurrentAnimationQueueIndex].Index;
						Animation& nextAnim = m_Animations[nextIndex];
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

			void QuadAnimator::ResetAnimationState(const AnimationDescriptor& descriptor) {
				Animation& anim = m_Animations[descriptor.m_Index];
				anim.m_CurrentFrame = 0;
				anim.m_CurrentFrameTick = 1;
			}

			bool QuadAnimator::CheckDescriptorValidity(const AnimationDescriptor& descriptor) const {
				if (std::strcmp(descriptor.m_AnimatorName, m_AnimatorName)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Missmatch between Quad Animator name '{}' specified in Animation Descriptor and the actual Quad Animator name '{}' it is passed to.", descriptor.m_AnimatorName, m_AnimatorName);
					return false;
				}

				if (descriptor.m_Index >= m_Animations.size()) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Index specified in Animation Descriptor '{}' is more or equal to the animation count '{}' inside Quad Animator.", descriptor.m_Index, m_Animations.size());
					return false;
				}

				return true;
			}

			int32_t QuadAnimator::ExtractFrameNumber(const std::string& keyStr) {
				const size_t start_pos = keyStr.find(' ');
				const size_t end_pos = keyStr.find('.');
				if (start_pos != std::string::npos && end_pos != std::string::npos && end_pos > start_pos) {
					return std::stoi(keyStr.substr(start_pos + 1, end_pos - (start_pos + 1)));
				}
				return -1;
			}
		}
	}


}
