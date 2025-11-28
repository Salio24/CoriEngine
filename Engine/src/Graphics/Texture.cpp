#include "Texture.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<Texture2D> Texture2D::Create(const std::shared_ptr<Image>& image) {
			image->FlipVertically();
			const Params params { .m_HasSemiTransparency = image->HasSemiTransparency() };
			return Create(image->GetPixelData(), image->GetWidth(), image->GetHeight(), params);
		}

		std::shared_ptr<Texture2D> Texture2D::Create(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params) {
			return nullptr;
		}

		std::shared_ptr<Texture2D> Texture2D::Create(const Descriptor& descriptor) {
			const auto image = Image::Create(descriptor.m_ImagePath);
			return Create(image);
		}
	}
}
