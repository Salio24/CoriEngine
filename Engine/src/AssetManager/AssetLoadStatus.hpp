#pragma once

namespace Cori {
	enum class AssetStatus : uint8_t {
		eEmpty, //remove
		eUnloaded,
		eLoading,
		eLoadQueued,
		eLoadFailed,
		ePlaceholder, //remove
		eLoaded,
		eUnspecified
	};
}