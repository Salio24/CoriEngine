#include "Font.hpp"
#include "FontData.hpp"
#include <PathDefinesGenerated.hpp>

namespace Cori {
	namespace Graphics {
		std::shared_ptr<Font> Font::Create(const std::filesystem::path& path, const std::vector<CharsetRange>& charsets, const float minimalScale /*48.0f*/, const float miterLimit /*2.0f*/) {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Loading Font from: {}", path.string());
			std::shared_ptr<Font> coriFont = nullptr;
			msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
			CORI_CORE_ASSERT(ft, "Failed to initialize FreeType when loading Font");
			msdfgen::FontHandle* font = msdfgen::loadFont(ft, path.string().c_str());
			if (font) {
				coriFont.reset(new Font(static_cast<void*>(font), charsets, path, minimalScale, miterLimit));
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Loaded Font from '{}' successfully.", path.string());
				msdfgen::destroyFont(font);
			} else {
				const std::filesystem::path placeholder = FileSystem::Internal::PathDefines::GetEngineDataRoot() / "placeholders/unifont-16.0.04.otf";
				msdfgen::FontHandle* fontPlaceholder = msdfgen::loadFont(ft, placeholder.string().c_str());
				CORI_CORE_ASSERT(fontPlaceholder, "Failed to load placeholder (bundled with the engine) Font. It should've been at bin/'Build Type if any '{}'", placeholder.string());
				coriFont.reset(new Font(static_cast<void*>(fontPlaceholder), charsets, placeholder, minimalScale, miterLimit));
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Failed to load Font from '{}', loaded bundled placeholder font instead.", path.string());
				msdfgen::destroyFont(fontPlaceholder);
			}
			msdfgen::deinitializeFreetype(ft);
			return coriFont;
		}

		std::shared_ptr<Font> Font::Create(const Descriptor& descriptor) {
			return Create(descriptor.m_FontPath, descriptor.m_CharsetRanges, descriptor.m_MinimalScale, descriptor.m_MiterLimit);
		}

		Font::Font(void* font, const std::vector<CharsetRange>& charsets, const std::filesystem::path& fontPath, const float minimalScale, const float miterLimit) {
			auto font_ = static_cast<msdfgen::FontHandle*>(font);

			m_Data = new Internal::FontData();

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
			// this take time
			int32_t notPackedCount = packer.pack(m_Data->m_Glyphs.data(), m_Data->m_Glyphs.size());
			if (notPackedCount > 0) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Failed to pack '{}' glyphs.", notPackedCount);
			}

			m_Data->m_FinalScale = packer.getScale();

			std::filesystem::path target = std::filesystem::path("cache") / "fonts" / (fontPath.stem().string() + "Cache.bin");

			std::filesystem::create_directories(target.parent_path());

			size_t fontFileSize = std::filesystem::file_size(fontPath);

			static auto CheckCached = [&] -> bool {
				std::ifstream f(target, std::ios::in | std::ios::binary);

				if (!f.is_open()) {
					CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "No cache found for font '{}', generating it now.", fontPath.stem().string());
					return true;
				}

				if (!f.good()) {
					f.close();
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Failed to read cached for font '{}', cache will be generated from scratch.", fontPath.stem().string());
					return true;
				}

				f.seekg(0, std::ios::end);
				size_t fileSize = f.tellg();
				f.seekg(0, std::ios::beg);

				std::vector<unsigned char> buffer(fileSize);

				f.read(reinterpret_cast<char*>(buffer.data()), fileSize);

				f.close();

				auto widthLoaded = *reinterpret_cast<int32_t*>(&buffer[0]);
				auto heightLoaded = *reinterpret_cast<int32_t*>(&buffer[4]);
				auto finalSizeLoaded = *reinterpret_cast<double*>(&buffer[8]);
				auto miterLimitLoaded = *reinterpret_cast<float*>(&buffer[16]);
				auto cachedFontFileSize = *reinterpret_cast<size_t*>(&buffer[20]);

				if (finalSizeLoaded != m_Data->m_FinalScale) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Cache for font with name '{}' was generated with different scale, regenerating it now.", fontPath.stem().string());
					return true;
				}

