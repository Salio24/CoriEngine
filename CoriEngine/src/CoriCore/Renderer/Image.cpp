#include "Image.hpp"
#include <SDL3_image/SDL_image.h>
#include "../FileManager.hpp" 

namespace Cori {
	Image::Image(const std::string& path) {
		surface = IMG_Load(path.c_str());

		if CORI_CORE_ASSERT_ERROR(static_cast<SDL_Surface*>(surface), "Failed to load image: {0}, {1}", FileManager::GetFilename(path), SDL_GetError()) { 
			surface = IMG_Load("assets/engine/textures/missing_texture32.png");

			//return; SDL_DestroySurface(static_cast<SDL_Surface*>(surface)); 
		}

		if (static_cast<SDL_Surface*>(surface)->format != SDL_PIXELFORMAT_RGBA32) {
			CORI_CORE_WARN("Converting {0} to RGBA32 (ARGB8888) format\n    You should convert all the images you're loading to RGBA32 before loading them; you shouldn't rely on the engine's automatic conversion.", FileManager::GetFilename(path));

			SDL_Surface* converted = SDL_ConvertSurface(static_cast<SDL_Surface*>(surface), SDL_PIXELFORMAT_RGBA32);
			if CORI_CORE_ASSERT_ERROR(converted, "Failed to convert image: {0}, {1}", FileManager::GetFilename(path), SDL_GetError()) { return; SDL_DestroySurface(static_cast<SDL_Surface*>(surface)); SDL_DestroySurface(converted); }

			SDL_DestroySurface(static_cast<SDL_Surface*>(surface));
			surface = static_cast<void*>(converted);
		}

		CORI_CORE_TRACE("Loaded image {0} successfully", FileManager::GetFilename(path));
	}

	Image::~Image() {
		SDL_DestroySurface(static_cast<SDL_Surface*>(surface));
	}

	void* Image::GetPixelData() const {
		return static_cast<SDL_Surface*>(surface)->pixels;
	}

	uint32_t Image::GetWidth() const {
		return static_cast<SDL_Surface*>(surface)->w;
	}

	uint32_t Image::GetHeight() const {
		return static_cast<SDL_Surface*>(surface)->h;
	}

	void Image::FlipVertically() {

		// Lock pixels for direct access

		if CORI_CORE_VERIFY_ERROR(SDL_LockSurface(static_cast<SDL_Surface*>(surface)) != 0, "FlipVerticaly: failed to lock surface: {0}", SDL_GetError()) return;

		const int height = static_cast<SDL_Surface*>(surface)->h;
		const int pitch = static_cast<SDL_Surface*>(surface)->pitch;
		auto pixels = static_cast<uint8_t*>(static_cast<SDL_Surface*>(surface)->pixels);

		std::vector<uint8_t> rowBuffer(pitch);

		for (int y = 0; y < height / 2; ++y) {
			uint8_t* topRow = pixels + y * pitch;
			uint8_t* bottomRow = pixels + (height - 1 - y) * pitch;

			std::copy_n(topRow, pitch, rowBuffer.data());
			std::copy_n(bottomRow, pitch, topRow);
			std::copy_n(rowBuffer.data(), pitch, bottomRow);
		}

		SDL_UnlockSurface(static_cast<SDL_Surface*>(surface));
	}

	void Image::FlipHorizontally() {
		if CORI_CORE_VERIFY_ERROR(SDL_LockSurface(static_cast<SDL_Surface*>(surface)) != 0, "FlipVerticaly: failed to lock surface: {0}", SDL_GetError()) return;

		const int width = static_cast<SDL_Surface*>(surface)->w;
		const int height = static_cast<SDL_Surface*>(surface)->h;
		const int pitch = static_cast<SDL_Surface*>(surface)->pitch;
		const int bpp = 4;
		auto      pixels = static_cast<uint8_t*>(static_cast<SDL_Surface*>(surface)->pixels);

		for (int y = 0; y < height; ++y) {
			uint8_t* rowStart = pixels + y * pitch;
			for (int x = 0; x < width / 2; ++x) {
				uint8_t* leftPixel = rowStart + x * bpp;
				uint8_t* rightPixel = rowStart + (width - 1 - x) * bpp;

				std::swap_ranges(leftPixel, leftPixel + bpp, rightPixel);
			}
		}

		SDL_UnlockSurface(static_cast<SDL_Surface*>(surface));
	}

	void Image::Mirror() {
		FlipVertically();
		FlipHorizontally();
	}
}