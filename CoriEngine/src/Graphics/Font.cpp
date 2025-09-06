#include "Font.hpp"
#include "FontData.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<Font> Font::Create(const std::filesystem::path& path, const std::initializer_list<CharsetRange> charsets, const float minimalScale /*48.0f*/, const float miterLimit /*2.0f*/) {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Loading Font from: {}", path.string());
			std::shared_ptr<Font> coriFont = nullptr;
			msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
			CORI_CORE_ASSERT(ft, "Failed to initialize FreeType when loading Font");
			msdfgen::FontHandle* font = msdfgen::loadFont(ft, path.c_str());
			if (font) {
				coriFont.reset(new Font(static_cast<void*>(font), charsets, minimalScale, miterLimit));
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Loaded Font from '{}' successfully.", path.string());
				msdfgen::destroyFont(font);
			} else {
				msdfgen::FontHandle* fontPlaceholder = msdfgen::loadFont(ft, "assets/engine/fonts/unifont-16.0.04.otf");
				CORI_CORE_ASSERT(fontPlaceholder, "Failed to load placeholder (bundled with the engine) Font. It should've been at bin/'Build Type if any'/assets/engine/fonts/unifont-16.0.04.otf");
				coriFont.reset(new Font(static_cast<void*>(fontPlaceholder), charsets, minimalScale, miterLimit));
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Failed to load Font from '{}', loaded bundled placeholder font instead.", path.string());
				msdfgen::destroyFont(fontPlaceholder);
			}
			msdfgen::deinitializeFreetype(ft);
			return coriFont;
		}

		Font::Font(void* font, const std::initializer_list<CharsetRange>& charsets, const float minimalScale, const float miterLimit) {
			auto font_ = static_cast<msdfgen::FontHandle*>(font);

			m_Data = new FontData();

			msdf_atlas::Charset charset;

			for (const auto& [m_Start, m_End] : charsets) {
				for (uint32_t c = m_Start; c <= m_End; ++c) {
					charset.add(c);
				}
			}

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Trying to load '{}' glyphs.", charset.size());

			int32_t loadedCount = m_Data->m_FontGeometry.loadCharset(font_, 1.0f, charset);

			if (loadedCount > 0) {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Loaded '{}' glyphs. This number can be lower than the requested glyph count '{}' due to font not having some ones.", loadedCount, charset.size());
			} else {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Failed to load charset with '{}' glyphs. loadCharset returned '{}'", charset.size(), loadedCount);
			}

			constexpr float angleThreshold = 3.0f;
			for (msdf_atlas::GlyphGeometry& glyph : m_Data->m_Glyphs) {
				glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, angleThreshold, 0);
			}

			msdf_atlas::TightAtlasPacker packer;
			packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::SQUARE);
			packer.setMinimumScale(minimalScale);
			packer.setSpacing(1);
			packer.setPixelRange(2.0);
			packer.setMiterLimit(miterLimit);
			int32_t notPackedCount = packer.pack(m_Data->m_Glyphs.data(), m_Data->m_Glyphs.size());
			if (notPackedCount > 0) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Failed to pack '{}' glyphs.", notPackedCount);
			}

			m_Data->m_FinalScale = packer.getScale();

			int32_t width = 0;
			int32_t height = 0;
			packer.getDimensions(width, height);
			msdf_atlas::ImmediateAtlasGenerator<float, 3, msdf_atlas::msdfGenerator, msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 3>> generator(width, height);
			msdf_atlas::GeneratorAttributes attributes;
			attributes.scanlinePass = true;
			generator.setAttributes(attributes);
			int32_t n = std::thread::hardware_concurrency();
			if (n == 1) {
				generator.setThreadCount(n);
			} else {
				generator.setThreadCount(n / 2);
			}
			generator.generate(m_Data->m_Glyphs.data(), m_Data->m_Glyphs.size());
			msdfgen::BitmapConstRef<msdf_atlas::byte, 3> storage = generator.atlasStorage();

			Texture::Params params { .m_PixelFormat = Texture::RGB888, .m_WrapMode = Texture::CLAMP_TO_EDGE, .m_Filter = Texture::LINEAR, .m_UnpackAlignment = 1, .m_HasSemiTransparency = false,  };
			m_Data->m_Atlas = Texture2D::Create(storage.pixels, storage.width, storage.height, params);
		}


		Font::~Font() {
			delete m_Data;
		}

		FontData* Font::GetData() {
			return m_Data;
		}
	}
}