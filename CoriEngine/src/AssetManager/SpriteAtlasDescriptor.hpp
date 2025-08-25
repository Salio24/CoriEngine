#pragma once
#include "Texture2DDescriptor.hpp"

namespace Cori {
	class SpriteAtlasDescriptor {
	public:
		constexpr SpriteAtlasDescriptor(std::string debugName, const Texture2DDescriptor& textureDescriptor, glm::ivec2 spriteResolution) noexcept
			: m_TextureDescriptor(textureDescriptor),
			m_SpriteResolution(spriteResolution),
			m_Name(std::move(debugName)),
			m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

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
		const std::string m_Name;

	private:
		const uint32_t m_RuntimeID{ 0 };
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };

	};
}