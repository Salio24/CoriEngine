#pragma once

namespace Cori {
	enum class AssetStatus : uint8_t {
		eEmpty,
		eUnloaded,
		eLoading,
		eLoadQueued,
		eLoadFailed,
		ePlaceholder,
		eLoaded,
		eUnspecified
	};
}