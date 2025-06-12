#pragma once
#include "Texture2DDescriptor.hpp"

namespace Cori {
	class SpriteAtlasDescriptor {
	public:
		constexpr SpriteAtlasDescriptor(std::string_view debugName, Texture2DDescriptor textureDescriptor, glm::ivec2 spriteResolution, float spriteScale = 1.0f) noexcept
			: m_TextureDescriptor(textureDescriptor),
			m_SpriteResolution(spriteResolution),
			m_SpriteScale(spriteScale),
			m_DebugName(debugName),
			m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		inline uint32_t GetRuntimeID() const { return m_RuntimeID; }
		inline std::string_view GetDebugName() const { return m_DebugName; }

		constexpr auto operator<=>(const SpriteAtlasDescriptor& other) const noexcept {
			return m_RuntimeID <=> other.m_RuntimeID;
		}

		constexpr bool operator==(const SpriteAtlasDescriptor& other) const noexcept {
			return m_RuntimeID == other.m_RuntimeID;
		}

		struct Hasher {
			std::size_t operator()(const SpriteAtlasDescriptor& descriptor) const noexcept {
				return std::hash<uint32_t>{}(descriptor.m_RuntimeID);
			}
		};

		const Texture2DDescriptor m_TextureDescriptor;
		const glm::ivec2 m_SpriteResolution;
		const float m_SpriteScale;

	private:
		const std::string_view m_DebugName;


		const uint32_t m_RuntimeID{ 0 };
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };

	};
}