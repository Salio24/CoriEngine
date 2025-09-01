#include "SpriteAtlas.hpp"
#include "AssetManager/AssetManager.hpp"

namespace Cori {
	std::expected<void, CoriError<>> SpriteAtlas::PreCreateHook([[maybe_unused]] std::string name, [[maybe_unused]] const std::shared_ptr<Image>& image, [[maybe_unused]] const glm::u16vec2 spriteResolution) {
		if (!(!(image->GetHeight() % spriteResolution.y) && !(image->GetWidth() % spriteResolution.x))) {
			return std::unexpected(CoriError(std::format("Can't create SpriteAtlas '{}'. Invalid sprite resolution, sprite resolution on x and y should be divisible by texture resolution without remainder. Texture resolution: ({}, {}). Sprite resolution: ({}, {}).", std::move(name), image->GetWidth(), image->GetHeight(), spriteResolution.x, spriteResolution.y)));
		}

		return {};
	}

	SpriteAtlas::SpriteAtlas(std::string name, const std::shared_ptr<Image>& image, const glm::u16vec2 spriteResolution) : m_Name(std::move(name)) {
		int32_t padding = 0;
		const auto success = image->AddPadding(spriteResolution);
		if (success) {
			padding = 1;
		}
		else {
			CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::SpriteAtlas }, "Failed to add padding to a sprite atlas, Texture bleeding might occur. Error: {}", success.error().what());
		}

		m_Texture = Texture2D::Create(image);
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::SpriteAtlas }, "Creating SpriteAtlas: '{}', texture size: '{}', sprite size: '{}'", m_Name, glm::to_string(glm::uvec2(m_Texture->GetWidth(), m_Texture->GetHeight())), glm::to_string(spriteResolution));

		const glm::uvec2 cellSize = { spriteResolution.x + padding * 2,	spriteResolution.y + padding * 2 };

		m_GridDimensions.x = m_Texture->GetWidth() / cellSize.x;
		m_GridDimensions.y = m_Texture->GetHeight() / cellSize.y;

		m_SpriteCount = m_GridDimensions.x * m_GridDimensions.y;
		m_SpriteUVs.reserve(m_SpriteCount);

		const glm::vec2 normalizedSpriteSize = { static_cast<float>(spriteResolution.x) / static_cast<float>(m_Texture->GetWidth()), static_cast<float>(spriteResolution.y) / static_cast<float>(m_Texture->GetHeight()) };
		const glm::vec2 normalizedCellSize = { static_cast<float>(cellSize.x) / static_cast<float>(m_Texture->GetWidth()), static_cast<float>(cellSize.y) / static_cast<float>(m_Texture->GetHeight()) };
		const glm::vec2 normalizedCellOffset = { static_cast<float>(padding) / static_cast<float>(m_Texture->GetWidth()), static_cast<float>(padding) / static_cast<float>(m_Texture->GetHeight()) };

		for (int32_t row = 0; row < m_GridDimensions.y; row++) {
			for (int32_t col = 0; col < m_GridDimensions.x; col++) {
				glm::vec2 cellNormalizedPos = { normalizedCellSize.x * static_cast<float>(col), 1.0f - normalizedCellSize.y * static_cast<float>(row + 1) };
				glm::vec2 spriteNormalizedPos = cellNormalizedPos + normalizedCellOffset;
				m_SpriteUVs.emplace_back(UVs{ spriteNormalizedPos, spriteNormalizedPos + normalizedSpriteSize });
			}
		}
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::SpriteAtlas }, "Created SpriteAtlas: '{}', grid dimensions: '{}', total sprite count: '{}'", m_Name, glm::to_string(m_GridDimensions), m_SpriteCount);
	}

	const UVs& SpriteAtlas::GetSpriteUVsAtIndex(uint32_t index) const {
		if (CORI_CORE_CHECK(index + 1 <= m_SpriteCount, "Requested a sprite UVs from '{}' at invalid index: {} : returned data from index: 0", m_Name, index)) { return m_SpriteUVs[0]; }
		return m_SpriteUVs[index];
	}

	const UVs& SpriteAtlas::GetSpriteUVsAtPosition(glm::u16vec2 pos) const  {
		if (CORI_CORE_CHECK(pos.x * pos.y <= m_SpriteCount, "Requested a sprite UVs from '{}' at invalid position: ({}, {}) : returned data from position: (0, 0)", m_Name, pos.x, pos.y)) { return m_SpriteUVs[0]; }
		return m_SpriteUVs[pos.x + m_GridDimensions.x * pos.y];
	}

	std::shared_ptr<Texture2D> SpriteAtlas::GetTexture() const {
		return m_Texture;
	}

}
