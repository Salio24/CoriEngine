// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Image.hpp"
#include <SDL3_image/SDL_image.h>
#include "../AssetManager/FileManager.hpp" 

namespace Cori {
	Image::Image(const std::string& path) {
		m_Surface = IMG_Load(path.c_str());

		if CORI_CORE_ASSERT_ERROR(static_cast<SDL_Surface*>(m_Surface), "Image: Failed to load image: '{0}', {1}", path, SDL_GetError()) {
			m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
		}
		else {
			m_Status = true;
		}
		
		if (static_cast<SDL_Surface*>(m_Surface)->format != SDL_PIXELFORMAT_RGBA32) {
			CORI_CORE_WARN("Image: Converting '{0}' to RGBA32 (ARGB8888) format\n    You should convert all the images you're loading to RGBA32 before loading them; you shouldn't rely on the engine's automatic conversion.", path);

			SDL_Surface* converted = SDL_ConvertSurface(static_cast<SDL_Surface*>(m_Surface), SDL_PIXELFORMAT_RGBA32);
			if CORI_CORE_ASSERT_ERROR(converted, "Image: Failed to convert image: '{0}', {1}", path, SDL_GetError()) { 
				m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
				SDL_DestroySurface(converted);
				
			}
			else {
				SDL_DestroySurface(static_cast<SDL_Surface*>(m_Surface));
				m_Surface = static_cast<void*>(converted);
				m_Status = false;
			}
		}

		if (m_Status) {
			CORI_CORE_TRACE("Image: Loaded image '{0}' successfully", path);
		}
		else {
			CORI_CORE_WARN("Image: Failed to load image '{0}', replacing it with a placeholder", path);
		}

		m_Status = true;
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

	void Image::FlipVertically() {

		// Lock pixels for direct access

		if (CORI_CORE_VERIFY_ERROR(SDL_LockSurface(static_cast<SDL_Surface*>(m_Surface)) != 0, "FlipVertically: failed to lock surface: {0}", SDL_GetError())) { return; }

		const int height = static_cast<SDL_Surface*>(m_Surface)->h;
		const int pitch = static_cast<SDL_Surface*>(m_Surface)->pitch;
		auto pixels = static_cast<uint8_t*>(static_cast<SDL_Surface*>(m_Surface)->pixels);

		std::vector<uint8_t> rowBuffer(pitch);

		for (int y = 0; y < height / 2; ++y) {
			uint8_t* topRow = pixels + y * pitch;
			uint8_t* bottomRow = pixels + (height - 1 - y) * pitch;

			std::copy_n(topRow, pitch, rowBuffer.data());
			std::copy_n(bottomRow, pitch, topRow);
			std::copy_n(rowBuffer.data(), pitch, bottomRow);
		}

		SDL_UnlockSurface(static_cast<SDL_Surface*>(m_Surface));
	}

	void Image::FlipHorizontally() {
		if (CORI_CORE_VERIFY_ERROR(SDL_LockSurface(static_cast<SDL_Surface*>(m_Surface)) != 0, "FlipHorizontally: failed to lock surface: {0}", SDL_GetError())) { return; }

		const int width = static_cast<SDL_Surface*>(m_Surface)->w;
		const int height = static_cast<SDL_Surface*>(m_Surface)->h;
		const int pitch = static_cast<SDL_Surface*>(m_Surface)->pitch;
		const int bpp = 4;
		auto      pixels = static_cast<uint8_t*>(static_cast<SDL_Surface*>(m_Surface)->pixels);

		for (int y = 0; y < height; ++y) {
			uint8_t* rowStart = pixels + y * pitch;
			for (int x = 0; x < width / 2; ++x) {
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