#pragma once

namespace Cori {
	enum class ErrorCode {
		eNotReady,
		eTimeout,
		eInvalidData,
		eInvalidHandle,
		eInvalidObject,
		eImmutableObject
	};

	inline constexpr std::string to_string(ErrorCode code) {
		switch (code) {
			case ErrorCode::eNotReady: return "NotReady";
			case ErrorCode::eTimeout: return "Timeout";
			case ErrorCode::eInvalidData: return "InvalidData";
			case ErrorCode::eInvalidHandle: return "eInvalidHandle";
			case ErrorCode::eInvalidObject: return "InvalidObject";
			default: return "invalid ( " + std::format( "{:x}", static_cast<uint32_t>( code ) ) + " )";
		}
	}
}