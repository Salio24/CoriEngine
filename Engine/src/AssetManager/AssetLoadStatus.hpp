#pragma once

namespace Cori {
	enum class AssetStatus : uint8_t {
		LOADING,
		PLACEHOLDER,
		FAILED,
		READY,
		UNSPECIFIED
	};
}