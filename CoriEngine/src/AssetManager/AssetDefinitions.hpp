#pragma once
#include "ShaderDescriptor.hpp"

// asset descriptors should be declared in appropriate namespaces, for the sake of cleanness of code
// it is not enforced, and declaring it somewhere else won't brake anything, but i do not recommend doing this as
// this will quickly turn asset managing into a mess and a pain

namespace Cori {
	namespace Shaders {
	}

	namespace Texture2Ds {
		inline const Cori::Texture2DDescriptor Placeholder{
			"Placeholder Texture",
			"assets/engine/textures/missing_texture32.png"
		};
	}

	namespace Images {

	}
}