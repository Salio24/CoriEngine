#pragma once
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {

		struct FontData;

		class Font : public Profiling::Trackable<Font> {
		public:
			struct CharsetRange {
				uint32_t m_Start;
				uint32_t m_End;
			};

			// all (probably) UTF-8 ranges: https://symbl.cc/en/unicode-table/
			struct CharsetRanges {
				static constexpr CharsetRange Latin = { 0x0020, 0x00FF };
				static constexpr CharsetRange LatinExtendedA = { 0x0100, 0x017F };
				static constexpr CharsetRange LatinExtendedB = { 0x0180, 0x024F };

				static constexpr CharsetRange Cyrillic = { 0x0400, 0x04FF };
				static constexpr CharsetRange CyrillicExtendedA = { 0x2DE0, 0x2DFF };
				static constexpr CharsetRange CyrillicExtendedB = { 0xA640, 0xA69F };
			};

			static std::shared_ptr<Font> Create(const std::filesystem::path& path, const std::initializer_list<CharsetRange> charsets, const float minimalScale = 48.0f, const float miterLimit = 2.0f);
			~Font();

		protected:
			friend class Renderer2D;
			FontData* GetData();

		private:
			Font(void* font, const std::initializer_list<CharsetRange>& charsets, const float minimalScale, const float miterLimit);
			FontData* m_Data{ nullptr };
		};
	}
}
