// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Texture.hpp"
#include "Core/Application.hpp"
#include "OpenGL/GL_Texture.hpp"

namespace Cori {
	std::shared_ptr<Texture2D> Texture2D::Create(const Texture2DDescriptor& descriptor) {


		std::shared_ptr<Texture2D> texture = Factory<Texture2D, GraphicsAPIs, const std::string&>::CreateShared(Application::GetCurrentAPI(), (std::string)descriptor.imagePath);
		CORI_CORE_ASSERT_FATAL(texture, "Failed to create Texture2D for API: {0}. Check registrations and API validity.", static_cast<int>(Application::GetCurrentAPI())); // output api as a string
		return texture;
	}

}