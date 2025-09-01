#pragma once
#include "Profiling/Trackable.hpp"
#include "Graphics/SpriteAtlas.hpp"
#include "Animation.hpp"

namespace Cori {
	namespace Graphics {
		struct Animation {
		struct PlayParams {
			// TODO: implement the logic that uses all this parameters
			uint32_t Loops{ 0 };
			uint32_t MaxFrames{ 0 };
			uint32_t StartFrame{ 0 };
			uint32_t MaxTicks{ 0 };
			uint32_t StartTick{ 0 };
			bool LoopedInSequence{ false };
		};

			AnimationData m_Data;
			std::shared_ptr<Texture2D> m_Texture;
			glm::vec2 m_Size;
		};


		class AnimationPack : public Profiling::Trackable<AnimationPack> {
		public:
			enum ConfigType : uint8_t {
				ASEPRITE,
				CORI
			};

			static std::shared_ptr<AnimationPack> Create(const std::filesystem::path& jsonPath, ConfigType type, const float timeStep, const std::string& name);
			~AnimationPack();

			[[nodiscard]] Animation GetAnimation(const uint32_t index);


		private:
			explicit AnimationPack(std::vector<AnimationData> animations, const std::shared_ptr<SpriteAtlas>& spriteAtlas, std::string name, const glm::u16vec2 frameResolution);
			explicit AnimationPack();

			std::vector<AnimationData> m_Animations;
			std::shared_ptr<SpriteAtlas> m_SpriteAtlas;
			std::string m_Name;
			glm::u16vec2 m_FrameSize{ 0, 0 };
			bool m_Valid = false;


		};
	}
}
