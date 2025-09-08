#include "Texture.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_Texture.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<Texture2D> Texture2D::Create(const std::shared_ptr<Image>& image) {
			image->FlipVertically();
			const Params params { .m_HasSemiTransparency = image->HasSemiTransparency() };
			std::shared_ptr<Texture2D> texture = Core::Factory<Texture2D, GraphicsAPIs, const void*, const uint32_t, const uint32_t, const Params&>::CreateShared(Core::Window::GetCurrentAPI(), image->GetPixelData(), image->GetWidth(), image->GetHeight(), params);
			CORI_CORE_ASSERT(texture, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
			return texture;
		}

		std::shared_ptr<Texture2D> Texture2D::Create(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params) {
			std::shared_ptr<Texture2D> texture = Core::Factory<Texture2D, GraphicsAPIs, const void*, const uint32_t, const uint32_t, const Params&>::CreateShared(Core::Window::GetCurrentAPI(), pixelData, width, height, params);
			CORI_CORE_ASSERT(texture, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
			return texture;
		}

		std::shared_ptr<Texture2D> Texture2D::Create(const Descriptor& descriptor) {
			const auto image = Image::Create(descriptor.m_ImagePath);
			return Create(image);
		}
	}
}
