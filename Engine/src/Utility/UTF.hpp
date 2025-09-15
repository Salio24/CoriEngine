#pragma once
#include <utf8.h>

namespace Cori {
	namespace Utility {
		/**
		 * @brief Converts a UTF-8 variable length encoded string to a UTF-32 fixed length encoded string.
		 * @param view View to the UTF-8 variable length encoded string.
		 * @return UTF-32 fixed length encoded string.
		 */
		inline std::u32string Utf8ToUtf32(const std::string_view& view) {
			std::u32string dest;

			if (!view.empty()) {
				try {
					dest.reserve(utf8::distance(view.begin(), view.end()));
				} catch (const utf8::invalid_utf8& e) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Utility::Self, Logger::Tags::Utility::UTF }, "Failed to convert to UTF-8 to UTF-32, Error: {}", e.what());
				}

				try {
					utf8::utf8to32(view.begin(), view.end(), std::back_inserter(dest));
				} catch (const utf8::invalid_utf8& e) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Utility::Self, Logger::Tags::Utility::UTF }, "Failed to convert to UTF-8 to UTF-32, Error: {}", e.what());
					dest.clear();
				}
			}

			return dest;
		}

		/**
		 * @brief Converts a UTF-8 variable length encoded string to a UTF-32 fixed length encoded string.
		 * @param string UTF-8 variable length encoded string.
		 * @return UTF-32 fixed length encoded string.
		 */
		inline std::u32string Utf8ToUtf32(const std::string& string) {
			return Utf8ToUtf32(std::string_view(string));
		}
	}
}