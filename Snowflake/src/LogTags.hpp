#pragma once
#include "Core/LogTag.hpp"

namespace Snowflake {
	struct Tags {
		struct Snowflake {
			static inline Cori::LogTag Self{ "Snowflake" };

			static inline Cori::LogTag ContentBrowser{ "Content Browser", &Self };

			static inline Cori::LogTag ThumbnailCache{ "Thumbnail Cache", &Self, Cori::LogLevel::eInfo };
		};
	};
}
