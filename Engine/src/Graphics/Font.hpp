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

			class Descriptor {
			public:
				constexpr Descriptor(std::string name, std::filesystem::path fontPath, std::vector<CharsetRange> charsetRanges, const float minimalScale = 48.0f, const float miterLimit = 1.0f)
					: m_Name(std::move(name)),
					m_FontPath(std::move(fontPath)),
					m_CharsetRanges(std::move(charsetRanges)),
					m_MinimalScale(minimalScale),
					m_MiterLimit(miterLimit),
					m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
				{ }

				using AssetType = Font;

				[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

				constexpr bool operator==(const Descriptor& other) const {
					return m_RuntimeID == other.m_RuntimeID;
				}

				struct Hasher {
					std::size_t operator()(const Descriptor& descriptor) const {
						return std::hash<uint32_t>{}(descriptor.m_RuntimeID);
					}
				};

				const std::string m_Name;
				const std::filesystem::path m_FontPath;
				const std::vector<CharsetRange> m_CharsetRanges;
				const float m_MinimalScale;
				const float m_MiterLimit;

			private:
				const uint32_t m_RuntimeID{ 0 };
				inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
			};

			static std::shared_ptr<Font> Create(const std::filesystem::path& path, const std::vector<CharsetRange>& charsets, const float minimalScale = 48.0f, const float miterLimit = 1.0f);

			static std::shared_ptr<Font> Create(const Descriptor& descriptor);

			~Font();

		protected:
			friend class Renderer2D;
			FontData* GetData();

		private:
			Font(void* font, const std::vector<CharsetRange>& charsets, const std::filesystem::path& fontPath, const float minimalScale, const float miterLimit);
			FontData* m_Data{ nullptr };
		};
	}
}
