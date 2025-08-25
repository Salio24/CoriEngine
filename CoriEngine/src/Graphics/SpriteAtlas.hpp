#pragma once
#include "Texture.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"

namespace Cori {

	class SpriteAtlas : public Profiling::Trackable<SpriteAtlas>, public SharedSelfFactory<SpriteAtlas, CoriError<>> {
	public:

		static std::expected<void, CoriError<>> PreCreateHook(std::string name, const std::shared_ptr<Texture2D>& texture, const glm::u16vec2 spriteResolution);

		[[nodiscard]] const UVs& GetSpriteUVsAtIndex(uint32_t index) const;

		[[nodiscard]] const UVs& GetSpriteUVsAtPosition(glm::uvec2 pos) const;

		[[nodiscard]] std::shared_ptr<Texture2D> GetTexture() const;

	protected:
		explicit SpriteAtlas(std::string name, const std::shared_ptr<Texture2D>& texture, const glm::u16vec2 spriteResolution);
	private:

		std::string m_Name;

		uint32_t m_SpriteCount;
		std::shared_ptr<Texture2D> m_Texture;
		glm::u16vec2 m_GridDimensions;

		std::vector<UVs> m_SpriteUVs;
	};
}