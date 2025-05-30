// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "CoriGraphicsAPI.hpp"
#include "../Application.hpp"
#include "OpenGL/GL_GraphicsAPI.hpp"

namespace Cori {
	std::unique_ptr<CoriGraphicsAPI> CoriGraphicsAPI::Create() {


		std::unique_ptr<CoriGraphicsAPI> gapi = Factory<CoriGraphicsAPI, GraphicsAPIs>::CreateUnique(Application::GetCurrentAPI());
		CORI_CORE_ASSERT_FATAL(gapi, "Failed to create CoriGraphicsAPI for API: {0}. Check registrations and API validity.", static_cast<int>(Application::GetCurrentAPI())); // output api as a string
		return gapi;
	}
}