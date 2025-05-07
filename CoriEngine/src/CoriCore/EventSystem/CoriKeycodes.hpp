#pragma once
#include <string_view>
#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 512
#include <magic_enum/magic_enum.hpp>
#include "../Logger.hpp"

namespace Cori {
	typedef enum CoriKeycode {

		// this is just an adapted sdl scancodes for cori API

		// may use stringification of macros instead of magic_enum
		// like here virtual const char* GetName() const override { return #type; }
		// return #type

		CORI_KEY_UNKNOWN = 0,

		/**
		*  \name Usage page 0x07
		*
		*  These values are from usage page 0x07 (USB keyboard page).
		*/
		/* @{ */

		CORI_KEY_A = 4,
		CORI_KEY_B = 5,
		CORI_KEY_C = 6,
		CORI_KEY_D = 7,
		CORI_KEY_E = 8,
		CORI_KEY_F = 9,
		CORI_KEY_G = 10,
		CORI_KEY_H = 11,
		CORI_KEY_I = 12,
		CORI_KEY_J = 13,
		CORI_KEY_K = 14,
		CORI_KEY_L = 15,
		CORI_KEY_M = 16,
		CORI_KEY_N = 17,
		CORI_KEY_O = 18,
		CORI_KEY_P = 19,
		CORI_KEY_Q = 20,
		CORI_KEY_R = 21,
		CORI_KEY_S = 22,
		CORI_KEY_T = 23,
		CORI_KEY_U = 24,
		CORI_KEY_V = 25,
		CORI_KEY_W = 26,
		CORI_KEY_X = 27,
		CORI_KEY_Y = 28,
		CORI_KEY_Z = 29,

		CORI_KEY_1 = 30,
		CORI_KEY_2 = 31,
		CORI_KEY_3 = 32,
		CORI_KEY_4 = 33,
		CORI_KEY_5 = 34,
		CORI_KEY_6 = 35,
		CORI_KEY_7 = 36,
		CORI_KEY_8 = 37,
		CORI_KEY_9 = 38,
		CORI_KEY_0 = 39,

		CORI_KEY_RETURN = 40,
		CORI_KEY_ESCAPE = 41,
		CORI_KEY_BACKSPACE = 42,
		CORI_KEY_TAB = 43,
		CORI_KEY_SPACE = 44,

		CORI_KEY_MINUS = 45,
		CORI_KEY_EQUALS = 46,
		CORI_KEY_LEFTBRACKET = 47,
		CORI_KEY_RIGHTBRACKET = 48,
		CORI_KEY_BACKSLASH = 49, /**< Located at the lower left of the return
									*   key on ISO keyboards and at the right end
									*   of the QWERTY row on ANSI keyboards.
									*   Produces REVERSE SOLIDUS (backslash) and
									*   VERTICAL LINE in a US layout, REVERSE
									*   SOLIDUS and VERTICAL LINE in a UK Mac
									*   layout, NUMBER SIGN and TILDE in a UK
									*   Windows layout, DOLLAR SIGN and POUND SIGN
									*   in a Swiss German layout, NUMBER SIGN and
									*   APOSTROPHE in a German layout, GRAVE
									*   ACCENT and POUND SIGN in a French Mac
									*   layout, and ASTERISK and MICRO SIGN in a
									*   French Windows layout.
									*/
		CORI_KEY_NONUSHASH = 50, /**< ISO USB keyboards actually use this code
									*   instead of 49 for the same key, but all
									*   OSes I've seen treat the two codes
									*   identically. So, as an implementor, unless
									*   your keyboard generates both of those
									*   codes and your OS treats them differently,
									*   you should generate CORI_KEY_BACKSLASH
									*   instead of this code. As a user, you
									*   should not rely on this code because SDL
									*   will never generate it with most (all?)
									*   keyboards.
									*/
		CORI_KEY_SEMICOLON = 51,
		CORI_KEY_APOSTROPHE = 52,
		CORI_KEY_GRAVE = 53, /**< Located in the top left corner (on both ANSI
								*   and ISO keyboards). Produces GRAVE ACCENT and
								*   TILDE in a US Windows layout and in US and UK
								*   Mac layouts on ANSI keyboards, GRAVE ACCENT
								*   and NOT SIGN in a UK Windows layout, SECTION
								*   SIGN and PLUS-MINUS SIGN in US and UK Mac
								*   layouts on ISO keyboards, SECTION SIGN and
								*   DEGREE SIGN in a Swiss German layout (Mac:
								*   only on ISO keyboards), CIRCUMFLEX ACCENT and
								*   DEGREE SIGN in a German layout (Mac: only on
								*   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
								*   French Windows layout, COMMERCIAL AT and
								*   NUMBER SIGN in a French Mac layout on ISO
								*   keyboards, and LESS-THAN SIGN and GREATER-THAN
								*   SIGN in a Swiss German, German, or French Mac
								*   layout on ANSI keyboards.
								*/
		CORI_KEY_COMMA = 54,
		CORI_KEY_PERIOD = 55,
		CORI_KEY_SLASH = 56,

		CORI_KEY_CAPSLOCK = 57,

		CORI_KEY_F1 = 58,
		CORI_KEY_F2 = 59,
		CORI_KEY_F3 = 60,
		CORI_KEY_F4 = 61,
		CORI_KEY_F5 = 62,
		CORI_KEY_F6 = 63,
		CORI_KEY_F7 = 64,
		CORI_KEY_F8 = 65,
		CORI_KEY_F9 = 66,
		CORI_KEY_F10 = 67,
		CORI_KEY_F11 = 68,
		CORI_KEY_F12 = 69,

		CORI_KEY_PRINTSCREEN = 70,
		CORI_KEY_SCROLLLOCK = 71,
		CORI_KEY_PAUSE = 72,
		CORI_KEY_INSERT = 73, /**< insert on PC, help on some Mac keyboards (but
									does send code 73, not 117) */
		CORI_KEY_HOME = 74,
		CORI_KEY_PAGEUP = 75,
		CORI_KEY_DELETE = 76,
		CORI_KEY_END = 77,
		CORI_KEY_PAGEDOWN = 78,
		CORI_KEY_RIGHT = 79,
		CORI_KEY_LEFT = 80,
		CORI_KEY_DOWN = 81,
		CORI_KEY_UP = 82,

		CORI_KEY_NUMLOCKCLEAR = 83, /**< num lock on PC, clear on Mac keyboards
										*/
		CORI_KEY_KP_DIVIDE = 84,
		CORI_KEY_KP_MULTIPLY = 85,
		CORI_KEY_KP_MINUS = 86,
		CORI_KEY_KP_PLUS = 87,
		CORI_KEY_KP_ENTER = 88,
		CORI_KEY_KP_1 = 89,
		CORI_KEY_KP_2 = 90,
		CORI_KEY_KP_3 = 91,
		CORI_KEY_KP_4 = 92,
		CORI_KEY_KP_5 = 93,
		CORI_KEY_KP_6 = 94,
		CORI_KEY_KP_7 = 95,
		CORI_KEY_KP_8 = 96,
		CORI_KEY_KP_9 = 97,
		CORI_KEY_KP_0 = 98,
		CORI_KEY_KP_PERIOD = 99,

		CORI_KEY_NONUSBACKSLASH = 100, /**< This is the additional key that ISO
											*   keyboards have over ANSI ones,
											*   located between left shift and Y.
											*   Produces GRAVE ACCENT and TILDE in a
											*   US or UK Mac layout, REVERSE SOLIDUS
											*   (backslash) and VERTICAL LINE in a
											*   US or UK Windows layout, and
											*   LESS-THAN SIGN and GREATER-THAN SIGN
											*   in a Swiss German, German, or French
											*   layout. */
		CORI_KEY_APPLICATION = 101, /**< windows contextual menu, compose */
		CORI_KEY_POWER = 102, /**< The USB document says this is a status flag,
								*   not a physical key - but some Mac keyboards
								*   do have a power key. */
		CORI_KEY_KP_EQUALS = 103,
		CORI_KEY_F13 = 104,
		CORI_KEY_F14 = 105,
		CORI_KEY_F15 = 106,
		CORI_KEY_F16 = 107,
		CORI_KEY_F17 = 108,
		CORI_KEY_F18 = 109,
		CORI_KEY_F19 = 110,
		CORI_KEY_F20 = 111,
		CORI_KEY_F21 = 112,
		CORI_KEY_F22 = 113,
		CORI_KEY_F23 = 114,
		CORI_KEY_F24 = 115,
		CORI_KEY_EXECUTE = 116,
		CORI_KEY_HELP = 117,    /**< AL Integrated Help Center */
		CORI_KEY_MENU = 118,    /**< Menu (show menu) */
		CORI_KEY_SELECT = 119,
		CORI_KEY_STOP = 120,    /**< AC Stop */
		CORI_KEY_AGAIN = 121,   /**< AC Redo/Repeat */
		CORI_KEY_UNDO = 122,    /**< AC Undo */
		CORI_KEY_CUT = 123,     /**< AC Cut */
		CORI_KEY_COPY = 124,    /**< AC Copy */
		CORI_KEY_PASTE = 125,   /**< AC Paste */
		CORI_KEY_FIND = 126,    /**< AC Find */
		CORI_KEY_MUTE = 127,
		CORI_KEY_VOLUMEUP = 128,
		CORI_KEY_VOLUMEDOWN = 129,
		/* not sure whether there's a reason to enable these */
		/*     CORI_KEY_LOCKINGCAPSLOCK = 130,  */
		/*     CORI_KEY_LOCKINGNUMLOCK = 131, */
		/*     CORI_KEY_LOCKINGSCROLLLOCK = 132, */
		CORI_KEY_KP_COMMA = 133,
		CORI_KEY_KP_EQUALSAS400 = 134,

		CORI_KEY_INTERNATIONAL1 = 135, /**< used on Asian keyboards, see
												footnotes in USB doc */
		CORI_KEY_INTERNATIONAL2 = 136,
		CORI_KEY_INTERNATIONAL3 = 137, /**< Yen */
		CORI_KEY_INTERNATIONAL4 = 138,
		CORI_KEY_INTERNATIONAL5 = 139,
		CORI_KEY_INTERNATIONAL6 = 140,
		CORI_KEY_INTERNATIONAL7 = 141,
		CORI_KEY_INTERNATIONAL8 = 142,
		CORI_KEY_INTERNATIONAL9 = 143,
		CORI_KEY_LANG1 = 144, /**< Hangul/English toggle */
		CORI_KEY_LANG2 = 145, /**< Hanja conversion */
		CORI_KEY_LANG3 = 146, /**< Katakana */
		CORI_KEY_LANG4 = 147, /**< Hiragana */
		CORI_KEY_LANG5 = 148, /**< Zenkaku/Hankaku */
		CORI_KEY_LANG6 = 149, /**< reserved */
		CORI_KEY_LANG7 = 150, /**< reserved */
		CORI_KEY_LANG8 = 151, /**< reserved */
		CORI_KEY_LANG9 = 152, /**< reserved */

		CORI_KEY_ALTERASE = 153,    /**< Erase-Eaze */
		CORI_KEY_SYSREQ = 154,
		CORI_KEY_CANCEL = 155,      /**< AC Cancel */
		CORI_KEY_CLEAR = 156,
		CORI_KEY_PRIOR = 157,
		CORI_KEY_RETURN2 = 158,
		CORI_KEY_SEPARATOR = 159,
		CORI_KEY_OUT = 160,
		CORI_KEY_OPER = 161,
		CORI_KEY_CLEARAGAIN = 162,
		CORI_KEY_CRSEL = 163,
		CORI_KEY_EXSEL = 164,

		CORI_KEY_KP_00 = 176,
		CORI_KEY_KP_000 = 177,
		CORI_KEY_THOUSANDSSEPARATOR = 178,
		CORI_KEY_DECIMALSEPARATOR = 179,
		CORI_KEY_CURRENCYUNIT = 180,
		CORI_KEY_CURRENCYSUBUNIT = 181,
		CORI_KEY_KP_LEFTPAREN = 182,
		CORI_KEY_KP_RIGHTPAREN = 183,
		CORI_KEY_KP_LEFTBRACE = 184,
		CORI_KEY_KP_RIGHTBRACE = 185,
		CORI_KEY_KP_TAB = 186,
		CORI_KEY_KP_BACKSPACE = 187,
		CORI_KEY_KP_A = 188,
		CORI_KEY_KP_B = 189,
		CORI_KEY_KP_C = 190,
		CORI_KEY_KP_D = 191,
		CORI_KEY_KP_E = 192,
		CORI_KEY_KP_F = 193,
		CORI_KEY_KP_XOR = 194,
		CORI_KEY_KP_POWER = 195,
		CORI_KEY_KP_PERCENT = 196,
		CORI_KEY_KP_LESS = 197,
		CORI_KEY_KP_GREATER = 198,
		CORI_KEY_KP_AMPERSAND = 199,
		CORI_KEY_KP_DBLAMPERSAND = 200,
		CORI_KEY_KP_VERTICALBAR = 201,
		CORI_KEY_KP_DBLVERTICALBAR = 202,
		CORI_KEY_KP_COLON = 203,
		CORI_KEY_KP_HASH = 204,
		CORI_KEY_KP_SPACE = 205,
		CORI_KEY_KP_AT = 206,
		CORI_KEY_KP_EXCLAM = 207,
		CORI_KEY_KP_MEMSTORE = 208,
		CORI_KEY_KP_MEMRECALL = 209,
		CORI_KEY_KP_MEMCLEAR = 210,
		CORI_KEY_KP_MEMADD = 211,
		CORI_KEY_KP_MEMSUBTRACT = 212,
		CORI_KEY_KP_MEMMULTIPLY = 213,
		CORI_KEY_KP_MEMDIVIDE = 214,
		CORI_KEY_KP_PLUSMINUS = 215,
		CORI_KEY_KP_CLEAR = 216,
		CORI_KEY_KP_CLEARENTRY = 217,
		CORI_KEY_KP_BINARY = 218,
		CORI_KEY_KP_OCTAL = 219,
		CORI_KEY_KP_DECIMAL = 220,
		CORI_KEY_KP_HEXADECIMAL = 221,

		CORI_KEY_LCTRL = 224,
		CORI_KEY_LSHIFT = 225,
		CORI_KEY_LALT = 226, /**< alt, option */
		CORI_KEY_LGUI = 227, /**< windows, command (apple), meta */
		CORI_KEY_RCTRL = 228,
		CORI_KEY_RSHIFT = 229,
		CORI_KEY_RALT = 230, /**< alt gr, option */
		CORI_KEY_RGUI = 231, /**< windows, command (apple), meta */

		CORI_KEY_MODE = 257,    /**< I'm not sure if this is really not covered
									*   by any of the above, but since there's a
									*   special SDL_KMOD_MODE for it I'm adding it here
									*/

									/* @} *//* Usage page 0x07 */

									/**
									*  \name Usage page 0x0C
									*
									*  These values are mapped from usage page 0x0C (USB consumer page).
									*
									*  There are way more keys in the spec than we can represent in the
									*  current scancode range, so pick the ones that commonly come up in
									*  real world usage.
									*/
									/* @{ */

		CORI_KEY_SLEEP = 258,                   /**< Sleep */
		CORI_KEY_WAKE = 259,                    /**< Wake */

		CORI_KEY_CHANNEL_INCREMENT = 260,       /**< Channel Increment */
		CORI_KEY_CHANNEL_DECREMENT = 261,       /**< Channel Decrement */

		CORI_KEY_MEDIA_PLAY = 262,          /**< Play */
		CORI_KEY_MEDIA_PAUSE = 263,         /**< Pause */
		CORI_KEY_MEDIA_RECORD = 264,        /**< Record */
		CORI_KEY_MEDIA_FAST_FORWARD = 265,  /**< Fast Forward */
		CORI_KEY_MEDIA_REWIND = 266,        /**< Rewind */
		CORI_KEY_MEDIA_NEXT_TRACK = 267,    /**< Next Track */
		CORI_KEY_MEDIA_PREVIOUS_TRACK = 268, /**< Previous Track */
		CORI_KEY_MEDIA_STOP = 269,          /**< Stop */
		CORI_KEY_MEDIA_EJECT = 270,         /**< Eject */
		CORI_KEY_MEDIA_PLAY_PAUSE = 271,    /**< Play / Pause */
		CORI_KEY_MEDIA_SELECT = 272,        /* Media Select */

		CORI_KEY_AC_NEW = 273,              /**< AC New */
		CORI_KEY_AC_OPEN = 274,             /**< AC Open */
		CORI_KEY_AC_CLOSE = 275,            /**< AC Close */
		CORI_KEY_AC_EXIT = 276,             /**< AC Exit */
		CORI_KEY_AC_SAVE = 277,             /**< AC Save */
		CORI_KEY_AC_PRINT = 278,            /**< AC Print */
		CORI_KEY_AC_PROPERTIES = 279,       /**< AC Properties */

		CORI_KEY_AC_SEARCH = 280,           /**< AC Search */
		CORI_KEY_AC_HOME = 281,             /**< AC Home */
		CORI_KEY_AC_BACK = 282,             /**< AC Back */
		CORI_KEY_AC_FORWARD = 283,          /**< AC Forward */
		CORI_KEY_AC_STOP = 284,             /**< AC Stop */
		CORI_KEY_AC_REFRESH = 285,          /**< AC Refresh */
		CORI_KEY_AC_BOOKMARKS = 286,        /**< AC Bookmarks */

		/* @} *//* Usage page 0x0C */


		/**
		*  \name Mobile keys
		*
		*  These are values that are often used on mobile phones.
		*/
		/* @{ */

		CORI_KEY_SOFTLEFT = 287, /**< Usually situated below the display on phones and
										used as a multi-function feature key for selecting
										a software defined function shown on the bottom left
										of the display. */
		CORI_KEY_SOFTRIGHT = 288, /**< Usually situated below the display on phones and
										used as a multi-function feature key for selecting
										a software defined function shown on the bottom right
										of the display. */
		CORI_KEY_CALL = 289, /**< Used for accepting phone calls. */
		CORI_KEY_ENDCALL = 290, /**< Used for rejecting phone calls. */

		/* @} *//* Mobile keys */

		/* Add any other keys here. */

		CORI_KEY_RESERVED = 400,    /**< 400-500 reserved for dynamic keycodes */

		CORI_KEY_COUNT = 512 /**< not a key, just marks the number of scancodes for array bounds */

	} CoriKeycode;

	static std::string_view CoriGetKeyName(CoriKeycode code) {
		std::string_view name = magic_enum::enum_name(code);
		constexpr std::string_view prefix = "CORI_";
		if (name.starts_with(prefix)) {
			name.remove_prefix(prefix.size());
		}
		return name;
	}
}