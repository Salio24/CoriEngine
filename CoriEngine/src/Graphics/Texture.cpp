#include "Texture.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_Texture.hpp"

namespace Cori {
	std::shared_ptr<Texture2D> Texture2D::Create(const std::shared_ptr<Image>& image) {
		image->FlipVertically();
		std::shared_ptr<Texture2D> texture = Factory<Texture2D, GraphicsAPIs, const void*, const uint32_t, const uint32_t, const bool, const Texture::PixelFormat, const Texture::WrapMode, const Texture::Filter>::CreateShared(Window::GetCurrentAPI(), image->GetPixelData(), image->GetWidth(), image->GetHeight(), image->HasSemiTransparency(), RGBA8888, CLAMP_TO_EDGE, NEAREST);
		CORI_CORE_ASSERT(texture, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Window::GetCurrentAPI()));
		return texture;
	}

	std::shared_ptr<Texture2D> Texture2D::Create(const void* pixelData, const uint32_t width, const uint32_t height, const bool hasSemiTransparency, const PixelFormat pixelFormat, const WrapMode wrapMode, const Filter filter) {
		std::shared_ptr<Texture2D> texture = Factory<Texture2D, GraphicsAPIs, const void*, const uint32_t, const uint32_t, const bool, const Texture::PixelFormat, const Texture::WrapMode, const Texture::Filter>::CreateShared(Window::GetCurrentAPI(), pixelData, width, height, hasSemiTransparency, pixelFormat, wrapMode, filter);
		CORI_CORE_ASSERT(texture, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Window::GetCurrentAPI()));
		return texture;
	}
}
