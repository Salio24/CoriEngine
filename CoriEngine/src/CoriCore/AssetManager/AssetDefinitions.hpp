#pragma once
#include "ShaderDescriptor.hpp"

// asset descriptors should be declared in appropriate namespaces, for the sake of cleanness of code
// it is not enforced, and declaring it somewhere else won't brake anything, but i do not recommend doing this as
// this will quickly turn asset managing into a mess and a pain

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