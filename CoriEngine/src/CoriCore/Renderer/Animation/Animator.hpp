#pragma once
#include <nlohmann/json.hpp>
#include "Animation.hpp"
#include "AnimationHandle.hpp"
#include "../../ECS/Entity.hpp"
#include "../../ECS/Components.hpp"
#include "../../AssetManager/AssetManager.hpp"

using json = nlohmann::json;

// move to components, or create a component wrapper
namespace Cori {
	class Animator {
	public:
		Animator(std::filesystem::path jsonPath, Entity entity, const float timeStep, const char* animatorName) : m_Entity(entity) {

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

		template<IsAnimationHandle Handle>
		void Start() {
			m_Animations[Handle::m_Index].m_CurrentFrame = 0;
			Play<Handle>();
		}
		
		template<IsAnimationHandle Handle>
		void Play() {
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

		glm::vec2 m_FrameSize{ 0.0f, 0.0f };

		private:
		int ExtractFrameNumber(const std::string& key_str) {
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

		const char* m_AnimatorName;
		std::shared_ptr<Texture2D> m_Atlas;
		std::vector<Animation> m_Animations;
		Entity m_Entity{};
	};
}