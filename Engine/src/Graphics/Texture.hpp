#pragma once
#include "Image.hpp"
#include "AssetManager/AssetLoadStatus.hpp"

namespace Cori {
	class AssetManager;

	namespace Graphics {
		struct UVs {
			glm::vec2 UVmin{ 0.0f, 0.0f };
			glm::vec2 UVmax{ 1.0f, 1.0f };

			explicit operator glm::vec4() const { return { UVmin, UVmax }; }
		};

		/**
		 * @brief Abstract class for textures.
		 */
		class Texture {
		public:
			/**
			 * @brief Available pixel formats.
			 */
			enum PixelFormat {
				RGBA8888, RGB888
			};

			/**
			 * @brief Available wrap modes.
			 */
			enum WrapMode {
				CLAMP_TO_EDGE, CLAMP_TO_BORDER, REPEAT
			};

			/**
			 * @brief Available filtering options.
			 */
			enum Filter {
				LINEAR, NEAREST
			};

			/**
			 * @brief Params to use when creating a texture.
			 */
			struct Params {
				PixelFormat m_PixelFormat{ RGBA8888 };
				WrapMode m_WrapMode{ CLAMP_TO_EDGE };
				Filter m_Filter{ NEAREST };

				/**
				 * @brief Generally you want to leave this by default if you're creating a texture from an image, they all have align of 4.
				 */
				int32_t m_UnpackAlignment{ 0 };

				/**
				 * @brief Whether or no to flag the texture as semi transparent.
				 */
				bool m_HasSemiTransparency{ false };
			};

			virtual ~Texture() = default;

			/**
			 * @brief Gives the texture width.
			 * @return Texture width.
			 */
			[[nodiscard]] virtual uint32_t GetWidth() const = 0;

			/**
			 * @brief Gives the texture height.
			 * @return Texture height.
			 */
			[[nodiscard]] virtual uint32_t GetHeight() const = 0;

			/**
			 * @brief Checks if texture has semi transparency.
			 * @return
			 */
			[[nodiscard]] virtual bool HasSemiTransparency() const = 0;

			/**
			 * @brief Binds the texture to the slot.
			 * @param slot Slot to bound the texture to.
			 * @note Don't touch this if you're not using it outside the provided Renderer2D.
			 */
			virtual void Bind(uint32_t slot) const = 0;

			AssetStatus GetStatus() const {
				return m_Status;
			}

		protected:
			AssetStatus m_Status{ AssetStatus::UNSPECIFIED };
		};

		/**
		 * @brief A regular 2D texture with 1 layer.
		 */
		class Texture2D : public Texture {
		public:
			/**
			 * @brief Texture2D Descriptor meant to be used with AssetManager only.
			 */
			class Descriptor {
			public:
				/**
				 * @brief Constructs a descriptor.
				 * @param name Name to be used in AssetManager logging.
				 * @param imagePath Path to the image that will be used to create the Texture2D.
				 */
				constexpr Descriptor(std::string name, std::filesystem::path imagePath)
					: m_ImagePath(std::move(imagePath)),
					m_Name(std::move(name)),
					m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
				{ }

				using AssetType = Texture2D;

				[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

				constexpr bool operator==(const Descriptor& other) const {
					return m_RuntimeID == other.m_RuntimeID;
				}

				struct Hasher {
					std::size_t operator()(const Descriptor& descriptor) const {
						return std::hash<uint32_t>{}(descriptor.m_RuntimeID);
					}
				};

				const std::filesystem::path m_ImagePath;
				const std::string m_Name;

			private:
				const uint32_t m_RuntimeID{ 0 };
				inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
			};

			/**
			 * @brief Creates a Texture2D from the Image.
			 * @param image Image to create Texture2D from.
			 * @return Shared pointer to the created Texture2D.
			 */
			[[nodiscard]] static std::shared_ptr<Texture2D> Create(const std::shared_ptr<Image>& image);

			/**
			 * @brief Creates a Texture2D.
			 * @param pixelData Pixel data to create Texture2D from.
			 * @param width Width of the source picture data.
			 * @param height Height of the source picture data.
			 * @param params Parameters to create a texture with.
			 * @return Shared pointer to the created Texture2D.
			 */
			[[nodiscard]] static std::shared_ptr<Texture2D> Create(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params);

		private:
			friend AssetManager;
			virtual void Upload(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params) = 0;

			[[nodiscard]] static std::shared_ptr<Texture2D> Create(const Descriptor& descriptor);
		};
	}
}