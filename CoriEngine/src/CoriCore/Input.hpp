#pragma once
#include <CoriEngine_export.hpp>
#include "EventSystem/CoriMouseCodes.hpp"
#include "EventSystem/CoriKeycodes.hpp"



namespace Cori {
	class CORI_ENGINE_API Input {
	public:
		static bool IsKeyPressed(CoriKeycode keycode) { return s_Instance->IsKeyPressedImpl(CoriKeycode keycode) }
	protected:
		virtual bool IsKeyPressedImpl(CoriKeycode keycode) = 0;

	private:
		static Input* s_Instance;

	};


}