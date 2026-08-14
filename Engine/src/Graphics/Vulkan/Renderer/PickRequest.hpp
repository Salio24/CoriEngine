#pragma once

namespace Cori {
	namespace Graphics {
		using EntityValueType = uint32_t;
		inline constexpr EntityValueType s_NullEntityID{ UINT32_MAX };

		struct PickRequest {
			uint64_t ticket{ 0 };
			float u{ 0.0f };
			float v{ 0.0f };
		};

		struct PickResult {
			uint64_t ticket{ 0 };
			EntityValueType entityID{ s_NullEntityID };
		};
	}
}
