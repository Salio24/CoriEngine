#pragma once
#include "ShaderDescriptor.hpp"

namespace Cori {
	namespace Shaders {
		inline const ShaderProgramDescriptor FlatColorQuad{
			"Flat Color Quad Shader",
			"assets/engine/shaders/flatColorQuadVert.glsl",
			"assets/engine/shaders/flatColorQuadFrag.glsl"
		};

		inline const ShaderProgramDescriptor TexturedQuad{
			"Textured Quad Shader",
			"assets/engine/shaders/texturedQuadVert.glsl",
			"assets/engine/shaders/texturedQuadFrag.glsl"
		};
	}

	namespace Texture2Ds {

	}

	namespace Images {

	}
}