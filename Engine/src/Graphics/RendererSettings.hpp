#pragma once
#include "Core/Console/CVar.hpp"

namespace Cori {
	namespace Graphics {
		struct RendererSettings {
			static constexpr std::string_view s_Category{ "r" };

			[[=CVar::Desc{"Enables or disables wireframe mode"}]] [[=CVar::Cheat]]
			bool Wireframe{ false };

			[[=CVar::Desc{"Sets the line width for wireframe mode"}]] [[=CVar::Cheat]]
			[[=CVar::Range{0.1, 5.0}]] [[=CVar::Archive]]
			float WireframeLineWidth{ 1.0f };

			[[=CVar::Desc{"Freezes frustum culling"}]] [[=CVar::Cheat]]
			bool FreezeCulling{ false };

			[[=CVar::Desc{"Disables frustum culling"}]] [[=CVar::Cheat]]
			bool DisableCulling{ false };

			[[=CVar::Desc{"Draws the mesh space AABB of every render object as a wireframe box"}]]
			bool DrawAABBs{ false };
		};

		struct RendererConfig {
			static constexpr std::string_view s_Category{ "rc" };
			static constexpr Core::ApplyTier s_Tier{ Core::ApplyTier::eRestart };

			[[=CVar::Desc{"Vulkan validation layers, read while the instance is being created"}]] [[=CVar::Archive]]
			bool ValidationLayers{ CORI_DEBUG_BOOL };
		};

		CORI_SETTINGS(RendererSettings, g_RendererSettings);
		CORI_SETTINGS(RendererConfig, g_RendererConfig);
	}
}
