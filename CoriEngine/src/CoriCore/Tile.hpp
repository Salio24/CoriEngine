#pragma once
#include "AssetManager/SpriteAtlasDescriptor.hpp"
#include "Renderer/Sprite.hpp"
#include "Profiling/Trackable.hpp"
#include "SelfFactory.hpp"

namespace Cori {
	class Tile : public Profiling::Trackable<Tile>, public SharedSeflFactory<Tile>{
	public:
		static bool PreCreateHook(const glm::vec2 position, const glm::vec2 size, const SpriteAtlasDescriptor& descriptor, const uint32_t index);
		
		const std::shared_ptr<Sprite> GetSprite() const { return m_Sprite; }

		const glm::vec2 GetPosition() const { return m_Position; }

		const glm::vec2 GetSize() const { return m_Size; }

	protected: 
		Tile(const glm::vec2 position, const glm::vec2 size, const SpriteAtlasDescriptor& descriptor, const uint32_t index); 
	private:

		glm::vec2 m_Position{ 0.0f };
		glm::vec2 m_Size{ 1.0f };

		std::shared_ptr<Sprite> m_Sprite;
	};
}