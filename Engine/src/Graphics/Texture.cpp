#include "Texture.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_Texture.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<Texture2D> Texture2D::Create(const std::shared_ptr<Image>& image) {
			image->FlipVertically();
			const Params params { .m_HasSemiTransparency = image->HasSemiTransparency() };
			return Create(image->GetPixelData(), image->GetWidth(), image->GetHeight(), params);
		}

		std::shared_ptr<Texture2D> Texture2D::Create(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params) {
			switch (Core::Window::GetCurrentAPI()) {
			case GraphicsAPIs::OpenGL:
				{
					auto texture = std::make_shared<Internal::OpenGLTexture2D>();
					texture->Upload(pixelData, width, height, params);
					CORI_CORE_ASSERT(texture->GetStatus() == AssetStatus::READY, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Core::Window::GetCurrentAPI()));
					return texture;
					break;
				}
			case GraphicsAPIs::Vulkan:
				{
					CORI_CORE_ASSERT(false, "Unsupported Graphics API.");
				}
			case GraphicsAPIs::None:
				{
					break;
				}
			}

			return nullptr;
		}

		std::shared_ptr<Texture2D> Texture2D::Create(const Descriptor& descriptor) {
			const auto image = Image::Create(descriptor.m_ImagePath);
			return Create(image);
		}
	}
}
