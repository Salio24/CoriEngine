#pragma once

namespace Cori {
	namespace Core {
		enum class AssetType : uint8_t {
			ePrimary,
			eSecondary,
			eUndefined
		};

		enum class AssetDeletionPolicy : uint8_t {
			eRefCounted,
			eKeepAlive
		};
	}
}


//FIXME: move asset load status enum here later