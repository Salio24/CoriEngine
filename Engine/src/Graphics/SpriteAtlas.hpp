#pragma once
#include "Texture.hpp"
#include "Image.hpp"
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		/**
		 * @brief Holds the sprite atlas and UVs for sprites in it. All spite atlases are padded, so no sprite atlas bleeding will occur.
		 * @note If SpriteAtlas fails to creat from the specified image, it will be created with the placeholder texture.
		 */
		class SpriteAtlas : public Profiling::Trackable<SpriteAtlas> {
		public:
			/**
			 * @brief SpriteAtlas Descriptor meant to be used with AssetManager only.
			 */
			class Descriptor {
			public:
				/**
				 * @brief Constructs a descriptor. It's recommended to use "inline const" when defining the Descriptor in a namespace
				 * @param name Name to be used assigned to SpriteAtlas.
				 * @param imagePath Path to the image that will be used to create the SpriteAtlas.
				 * @param spriteResolution Resolution of one sprite in an atlas.
				 * @note Total image size should be divisible by spriteResolution without a remainder.
				 */
				constexpr Descriptor(std::string name, std::filesystem::path imagePath, const glm::uvec2 spriteResolution) noexcept
					: m_TexturePath(std::move(imagePath)),
					m_SpriteResolution(spriteResolution),
					m_Name(std::move(name)),
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

			/**
			 * @brief Creates a SpriteAtlas.
			 * @param name Name to be assigned to the SpriteAtlas.
			 * @param image Image that will be used to create the SpriteAtlas.
			 * @param spriteResolution Resolution of one sprite in an atlas.
			 * @note Total image size should be divisible by spriteResolution without a remainder.
			 * @return Spared pointer to the created SpriteAtlas.
			 */
			[[nodiscard]] static std::shared_ptr<SpriteAtlas> Create(std::string name, const std::shared_ptr<Image>& image, const glm::u16vec2 spriteResolution);

			/**
			 * @brief Request the UVs for the spite at specific index.
			 * @param index Sprite index to request.
			 * @return UVs for the requested sprite, or UVs for sprite at index 0 if no sprite with the specified index exist in the SpriteAtlas.
			 */
			[[nodiscard]] const UVs& GetSpriteUVsAtIndex(uint32_t index) const;

			/**
			 * @brief Request the UVs for the spite at specific position.
			 * @param pos Sprite position to request.
			 * @return UVs for the requested sprite, or UVs for sprite at position (0, 0) if no sprite with the specified position in the SpriteAtlas.
			 */
			[[nodiscard]] const UVs& GetSpriteUVsAtPosition(glm::u16vec2 pos) const;

			/**
			 * @brief Checks if the SpriteAtlas was created successfully.
			 * @return True if successful, false otherwise.
			 */
			[[nodiscard]] bool GetSuccessStatus() const;

			/**
			 * @brief Retrieves the Texture2D stored in the SpriteAtlas.
			 * @return Shared pointer to the Texture2D.
			 */
			[[nodiscard]] std::shared_ptr<Texture2D> GetTexture() const;

		private:
			explicit SpriteAtlas(std::string name, const std::shared_ptr<Image>& image, const glm::u16vec2 spriteResolution, const bool success);

			friend AssetManager;
			[[nodiscard]] static std::shared_ptr<SpriteAtlas> Create(const Descriptor& descriptor);
			std::string m_Name;

			uint32_t m_SpriteCount;
			std::shared_ptr<Texture2D> m_Texture;
			glm::u16vec2 m_GridDimensions;

			std::vector<UVs> m_SpriteUVs;

			bool m_Success;
		};
	}
}