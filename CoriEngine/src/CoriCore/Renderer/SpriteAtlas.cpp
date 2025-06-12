#include "SpriteAtlas.hpp"
#include "../AssetManager/AssetManager.hpp"

namespace Cori {
	bool SpriteAtlas::PreCreateHook(const SpriteAtlasDescriptor& descriptor) {

		return true;
	}

	SpriteAtlas::SpriteAtlas(const SpriteAtlasDescriptor& descriptor) {
		m_Texture = AssetManager::GetTexture2D(descriptor.m_TextureDescriptor);

		CORI_CORE_ASSERT_ERROR((!(m_Texture->GetHeight() % descriptor.m_SpriteResolution.y) && !(m_Texture->GetWidth() % descriptor.m_SpriteResolution.x)), "Invalid sprite resolution, sprite resolution on x and y should be divisible by texture resolution without remainder. Atlas: {0}", descriptor.GetDebugName());

		m_DebugName = descriptor.GetDebugName();

		m_SpriteAtlasDimensions.x = m_Texture->GetWidth() / descriptor.m_SpriteResolution.x;
		m_SpriteAtlasDimensions.y = m_Texture->GetHeight() / descriptor.m_SpriteResolution.y;

		m_SpriteAtlasSize = m_SpriteAtlasDimensions.x * m_SpriteAtlasDimensions.y;

		m_SpriteUVs.reserve(m_SpriteAtlasSize);

		glm::vec2 fullSpriteTextureSize = { (float)descriptor.m_SpriteResolution.x / m_Texture->GetWidth(), (float)descriptor.m_SpriteResolution.y / m_Texture->GetHeight() };

		glm::vec2 scaledSpriteTextureSize = { fullSpriteTextureSize.x * descriptor.m_SpriteScale, fullSpriteTextureSize.y * descriptor.m_SpriteScale };
		for (int row = 0; row < m_SpriteAtlasDimensions.y; row++) {
			for (int col = 0; col < m_SpriteAtlasDimensions.x; col++) {
				glm::vec2 texturePos = { fullSpriteTextureSize.x * col, 1.0f - fullSpriteTextureSize.y * (row + 1) };

				m_SpriteUVs.emplace_back(UVs{ texturePos, texturePos + scaledSpriteTextureSize });
			}
		}
	}

	const UVs& SpriteAtlas::GetSpriteUVsAtIndex(uint32_t index) const {
		if (CORI_CORE_ASSERT_ERROR(index + 1 <= m_SpriteAtlasSize, "Requested a sprite UVs from ({0}) at invalid index: {1} : returned data from index: 0", m_DebugName, index)) { return m_SpriteUVs[0]; }
		return m_SpriteUVs[index];
	}

	const UVs& SpriteAtlas::GetSpriteUVsAtPosition(glm::ivec2 pos) const  {
		if (CORI_CORE_ASSERT_ERROR(pos.x * pos.y <= m_SpriteAtlasSize, "Requested a sprite UVs from ({0}) at invalid position: ({1}, {2}) : returned data from position: (0, 0)", m_DebugName, pos.x, pos.y)) { return m_SpriteUVs[0]; }
		return m_SpriteUVs[pos.x * pos.y - 1];
	}

	const std::shared_ptr<Texture2D> SpriteAtlas::GetTexture() const {
		return m_Texture;
	}

}