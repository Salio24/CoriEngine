#pragma once
#include "Core/Application.hpp"

namespace Cori {
	namespace Utility {
		template<typename T>
		float ScaleUIUnit(const T v) {
			return static_cast<float>(v) * Core::Application::GetWindow().GetDisplayScale();
		}
	}
}
