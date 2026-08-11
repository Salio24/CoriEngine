#pragma once
#include <imgui.h>
#include "Utility/ImGuiHelpers.hpp"

namespace Snowflake {
	namespace Theme {
		using Cori::Utility::Hex24ToImVec4;
		using Cori::Utility::WithAlpha;

		// https://catppuccin.com/palette
		struct Palette {
			ImVec4 rosewater;
			ImVec4 flamingo;
			ImVec4 pink;
			ImVec4 mauve;
			ImVec4 red;
			ImVec4 maroon;
			ImVec4 peach;
			ImVec4 yellow;
			ImVec4 green;
			ImVec4 teal;
			ImVec4 sky;
			ImVec4 sapphire;
			ImVec4 blue;
			ImVec4 lavender;
			ImVec4 text;
			ImVec4 subtext1;
			ImVec4 subtext0;
			ImVec4 overlay2;
			ImVec4 overlay1;
			ImVec4 overlay0;
			ImVec4 surface2;
			ImVec4 surface1;
			ImVec4 surface0;
			ImVec4 base;
			ImVec4 mantle;
			ImVec4 crust;
			bool light;
		};

		inline constexpr Palette Latte{
			.rosewater = Hex24ToImVec4(0xDC8A78),
			.flamingo = Hex24ToImVec4(0xDD7878),
			.pink = Hex24ToImVec4(0xEA76CB),
			.mauve = Hex24ToImVec4(0x8839EF),
			.red = Hex24ToImVec4(0xD20F39),
			.maroon = Hex24ToImVec4(0xE64553),
			.peach = Hex24ToImVec4(0xFE640B),
			.yellow = Hex24ToImVec4(0xDF8E1D),
			.green = Hex24ToImVec4(0x40A02B),
			.teal = Hex24ToImVec4(0x179299),
			.sky = Hex24ToImVec4(0x04A5E5),
			.sapphire = Hex24ToImVec4(0x209FB5),
			.blue = Hex24ToImVec4(0x1E66F5),
			.lavender = Hex24ToImVec4(0x7287FD),
			.text = Hex24ToImVec4(0x4C4F69),
			.subtext1 = Hex24ToImVec4(0x5C5F77),
			.subtext0 = Hex24ToImVec4(0x6C6F85),
			.overlay2 = Hex24ToImVec4(0x7C7F93),
			.overlay1 = Hex24ToImVec4(0x8C8FA1),
			.overlay0 = Hex24ToImVec4(0x9CA0B0),
			.surface2 = Hex24ToImVec4(0xACB0BE),
			.surface1 = Hex24ToImVec4(0xBCC0CC),
			.surface0 = Hex24ToImVec4(0xCCD0DA),
			.base = Hex24ToImVec4(0xEFF1F5),
			.mantle = Hex24ToImVec4(0xE6E9EF),
			.crust = Hex24ToImVec4(0xDCE0E8),
			.light = true
		};

		inline constexpr Palette Frappe{
			.rosewater = Hex24ToImVec4(0xF2D5CF),
			.flamingo = Hex24ToImVec4(0xEEBEBE),
			.pink = Hex24ToImVec4(0xF4B8E4),
			.mauve = Hex24ToImVec4(0xCA9EE6),
			.red = Hex24ToImVec4(0xE78284),
			.maroon = Hex24ToImVec4(0xEA999C),
			.peach = Hex24ToImVec4(0xEF9F76),
			.yellow = Hex24ToImVec4(0xE5C890),
			.green = Hex24ToImVec4(0xA6D189),
			.teal = Hex24ToImVec4(0x81C8BE),
			.sky = Hex24ToImVec4(0x99D1DB),
			.sapphire = Hex24ToImVec4(0x85C1DC),
			.blue = Hex24ToImVec4(0x8CAAEE),
			.lavender = Hex24ToImVec4(0xBABBF1),
			.text = Hex24ToImVec4(0xC6D0F5),
			.subtext1 = Hex24ToImVec4(0xB5BFE2),
			.subtext0 = Hex24ToImVec4(0xA5ADCE),
			.overlay2 = Hex24ToImVec4(0x949CBB),
			.overlay1 = Hex24ToImVec4(0x838BA7),
			.overlay0 = Hex24ToImVec4(0x737994),
			.surface2 = Hex24ToImVec4(0x626880),
			.surface1 = Hex24ToImVec4(0x51576D),
			.surface0 = Hex24ToImVec4(0x414559),
			.base = Hex24ToImVec4(0x303446),
			.mantle = Hex24ToImVec4(0x292C3C),
			.crust = Hex24ToImVec4(0x232634),
			.light = false
		};

