#include "Image.hpp"
#include <SDL3_image/SDL_image.h>

namespace Cori {
	Image::Image(const std::filesystem::path& path) {
		if (std::filesystem::exists(path)) {
			m_Surface = IMG_Load(path.c_str());
		} else {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Could not find an image at the specified path: '{}', a placeholder will be loaded instead.", path.string());
			m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
		}


		if (!m_Surface) {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Failed to load image at path: '{}'. SDL_Error: '{}'", path.string(), SDL_GetError());
			m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
		}
		else {
			m_Status = true;
		}

		if (m_Status) {
			if (static_cast<SDL_Surface*>(m_Surface)->format != SDL_PIXELFORMAT_RGBA32) {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Converting '{}' to RGBA32 (ARGB888) format. Initial format ID: '{}'. (Refer to SDL3s' SDL_PixelFormat to get the exact format from ID)", path.string(), static_cast<uint32_t>(static_cast<SDL_Surface*>(m_Surface)->format));

				SDL_Surface* converted = SDL_ConvertSurface(static_cast<SDL_Surface*>(m_Surface), SDL_PIXELFORMAT_RGBA32);
				if (!converted) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Failed to convert image at path: '{}'. Loading placeholder. SDL_Error: '{}'", path.string(), SDL_GetError());
					m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
					SDL_DestroySurface(converted);
					m_Status = false;
				}
				else {
					SDL_DestroySurface(static_cast<SDL_Surface*>(m_Surface));
					m_Surface = static_cast<void*>(converted);
				}
			}

			const auto pixels = static_cast<Uint8*>(static_cast<SDL_Surface*>(m_Surface)->pixels);
			for (int32_t y = 0; y < static_cast<SDL_Surface*>(m_Surface)->h; ++y) {
				const Uint8* row = pixels + (y * static_cast<SDL_Surface*>(m_Surface)->pitch);
				for (int32_t x = 0; x < static_cast<SDL_Surface*>(m_Surface)->w; ++x) {
					const Uint8 alpha = row[x * 4 + 3];
					if (alpha < 255 && alpha != 0) {
						m_HasSemiTransparency = true;
						break;
					}
				}
			}
		}

		if (m_Status) {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image } ,"Loaded image from path: '{}' successfully", path.string());
		}
		else {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image } ,"Failed to load image at path '{}', a placeholder was loaded instead.", path.string());
		}
	}

	Image::~Image() {
		SDL_DestroySurface(static_cast<SDL_Surface*>(m_Surface));
	}

	void* Image::GetPixelData() const {
		return static_cast<SDL_Surface*>(m_Surface)->pixels;
	}

	uint32_t Image::GetWidth() const {
		return static_cast<SDL_Surface*>(m_Surface)->w;
	}

	uint32_t Image::GetHeight() const {
		return static_cast<SDL_Surface*>(m_Surface)->h;
	}

	bool Image::HasSemiTransparency() const {
		return m_HasSemiTransparency;
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void Image::FlipVertically() {
		if (CORI_CORE_CHECK(SDL_LockSurface(static_cast<SDL_Surface*>(m_Surface)) != 0, "FlipVertically: Failed to lock surface. SDL_Error: {}", SDL_GetError())) { return ;}

		const int32_t height = static_cast<SDL_Surface*>(m_Surface)->h;
		const int32_t pitch = static_cast<SDL_Surface*>(m_Surface)->pitch;
		const auto pixels = static_cast<uint8_t*>(static_cast<SDL_Surface*>(m_Surface)->pixels);

		std::vector<uint8_t> rowBuffer(pitch);

		for (int32_t y = 0; y < height / 2; ++y) {
			uint8_t* topRow = pixels + y * pitch;
			uint8_t* bottomRow = pixels + (height - 1 - y) * pitch;

			std::copy_n(topRow, pitch, rowBuffer.data());
			std::copy_n(bottomRow, pitch, topRow);
			std::copy_n(rowBuffer.data(), pitch, bottomRow);
		}

		SDL_UnlockSurface(static_cast<SDL_Surface*>(m_Surface));
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void Image::FlipHorizontally() {
		if (CORI_CORE_CHECK(SDL_LockSurface(static_cast<SDL_Surface*>(m_Surface)) != 0, "FlipHorizontally: Failed to lock surface. SDL_Error: {}", SDL_GetError())) { return ;}


		const int32_t width = static_cast<SDL_Surface*>(m_Surface)->w;
		const int32_t height = static_cast<SDL_Surface*>(m_Surface)->h;
		const int32_t pitch = static_cast<SDL_Surface*>(m_Surface)->pitch;
		const int32_t bpp = 4;
		const auto pixels = static_cast<uint8_t*>(static_cast<SDL_Surface*>(m_Surface)->pixels);

		for (int32_t y = 0; y < height; ++y) {
			uint8_t* rowStart = pixels + y * pitch;
			for (int32_t x = 0; x < width / 2; ++x) {
				uint8_t* leftPixel = rowStart + x * bpp;
				uint8_t* rightPixel = rowStart + (width - 1 - x) * bpp;

				std::swap_ranges(leftPixel, leftPixel + bpp, rightPixel);
			}
		}

		SDL_UnlockSurface(static_cast<SDL_Surface*>(m_Surface));
	}

	void Image::Mirror() {
		FlipVertically();
		FlipHorizontally();
	}
}