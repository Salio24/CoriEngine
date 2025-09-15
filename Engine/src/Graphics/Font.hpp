#pragma once
#include "Profiling/Trackable.hpp"



namespace Cori {
	class AssetManager;
	namespace Graphics {
		namespace Internal {
			struct FontData;
		}

		/**
		 * @brief Font asset to be used when rendering text. Pretty expensive to create if not cached, always preload it.
		 */
		class Font : public Profiling::Trackable<Font> {
		public:
			/**
			 * @brief A UTF-32 charset range.
			 */
			struct CharsetRange {
				uint32_t m_Start;
				uint32_t m_End;
			};

			/**
			 * @brief These are predefined UTF-32 charset ranges to be used when creating a Font.
			 * @note This is of course not all charset ranges you can use, find more here: https://symbl.cc/en/unicode-table/
			 */
			struct CharsetRanges {
				static constexpr CharsetRange Latin = { 0x0020, 0x00FF };
				static constexpr CharsetRange LatinExtendedA = { 0x0100, 0x017F };
				static constexpr CharsetRange LatinExtendedB = { 0x0180, 0x024F };

				static constexpr CharsetRange Cyrillic = { 0x0400, 0x04FF };
				static constexpr CharsetRange CyrillicExtendedA = { 0x2DE0, 0x2DFF };
				static constexpr CharsetRange CyrillicExtendedB = { 0xA640, 0xA69F };
			};

			/**
			 * @brief Font Descriptor meant to be used with AssetManager only.
			 */
			class Descriptor {
			public:
				/**
				 * @brief Constructs a descriptor. It's recommended to use "inline const" when defining the Descriptor in a namespace.
				 * @param name Name to be used in AssetManager logging.
				 * @param fontPath Path to the font file.
				 * @param charsetRanges An vector with CharsetRanges to be loaded from the font.
				 * @param minimalScale Minimal glyph scale, the higher the value, the slower generation time, higher memory usage, but smoother glyphs when using high font size. Default is a good middle-ground, increase only if your glyphs looks choppy or have slight imperfections.
				 * @param miterLimit You shouldn't touch this, the default is good, but you can try to increase it in case you see some significant artifacts.
				 */
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

			/**
			 * @brief Creates a Font object.
			 * @param path Path to the font file.
			 * @param charsets An vector with CharsetRanges to be loaded from the font.
			 * @param minimalScale Minimal glyph scale, the higher the value, the slower generation time, higher memory usage, but smoother glyphs when using high font size. Default is a good middle-ground, increase only if your glyphs looks choppy or have slight imperfections.
			 * @param miterLimit You shouldn't touch this, the default is good, but you can try to increase it in case you see some significant artifacts.
			 * @return Shared pointer to the loaded Font asset.
			 */
			[[nodiscard]] static std::shared_ptr<Font> Create(const std::filesystem::path& path, const std::vector<CharsetRange>& charsets, const float minimalScale = 48.0f, const float miterLimit = 1.0f);

			~Font();

		private:
			friend AssetManager;
			friend class Renderer2D;
			[[nodiscard]] static std::shared_ptr<Font> Create(const Descriptor& descriptor);
			Internal::FontData* GetData();

			Font(void* font, const std::vector<CharsetRange>& charsets, const std::filesystem::path& fontPath, const float minimalScale, const float miterLimit);
			Internal::FontData* m_Data{ nullptr };
		};
	}
}