				if (miterLimitLoaded != miterLimit) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Cache for font with name '{}' was generated with different miter limit, regenerating it now.", fontPath.stem().string());
					return true;
				}

				if (cachedFontFileSize != fontFileSize) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Cache for font with name '{}' was generated with the font that had different filesize, regenerating it now.", fontPath.stem().string());
					return true;
				}

				for (const auto& [m_Start, m_End] : charsets) {
					bool found = false;
					for (uint32_t i = 0; i < static_cast<uint32_t>(buffer[28]); ++i) {
						size_t startPos = 32 + i * 8;
						auto start = *reinterpret_cast<uint32_t*>(&buffer[startPos]);
						auto end = *reinterpret_cast<uint32_t*>(&buffer[startPos + 4]);

						if (start == m_Start && end == m_End) {
							found = true;
							break;
						}
					}

					if (!found) {
						CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Cache for font with name '{}' was generated with different charsets, regenerating it now.", fontPath.stem().string());
						return true;
					}
				}

				size_t assumedAtlasSize = widthLoaded * 3 * heightLoaded;
				size_t pixelsOffset = 32 + static_cast<uint32_t>(buffer[28]) * 8;
				size_t actualAtlasSize = fileSize - pixelsOffset;

				if (assumedAtlasSize != actualAtlasSize) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Cache for font with name '{}' has corrupted pixel data, regenerating it now.", fontPath.stem().string());
					return true;
				}

				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Font }, "Cache found for font '{}', msdf-atlas will be loaded from it.", fontPath.stem().string());

				void* pixels = &buffer[pixelsOffset];

				Texture::Params params2 { .m_PixelFormat = Texture::RGB888, .m_WrapMode = Texture::CLAMP_TO_EDGE, .m_Filter = Texture::LINEAR, .m_UnpackAlignment = 1, .m_HasSemiTransparency = false, };
				m_Data->m_Atlas = Texture2D::Create(pixels, widthLoaded, heightLoaded, params2);
				return false;
			};

			bool regenerate = CheckCached();

			if (regenerate) {
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
				}
				else {
					if (n > 12) {
						generator.setThreadCount(n - 4);
					} else {
						generator.setThreadCount(n / 2);
					}
				}

				generator.generate(m_Data->m_Glyphs.data(), m_Data->m_Glyphs.size());
				msdfgen::BitmapConstRef<msdf_atlas::byte, 3> storage = generator.atlasStorage();

				std::ofstream out(target, std::ios::out | std::ios::binary);

				uint32_t charsetSize = charsets.size();

				/* Font Cache binary file layout
				 * offset 0 - size 4 bytes (int32_t) - atlas width,
				 * offset 4 - size 4 bytes (int32_t) - atlas width,
				 * offset 8 - size 8 bytes (double) - finalScale,
				 * offset 16 - size 4  bytes (float) - mitterLimit,
				 * offset 20 - size 8 bytes (size_t) - size of the font file the cache was created from,
				 * offset 28 - size 4 bytes (uint32_t) - the amount of charsets the cache was created with, and also the amount of charsets present further in the file,
				 * offset 32: next there is charsets, the amount is described by the previous 4 bytes. Each charset is 8 bytes, first 4 bytes (uint32_t) - start codepoint, last 4 bytes (uint32_t) - end codepoint.
				 * the rest is msdf-atlas in uncompressed RGB888 format, without padding for Alpha channel.
				 */

				out.write(reinterpret_cast<const char*>(&storage.width), sizeof(storage.width));
				out.write(reinterpret_cast<const char*>(&storage.height), sizeof(storage.height));
				out.write(reinterpret_cast<const char*>(&m_Data->m_FinalScale), sizeof(m_Data->m_FinalScale));
				out.write(reinterpret_cast<const char*>(&miterLimit), sizeof(miterLimit));
				out.write(reinterpret_cast<const char*>(&fontFileSize), sizeof(fontFileSize));
				out.write(reinterpret_cast<const char*>(&charsetSize), sizeof(charsetSize));

				for (const auto& [m_Start, m_End] : charsets) {
					out.write(reinterpret_cast<const char*>(&m_Start), sizeof(m_Start));
					out.write(reinterpret_cast<const char*>(&m_End), sizeof(m_End));
				}

				out.write(reinterpret_cast<const char*>(storage.pixels), storage.width * 3 * storage.height);

				out.close();

				Texture::Params params{.m_PixelFormat = Texture::RGB888, .m_WrapMode = Texture::CLAMP_TO_EDGE, .m_Filter = Texture::LINEAR, .m_UnpackAlignment = 1, .m_HasSemiTransparency = false,};
				m_Data->m_Atlas = Texture2D::Create(storage.pixels, storage.width, storage.height, params);
			}
		}


		Font::~Font() {
			delete m_Data;
		}

		Internal::FontData* Font::GetData() {
			return m_Data;
		}
	}
}