		inline constexpr Palette Macchiato{
			.rosewater = Hex24ToImVec4(0xF4DBD6),
			.flamingo = Hex24ToImVec4(0xF0C6C6),
			.pink = Hex24ToImVec4(0xF5BDE6),
			.mauve = Hex24ToImVec4(0xC6A0F6),
			.red = Hex24ToImVec4(0xED8796),
			.maroon = Hex24ToImVec4(0xEE99A0),
			.peach = Hex24ToImVec4(0xF5A97F),
			.yellow = Hex24ToImVec4(0xEED49F),
			.green = Hex24ToImVec4(0xA6DA95),
			.teal = Hex24ToImVec4(0x8BD5CA),
			.sky = Hex24ToImVec4(0x91D7E3),
			.sapphire = Hex24ToImVec4(0x7DC4E4),
			.blue = Hex24ToImVec4(0x8AADF4),
			.lavender = Hex24ToImVec4(0xB7BDF8),
			.text = Hex24ToImVec4(0xCAD3F5),
			.subtext1 = Hex24ToImVec4(0xB8C0E0),
			.subtext0 = Hex24ToImVec4(0xA5ADCB),
			.overlay2 = Hex24ToImVec4(0x939AB7),
			.overlay1 = Hex24ToImVec4(0x8087A2),
			.overlay0 = Hex24ToImVec4(0x6E738D),
			.surface2 = Hex24ToImVec4(0x5B6078),
			.surface1 = Hex24ToImVec4(0x494D64),
			.surface0 = Hex24ToImVec4(0x363A4F),
			.base = Hex24ToImVec4(0x24273A),
			.mantle = Hex24ToImVec4(0x1E2030),
			.crust = Hex24ToImVec4(0x181926),
			.light = false
		};

		inline constexpr Palette Mocha{
			.rosewater = Hex24ToImVec4(0xF5E0DC),
			.flamingo = Hex24ToImVec4(0xF2CDCD),
			.pink = Hex24ToImVec4(0xF5C2E7),
			.mauve = Hex24ToImVec4(0xCBA6F7),
			.red = Hex24ToImVec4(0xF38BA8),
			.maroon = Hex24ToImVec4(0xEBA0AC),
			.peach = Hex24ToImVec4(0xFAB387),
			.yellow = Hex24ToImVec4(0xF9E2AF),
			.green = Hex24ToImVec4(0xA6E3A1),
			.teal = Hex24ToImVec4(0x94E2D5),
			.sky = Hex24ToImVec4(0x89DCEB),
			.sapphire = Hex24ToImVec4(0x74C7EC),
			.blue = Hex24ToImVec4(0x89B4FA),
			.lavender = Hex24ToImVec4(0xB4BEFE),
			.text = Hex24ToImVec4(0xCDD6F4),
			.subtext1 = Hex24ToImVec4(0xBAC2DE),
			.subtext0 = Hex24ToImVec4(0xA6ADC8),
			.overlay2 = Hex24ToImVec4(0x9399B2),
			.overlay1 = Hex24ToImVec4(0x7F849C),
			.overlay0 = Hex24ToImVec4(0x6C7086),
			.surface2 = Hex24ToImVec4(0x585B70),
			.surface1 = Hex24ToImVec4(0x45475A),
			.surface0 = Hex24ToImVec4(0x313244),
			.base = Hex24ToImVec4(0x1E1E2E),
			.mantle = Hex24ToImVec4(0x181825),
			.crust = Hex24ToImVec4(0x11111B),
			.light = false
		};

		// Not an official Catppuccin flavor: Latte's accents on a dark neutral ramp.
		inline constexpr Palette Espresso{
			.rosewater = Hex24ToImVec4(0xDC8A78),
			.flamingo = Hex24ToImVec4(0xDD7878),
			.pink = Hex24ToImVec4(0xEA76CB),
			.mauve = Hex24ToImVec4(0xA76CF3),
			.red = Hex24ToImVec4(0xF24D71),
			.maroon = Hex24ToImVec4(0xE95966),
			.peach = Hex24ToImVec4(0xFE640B),
			.yellow = Hex24ToImVec4(0xDF8E1D),
			.green = Hex24ToImVec4(0x40A02B),
			.teal = Hex24ToImVec4(0x1899A0),
			.sky = Hex24ToImVec4(0x04A5E5),
			.sapphire = Hex24ToImVec4(0x209FB5),
			.blue = Hex24ToImVec4(0x4E86F7),
			.lavender = Hex24ToImVec4(0x7287FD),
			.text = Hex24ToImVec4(0xD6D7E1),
			.subtext1 = Hex24ToImVec4(0xC0C1CE),
			.subtext0 = Hex24ToImVec4(0xABACBA),
			.overlay2 = Hex24ToImVec4(0x8A8C9E),
			.overlay1 = Hex24ToImVec4(0x73768C),
			.overlay0 = Hex24ToImVec4(0x5F6477),
			.surface2 = Hex24ToImVec4(0x454A59),
			.surface1 = Hex24ToImVec4(0x373B48),
			.surface0 = Hex24ToImVec4(0x292D38),
			.base = Hex24ToImVec4(0x171B24),
			.mantle = Hex24ToImVec4(0x12151C),
			.crust = Hex24ToImVec4(0x0D0F14),
			.light = false
		};

		// The whole editor theme is these two lines: pick a flavor, pick an accent member of it
		inline constexpr const Palette& Flavor = Espresso;
		inline constexpr ImVec4 Palette::* AccentColor = &Palette::mauve;

		inline constexpr ImVec4 Accent = Flavor.*AccentColor;
		inline constexpr ImVec4 Transparent = Hex24ToImVec4(0x000000, 0.0f);

		// Screen dimmer behind modals: darkest neutral on dark flavors, a grey wash on Latte
		inline constexpr ImVec4 Scrim = WithAlpha(Flavor.light ? Flavor.surface2 : Flavor.crust, 0.6f);

		// Accent fills carry less alpha on light flavors so labels stay readable over them
		constexpr ImVec4 AccentTint(const float alpha) {
			return WithAlpha(Accent, Flavor.light ? alpha * 0.6f : alpha);
		}
	}
}