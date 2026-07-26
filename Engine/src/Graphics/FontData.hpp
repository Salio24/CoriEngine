#pragma once
//legacy from old 2d renderer, need to rewire
#if 0
#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include "Texture.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			struct FontData {
				FontData() : m_FontGeometry(&m_Glyphs) {}
				std::shared_ptr<Texture2D> m_Atlas;
				std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
				msdf_atlas::FontGeometry m_FontGeometry;
				double m_FinalScale{};
			};
		}
	}
}
#endif