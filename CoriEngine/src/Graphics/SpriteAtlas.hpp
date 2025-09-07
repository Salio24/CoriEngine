#pragma once
#include "Texture.hpp"
#include "Image.hpp"
#include "Profiling/Trackable.hpp"

namespace Cori {
	class SpriteAtlas : public Profiling::Trackable<SpriteAtlas> {
	public:
		class Descriptor {
		public:
			constexpr Descriptor(std::string debugName, std::filesystem::path texturePath, const glm::uvec2 spriteResolution) noexcept
				: m_TexturePath(std::move(texturePath)),
				m_SpriteResolution(spriteResolution),
				m_Name(std::move(debugName)),
				m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
			{ }

			using AssetType = SpriteAtlas;

			[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

			constexpr bool operator==(const Descriptor& other) const noexcept {
				return m_RuntimeID == other.m_RuntimeID;
			}

			struct Hasher {
				std::size_t operator()(const Descriptor& descriptor) const noexcept {
					return std::hash<uint32_t>{}(descriptor.m_RuntimeID);
				}
			};

			const std::filesystem::path m_TexturePath;
			const glm::uvec2 m_SpriteResolution;
			const std::string m_Name;

		private:
			const uint32_t m_RuntimeID{ 0 };
			inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };

		};

		static std::shared_ptr<SpriteAtlas> Create(std::string name, const std::shared_ptr<Image>& image, const glm::u16vec2 spriteResolution);

		static std::shared_ptr<SpriteAtlas> Create(const Descriptor& descriptor);

		[[nodiscard]] const UVs& GetSpriteUVsAtIndex(uint32_t index) const;

		[[nodiscard]] const UVs& GetSpriteUVsAtPosition(glm::u16vec2 pos) const;

		[[nodiscard]] bool GetSuccessStatus() const;

		[[nodiscard]] std::shared_ptr<Texture2D> GetTexture() const;

	private:
		explicit SpriteAtlas(std::string name, const std::shared_ptr<Image>& image, const glm::u16vec2 spriteResolution, const bool success);

		std::string m_Name;

		uint32_t m_SpriteCount;
		std::shared_ptr<Texture2D> m_Texture;
		glm::u16vec2 m_GridDimensions;

		std::vector<UVs> m_SpriteUVs;

		bool m_Success;
	};
}