#pragma once
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		/**
		 * @brief Used to load an image. It can flip and image and add padding to it. Mainly used for texture loading.
		 */
		class Image : public Profiling::Trackable<Image> {
		public:
			/**
			 * @brief Flips a loaded image vertically.
			 */
			void FlipVertically();

			/**
			 * @brief Flips a loaded image horizontally.
			 */
			void FlipHorizontally();


			/**
			 * @brief Gives you the pointer to the start of the pixel data, format is RGBA8888.
			 * @return Void pointer to the pixel data.
			 */
			[[nodiscard]] void* GetPixelData() const;

			/**
			 * @brief Gives you the image width.
			 * @return Image width.
			 */
			[[nodiscard]] uint32_t GetWidth() const;

			/**
			 * @brief Gives you the image height.
			 * @return Image height.
			 */
			[[nodiscard]] uint32_t GetHeight() const;

			/**
			 * @brief Checks if an image has semi transparency or no.
			 * @details Image is considered semi transparent if at least one Alpha chanel Byte is not 0x00 or not 0xFF.
			 * @return True if semi transparent, false otherwise.
			 */
			[[nodiscard]] bool HasSemiTransparency() const;

			/**
			 * @brief Checks if the image was loaded successfully.
			 * @return True means image was loaded successful, false means image loading failed and the image contains a placeholder.
			 */
			[[nodiscard]] bool GetSuccessStatus() const { return m_SuccessStatus; }

			/**
			 * @brief Checks if an image is padded.
			 * @return True if is padded, false otherwise.
			 */
			[[nodiscard]] bool IsPadded() const { return m_IsPadded; }

			/**
			 * @brief Adds padding to the image that will be used in a sprite atlas.
			 * @details It is needed to avoid the GPU from sampling a wrong texel in an event of floating point error.
			 * Prevents texel bleeding, or some call it sprite atlas/sheet bleeding. SpriteAtlas does this automatically, so no need to call this manually before giving the Image to the SpriteAtlas.
			 * @param spriteResolution Resolution of one Sprite in a SpriteAtlas.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> AddPadding(const glm::u16vec2 spriteResolution);

			/**
			 * @brief Creates an Image from the picture at the specified path.
			 * @param path Picture path.
			 * @return Shared pointer to the created Image object.
			 */
			[[nodiscard]] static std::shared_ptr<Image> Create(const std::filesystem::path& path);

			~Image();

		private:
			explicit Image(const std::filesystem::path& path);

			bool m_HasSemiTransparency{ false };

			bool m_SuccessStatus{ false };
			bool m_IsPadded{ false };
			void* m_Surface;
		};
	}
}