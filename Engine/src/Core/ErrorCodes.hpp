#pragma once
#include <glaze/glaze.hpp>

namespace Cori {
	enum class ErrorCode {
		eNotReady,
		eTimeout,
		eInvalidData,
		eInvalidHandle,
		eUninitializedAssetRef,
		eInvalidObject,
		eObjectDoesNotExist,
		eObjectAlreadyExists,
		eImmutableObject,
		eFailedToOpenFile,
		eParseFailure,
		eCreationFailure,
		eRedundantCall
	};

	inline constexpr std::string_view to_string(ErrorCode code) {
		//switch (code) {
		//	case ErrorCode::eNotReady: return "NotReady";
		//	case ErrorCode::eTimeout: return "Timeout";
		//	case ErrorCode::eInvalidData: return "InvalidData";
		//	case ErrorCode::eInvalidHandle: return "eInvalidHandle";
		//	case ErrorCode::eInvalidObject: return "InvalidObject";
		//	default: return "invalid ( " + std::format( "{:x}", static_cast<uint32_t>( code ) ) + " )";
		//}

		return glz::enum_to_string<ErrorCode>(code);
	}
}