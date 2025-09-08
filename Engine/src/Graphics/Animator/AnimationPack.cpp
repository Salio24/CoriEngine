#include "AnimationPack.hpp"
#include "Core/Application.hpp"
#include "Graphics/Image.hpp"
#include "AssetManager/AssetManager.hpp"
#include "AssetManager/EngineAssets.hpp"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace {
	int32_t ExtractFrameNumber(const std::string& key) {
		const size_t startPos = key.find(' ');
		const size_t endPos = key.find('.');
		if (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos) {
			return std::stoi(key.substr(startPos + 1, endPos - (startPos + 1)));
		}
		return -1;
	}
}

namespace Cori {
	namespace Graphics {
		std::shared_ptr<AnimationPack> AnimationPack::Create(const std::filesystem::path& jsonPath, ConfigType type, const float timeStep, const std::string& name) {
			std::ifstream f(jsonPath);

			if (type == ASEPRITE) {
				try {
					CORI_CORE_INFO_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Loading AnimationPack from: {}", jsonPath.string());
					if (!f.good()) {
						throw Core::CoriError(std::format("Failed to open json file.", jsonPath.string()));
					}

					json data = json::parse(f);

					const json& framesObject = data["frames"];
					const glm::u16vec2 frameResolution = {framesObject.items().begin().value()["frame"]["w"], framesObject.items().begin().value()["frame"]["h"]};

					const std::filesystem::path atlasPath = jsonPath.parent_path() / data["meta"]["image"];
					auto image = Image::Create(atlasPath);
					if (!image->GetSuccessStatus()) {
						throw Core::CoriError(std::format("Failed to load image: {}", atlasPath.string()));
					}

					const glm::uvec2 initialImageResolution = { image->GetWidth(), image->GetHeight() };

					auto atlas = SpriteAtlas::Create(data["meta"]["image"], image, frameResolution);
					if (!atlas->GetSuccessStatus()) {
						throw Core::CoriError(std::format("Failed to load Sprite Atlas from: {}", atlasPath.string()));
					}

					std::vector<std::pair<uint32_t, json>> sortedFrameItems;

					for (const auto& [key, frameData] : framesObject.items()) {
						int32_t frameNum = ExtractFrameNumber(key);
						if (frameNum != -1) {
							sortedFrameItems.emplace_back( frameNum, frameData );
						}
						else {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Could not parse frame number from key: {}", key);
						}
					}

					std::ranges::sort(sortedFrameItems, [](const auto& a, const auto& b) {
						return a.first < b.first;
					});

					std::vector<AnimationFrame> frames;
					std::vector<AnimationData> animations;
					animations.reserve(initialImageResolution.y / frameResolution.y);

					glm::uvec2 oldPos;
					glm::uvec2 pos{ 0.0f, 0.0f };

					for (const auto& [frameIndex, frameData] : sortedFrameItems) {
						glm::u16vec2 currentFrameResolution = { frameData["frame"]["w"], frameData["frame"]["h"] };
						if (frameResolution != currentFrameResolution) {
							throw Core::CoriError(std::format("All animation frames should have the same resolution. Mismatch between frame '0' resolution: ({}, {}), and frame '{}' resolution: ({}, {})", frameResolution.x, frameResolution.y, frameIndex, currentFrameResolution.x, currentFrameResolution.y));
						}

						oldPos = pos;
						pos = { frameData["frame"]["x"], frameData["frame"]["y"] };
						if (pos.y - oldPos.y == frameResolution.y) {
							AnimationData anim(frames);
							animations.push_back(anim);
							frames.clear();
						}

						uint32_t col = pos.x / frameResolution.x;
						uint32_t row = pos.y / frameResolution.y;

						AnimationFrame frame;

						frame.m_UVs = atlas->GetSpriteUVsAtPosition({col, row});
						frame.m_TickDuration = std::round(static_cast<float>(frameData["duration"]) / (timeStep * 1000.0f));

						frames.push_back(frame);

						if (frameIndex + 1 == sortedFrameItems.size()) {
							AnimationData anim(frames);
							animations.push_back(anim);
						}
					}

					CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::AnimationPack }, "Loaded Animation Pack '{}' from ASEPRITE config: {}", name, jsonPath.string());
					return std::shared_ptr<AnimationPack>(new AnimationPack(animations, atlas, name, frameResolution));
				}
				catch (std::exception& e) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::AnimationPack }, "Failed to parse json file: {1}, and create the Animation Pack '{3}'. \n{0}Error: {2}. \n{0}Created a placeholder Animation Pack.", CORI_SECOND_LINE_SPACING, jsonPath.string(), e.what(), name);
					return std::shared_ptr<AnimationPack>(new AnimationPack());
				}
			}
			if (type == CORI) {
				try {
					throw Core::CoriError("Cori animation pack config type is not yet implemented.");
				}
				catch (std::exception& e) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::AnimationPack }, "Failed to parse json file: {1}, and create the Animation Pack '{3}'. \n{0}Error: {2}. \n{0}Created a placeholder Animation Pack.", CORI_SECOND_LINE_SPACING, jsonPath.string(), e.what(), name);
					return std::shared_ptr<AnimationPack>(new AnimationPack());
				}
			}

			return std::shared_ptr<AnimationPack>(new AnimationPack());
		}

		std::shared_ptr<AnimationPack> AnimationPack::Create(const Descriptor& descriptor) {
			return Create(descriptor.m_JsonPath, descriptor.m_ConfigType, Core::Application::GetGameTimer().GetTimestep(), descriptor.m_Name);
		}

		Animation AnimationPack::GetAnimation(const uint32_t index) {
			if (m_Valid) {
				if (CORI_CORE_CHECK(index + 1 <= m_Animations.size(), "Animation Pack '{}' doesn't have an animation at index '{}', returning animation at index '0'. Total animation cound '{}'", m_Name, index, m_Animations.size())) { return Animation(m_Animations[0], m_SpriteAtlas->GetTexture(), m_FrameSize); }
				return Animation(m_Animations[index], m_SpriteAtlas->GetTexture(), m_FrameSize);
			}

			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::AnimationPack }, "Animation Pack '{}', was created as a placeholder. Requested animation at '{}' doesn't exist, returning a placeholder.", m_Name, index);
			std::vector<AnimationFrame> frames;
			constexpr AnimationFrame frame { UVs{}, 2 };
			frames.push_back(frame);
			const AnimationData data(frames);
			//return Animation(data, AssetManager::GetTexture2D(AssetPlaceholders::Texture2D), glm::vec2{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() });
			return Animation(data, AssetManager::Get(Internal::AssetPlaceholders::Texture2D), glm::vec2{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() });
		}

		AnimationPack::AnimationPack(std::vector<AnimationData> animations, const std::shared_ptr<SpriteAtlas>& spriteAtlas, std::string name, const glm::u16vec2 frameResolution) : m_Animations(std::move(animations)), m_SpriteAtlas(spriteAtlas), m_Name(std::move(name)), m_FrameSize(frameResolution) {
			m_Valid = true;
		}

		AnimationPack::AnimationPack() {
			m_Valid = false;
		}
	}
}

