#pragma once

namespace Cori {
	enum class AssetStatus : uint8_t {
		eUnspecified,
		eUnloaded,
		eLoading,
		eLoadQueued,
		eLoaded,
		eLoadFailed,
		eStreaming,
		eStreamingQueued
	};
}