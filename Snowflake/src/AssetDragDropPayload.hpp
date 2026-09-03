#pragma once
#include "Core/AssetManager/AssetManager2.hpp"

namespace Snowflake {
	struct AssetDragDropPayload {
		static constexpr const char* s_PayloadType{ "CORI_ASSET" };

		Cori::Core::AssetID id{ 0 };
		uint32_t vectorKey{ UINT32_MAX };
		uint64_t typeHash{ UINT64_MAX };

		[[nodiscard]] bool IsSet() const {
			return id != 0;
		}

		[[nodiscard]] bool Is(const uint64_t wantedTypeHash) const {
			return IsSet() && typeHash == wantedTypeHash;
		}
	};
}
