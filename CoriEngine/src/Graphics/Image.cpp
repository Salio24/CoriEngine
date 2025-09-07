#include "Image.hpp"
#include <SDL3_image/SDL_image.h>

namespace {
	[[nodiscard]] Uint32 GetPixel32(const SDL_Surface* surface, const int32_t x, const int32_t y) {
		const auto* pixels = static_cast<Uint32*>(surface->pixels);
		return pixels[y * (surface->pitch / sizeof(Uint32)) + x];
	}

	void SetPixel32(const SDL_Surface* surface, const int32_t x, const int32_t y, const Uint32 pixel) {
		auto* pixels = static_cast<Uint32*>(surface->pixels);
		pixels[y * (surface->pitch / sizeof(Uint32)) + x] = pixel;
	}
}

namespace Cori {
	Image::Image(const std::filesystem::path& path) {
		if (std::filesystem::exists(path)) {
			m_Surface = IMG_Load(path.c_str());
		} else {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Could not find an image at the specified path: '{}', a placeholder will be loaded instead.", path.string());
			m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
			CORI_CORE_ASSERT(m_Surface, "Failed to load a placeholder image, placeholder path: 'assets/engine/textures/missing_texture32.png'");
		}


		if (!m_Surface) {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Failed to load image at path: '{}'. SDL_Error: '{}'", path.string(), SDL_GetError());
			m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
			CORI_CORE_ASSERT(m_Surface, "Failed to load a placeholder image, placeholder path: 'assets/engine/textures/missing_texture32.png'");
		}
		else {
			m_SuccessStatus = true;
		}

		if (m_SuccessStatus) {
			if (static_cast<SDL_Surface*>(m_Surface)->format != SDL_PIXELFORMAT_RGBA32) {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Converting '{}' to RGBA32 (ARGB888) format. Initial format ID: '{}'. (Refer to SDL3s' SDL_PixelFormat to get the exact format from ID)", path.string(), static_cast<uint32_t>(static_cast<SDL_Surface*>(m_Surface)->format));

				SDL_Surface* converted = SDL_ConvertSurface(static_cast<SDL_Surface*>(m_Surface), SDL_PIXELFORMAT_RGBA32);
				if (!converted) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Failed to convert image at path: '{}'. Loading placeholder. SDL_Error: '{}'", path.string(), SDL_GetError());
					m_Surface = IMG_Load("assets/engine/textures/missing_texture32.png");
					CORI_CORE_ASSERT(m_Surface, "Failed to load a placeholder image, placeholder path: 'assets/engine/textures/missing_texture32.png'");
					SDL_DestroySurface(converted);
					m_SuccessStatus = false;
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

		if (m_SuccessStatus) {
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

	std::expected<void, CoriError<>> Image::AddPadding(const glm::u16vec2 spriteResolution) {
		auto* originalSurface = static_cast<SDL_Surface*>(m_Surface);

		const uint32_t cols = originalSurface->w / spriteResolution.x;
		const uint32_t rows = originalSurface->h / spriteResolution.y;
		constexpr int32_t padding = 1;

		const uint32_t newWidth = cols * spriteResolution.x + cols * padding * 2;
		const uint32_t newHeight = rows * spriteResolution.y + rows * padding * 2;

		SDL_Surface* paddedSurface = SDL_CreateSurface(static_cast<int32_t>(newWidth), static_cast<int32_t>(newHeight), originalSurface->format);
		if (!paddedSurface) {
			return std::unexpected(CoriError(std::format("Failed to create new padded surface. SDL_Error: {}", SDL_GetError())));
		}

		SDL_FillSurfaceRect(paddedSurface, nullptr, 0x00000000);

		for (uint32_t row = 0; row < rows; ++row) {
			for (uint32_t col = 0; col < cols; ++col) {
				SDL_Rect srcRect = {
						static_cast<int32_t>(col * spriteResolution.x),
						static_cast<int32_t>(row * spriteResolution.y),
						static_cast<int32_t>(spriteResolution.x),
						static_cast<int32_t>(spriteResolution.y)
					};

				SDL_Rect dstRect = {
						padding + static_cast<int32_t>(col * (spriteResolution.x + padding * 2)),
						padding + static_cast<int32_t>(row * (spriteResolution.y + padding * 2)),
						static_cast<int32_t>(spriteResolution.x),
						static_cast<int32_t>(spriteResolution.y)
					};

				SDL_BlitSurface(originalSurface, &srcRect, paddedSurface, &dstRect);

				if (!SDL_LockSurface(paddedSurface) || !SDL_LockSurface(originalSurface)) {
					SDL_DestroySurface(paddedSurface);
					return std::unexpected(CoriError(std::format("Failed to lock surfaces for padding. SDL_Error: {}", SDL_GetError())));
				}

				for (int x = 0; x < srcRect.w; ++x) {
					const Uint32 topPixel = GetPixel32(originalSurface, srcRect.x + x, srcRect.y);
					const Uint32 bottomPixel = GetPixel32(originalSurface, srcRect.x + x, srcRect.y + srcRect.h - 1);
					for (int32_t p = 1; p <= padding; ++p) {
						SetPixel32(paddedSurface, dstRect.x + x, dstRect.y - p, topPixel);
						SetPixel32(paddedSurface, dstRect.x + x, dstRect.y + srcRect.h - 1 + p, bottomPixel);
					}
				}

				for (int y = 0; y < srcRect.h; ++y) {
					const Uint32 leftPixel = GetPixel32(originalSurface, srcRect.x, srcRect.y + y);
					const Uint32 rightPixel = GetPixel32(originalSurface, srcRect.x + srcRect.w - 1, srcRect.y + y);
					for (int32_t p = 1; p <= padding; ++p) {
						SetPixel32(paddedSurface, dstRect.x - p, dstRect.y + y, leftPixel);
						SetPixel32(paddedSurface, dstRect.x + srcRect.w - 1 + p, dstRect.y + y, rightPixel);
					}
				}

				const Uint32 tl = GetPixel32(originalSurface, srcRect.x, srcRect.y);
				const Uint32 tr = GetPixel32(originalSurface, srcRect.x + srcRect.w - 1, srcRect.y);
				const Uint32 bl = GetPixel32(originalSurface, srcRect.x, srcRect.y + srcRect.h - 1);
				const Uint32 br = GetPixel32(originalSurface, srcRect.x + srcRect.w - 1, srcRect.y + srcRect.h - 1);

				for (int32_t px = 1; px <= padding; ++px) {
					for (int32_t py = 1; py <= padding; ++py) {
						SetPixel32(paddedSurface, dstRect.x - px, dstRect.y - py, tl);
						SetPixel32(paddedSurface, dstRect.x + srcRect.w - 1 + px, dstRect.y - py, tr);
						SetPixel32(paddedSurface, dstRect.x - px, dstRect.y + srcRect.h - 1 + py, bl);
						SetPixel32(paddedSurface, dstRect.x + srcRect.w - 1 + px, dstRect.y + srcRect.h - 1 + py, br);
					}
				}

				SDL_UnlockSurface(originalSurface);
				SDL_UnlockSurface(paddedSurface);
			}
		}

		SDL_DestroySurface(originalSurface);
		m_Surface = static_cast<void*>(paddedSurface);
		m_IsPadded = true;

		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Image }, "Successfully added padding to image.");

		return {};
	}

	std::shared_ptr<Image> Image::Create(const std::filesystem::path& path) {
		return std::shared_ptr<Image>(new Image(path));
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void Image::FlipVertically() {
		if (CORI_CORE_CHECK(SDL_LockSurface(static_cast<SDL_Surface*>(m_Surface)) != 0, "FlipVertically: Failed to lock surface. SDL_Error: {}", SDL_GetError())) { return; }

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