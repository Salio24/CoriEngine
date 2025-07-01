#include "Animator.hpp"

namespace Cori {
	namespace Components {
		namespace Entity {
			Animator::Animator(std::filesystem::path jsonPath, Cori::Entity entity, const float timeStep, const char* animatorName) : m_Entity(entity) {
				std::ifstream f(jsonPath);
				m_AnimatorName = animatorName;

				CORI_CORE_ASSERT_FATAL(f.is_open(), "Failed to open JSON file '{0}'", jsonPath.string());

				json data;
				try {
					data = json::parse(f);
				}
				catch (json::parse_error& e) {
					CORI_CORE_ERROR("SpriteSheet: Failed to parse JSON file '{0}'. Error: {1}", jsonPath.string(), e.what());
					return;
				}
				f.close();

				if (!data.contains("frames") || !data["frames"].is_object()) {
					CORI_CORE_ERROR("SpriteSheet: JSON missing 'frames' object or it's not an object in '{0}'", jsonPath.string());
					return;
				}

				if (!data.contains("meta")) {
					CORI_CORE_ERROR("SpriteSheet: JSON missing 'meta' object.", jsonPath.string());
					return;
				}

				const Cori::Texture2DDescriptor texture{
					std::string(animatorName) + std::string(" Atlas "),
					(jsonPath.parent_path() / std::string(data["meta"]["image"])).string()
				};

				m_Atlas = Cori::AssetManager::GetTexture2DOwning(texture);

				glm::vec2 atlasSize;
				glm::vec2 frameUVSize{ 0.0f, 0.0f };

				atlasSize.x = data["meta"]["size"]["w"];
				atlasSize.y = data["meta"]["size"]["h"];

				const json& framesObject = data["frames"];

				std::vector<json> frameValuesArray;
				for (const auto& [key, frameData] : framesObject.items()) {
					frameValuesArray.push_back(frameData);
				}

				for (size_t i = 0; i < frameValuesArray.size(); ++i) {
					const json& frameData = frameValuesArray[i];
				}

				std::vector<std::pair<int, json>> sortedFrameItems;

				for (const auto& [keyStr, frameData] : framesObject.items()) {
					int frameNum = ExtractFrameNumber(keyStr);
					if (frameNum != -1) {
						sortedFrameItems.push_back({ frameNum, frameData });
					}
					else {
						CORI_CORE_WARN("Warning: Could not parse frame number from key: {}", keyStr);
					}
				}

				std::sort(sortedFrameItems.begin(), sortedFrameItems.end(),
					[](const auto& a, const auto& b) {
						return a.first < b.first;
					});

				std::vector<AnimationFrame> frames;

				glm::vec2 oldPos{ 0.0f, 0.0f };
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
						frame.m_UVs.UVmin = { (pos.x / m_FrameSize.x) * frameUVSize.x, 1.0f - ((pos.y / m_FrameSize.y) * frameUVSize.y + frameUVSize.y) };
						frame.m_UVs.UVmax = { (pos.x / m_FrameSize.x) * frameUVSize.x + frameUVSize.x, 1.0f - ((pos.y / m_FrameSize.y) * frameUVSize.y) };
						frame.m_TickDuration = std::round(float(frameData["duration"]) / (timeStep * 1000.0f));


						frames.push_back(frame);

						if (frameNum + 1 == sortedFrameItems.size()) {
							Animation anim(frames);
							m_Animations.push_back(anim);
						}
					}
				}

				CORI_CORE_DEBUG("AAA");
			}

			void Animator::UpdateSequence() {
				if (m_AnimationQueue.empty() || m_CurrentAnimationIndex >= m_AnimationQueue.size()) {
					return;
				}
				m_AnimationQueue[m_CurrentAnimationIndex].Player();
			}

			int Animator::ExtractFrameNumber(const std::string& key_str) {
				size_t start_pos = key_str.find(' ');
				size_t end_pos = key_str.find('.');
				if (start_pos != std::string::npos && end_pos != std::string::npos && end_pos > start_pos) {
					try {
						return std::stoi(key_str.substr(start_pos + 1, end_pos - (start_pos + 1)));
					}
					catch (const std::exception& e) {
					}
				}
				return -1;
			}
		}
	}


}