#pragma once
#include "../AssetManager/SpriteAtlasDescriptor.hpp"
#include "Sprite.hpp"
#include "Texture.hpp"
#include "../Profiling/Trackable.hpp"
#include "../SelfFactory.hpp"

namespace Cori {

	class SpriteAtlas : public Profiling::Trackable<SpriteAtlas>, public SharedSeflFactory<SpriteAtlas> {
	public:

		static bool PreCreateHook(const SpriteAtlasDescriptor& descriptor);

		const std::shared_ptr<Texture2D> GetTexture() const;

		const UVs& GetSpriteUVsAtIndex(uint32_t index) const;

		const UVs& GetSpriteUVsAtPosition(glm::ivec2 pos) const;

	protected: 
		SpriteAtlas(const SpriteAtlasDescriptor& descriptor); 
	private:

		std::string_view m_DebugName;

		// both referring to grind dimension and grid size, not to pixel ones
		glm::ivec2 m_SpriteAtlasDimensions;
		uint32_t m_SpriteAtlasSize;

		// replaced with std::shared

		std::shared_ptr<Texture2D> m_Texture;

		std::vector<UVs> m_SpriteUVs;
	};
}