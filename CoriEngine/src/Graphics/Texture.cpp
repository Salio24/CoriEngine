#include "Texture.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_Texture.hpp"

namespace Cori {
	std::shared_ptr<Texture2D> Texture2D::Create(const std::shared_ptr<Image>& image) {
		std::shared_ptr<Texture2D> texture = Factory<Texture2D, GraphicsAPIs, const std::shared_ptr<Image>&>::CreateShared(Window::GetCurrentAPI(), image);
		CORI_CORE_ASSERT(texture, "Failed to create Texture2D for API: {}. Check registrations and API validity.", APIEnumToName(Window::GetCurrentAPI()));
		return texture;
	}

}