#pragma once

namespace Cori {
	namespace ProfileParts {
		inline constexpr uint64_t Core = 1ull << 0;
		inline constexpr uint64_t Assets = 1ull << 1;
		inline constexpr uint64_t RenderingLoop = 1ull << 2;
		inline constexpr uint64_t RenderingAssets = 1ull << 3;
	}
}

#ifdef CORI_ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#include <utility>
namespace Cori {
	enum ProfileMessageSeverity {
		Trace = std::to_underlying(tracy::MessageSeverity::Trace),   // Broadly track variable states and events in the software program.
		Debug = std::to_underlying(tracy::MessageSeverity::Debug),   // Describes variable states and details about specific internal events in the software, that are useful for investigations.
		Info = std::to_underlying(tracy::MessageSeverity::Info),    // Describes normal events, which inform on the expected progress and state of your software.
		Warning = std::to_underlying(tracy::MessageSeverity::Warning), // Describes potentially dangerous situations caused by unexpected events and states.
		Error = std::to_underlying(tracy::MessageSeverity::Error),   // Describes the occurrence of unexpected behavior. Does not interrupt the execution of the software.
		Fatal = std::to_underlying(tracy::MessageSeverity::Fatal),   // Describes a critical event that will lead to a software failure/crash.
		COUNT
	};

	namespace ProfileParts {
		inline constexpr uint64_t Mask = Core | Assets | RenderingLoop;
	}
}


#define CORI_PROFILER_STACK_DEPTH 35
#define CORI_PROFILE_PART_ENABLED(part) ((Cori::ProfileParts::Mask & (part)) != 0)

#define CORI_PROFILE_FUNCTION() ZoneScopedS(CORI_PROFILER_STACK_DEPTH)
#define CORI_PROFILE_FUNCTION_C(color) ZoneScopedCS(color, CORI_PROFILER_STACK_DEPTH)

#define CORI_PROFILE_FUNCTION_P(part) SuppressVarShadowWarning( ZoneNamedS(___tracy_scoped_zone, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) )
#define CORI_PROFILE_FUNCTION_CP(part, color) SuppressVarShadowWarning( ZoneNamedCS(___tracy_scoped_zone, color, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) )

#define CORI_PROFILE_SCOPE(name) ZoneScopedN(name)
#define CORI_PROFILE_SCOPE_C(name, color) ZoneScopedNC(name, color)

#define CORI_PROFILE_SCOPE_P(part, name) SuppressVarShadowWarning( ZoneNamedN(___tracy_scoped_zone, name, CORI_PROFILE_PART_ENABLED(part)) )
#define CORI_PROFILE_SCOPE_CP(part, name, color) SuppressVarShadowWarning( ZoneNamedNC(___tracy_scoped_zone, name, color, CORI_PROFILE_PART_ENABLED(part)) )

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME(name) ZoneScoped; std::string_view sv{name}; ZoneName(sv.data(), sv.size())
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_C(name, color) ZoneScoped; std::string_view sv{name}; ZoneName(sv.data(), sv.size()); ZoneColor(color)

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_P(part, name) SuppressVarShadowWarning( ZoneNamed(___tracy_scoped_zone, CORI_PROFILE_PART_ENABLED(part)) ); if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); }
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_CP(part, name, color) SuppressVarShadowWarning( ZoneNamed(___tracy_scoped_zone, CORI_PROFILE_PART_ENABLED(part)) ); if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); ZoneColor(color); }

#define CORI_PROFILER_FRAME_START() FrameMark

#define CORI_PROFILER_ZONE_TEXT(text) std::string_view sv{text}; ZoneText(sv.data(), sv.size())
#define CORI_PROFILER_ZONE_TEXT_F(fmt, ...) ZoneTextF(fmt, __VA_ARGS__)

#define CORI_PROFILER_ZONE_TEXT_P(part, text) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{text}; ZoneText(sv.data(), sv.size()); } } while(0)
#define CORI_PROFILER_ZONE_TEXT_FP(part, fmt, ...) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { ZoneTextF(fmt, __VA_ARGS__); } } while(0)

#define CORI_PROFILER_ZONE_VALUE(value) ZoneValue(value)
#define CORI_PROFILER_ZONE_VALUE_P(part, value) if constexpr (CORI_PROFILE_PART_ENABLED(part)) { ZoneValue(value); }


#define CORI_PROFILER_MSG_CF(color, fmt, ...) do { char b[256]; int n = snprintf(b, sizeof b, fmt, __VA_ARGS__); TracyLogString(tracy::MessageSeverity::Info, color, CORI_PROFILER_STACK_DEPTH, n, b); } while(0)
#define CORI_PROFILER_MSG_SCF(severity, color, fmt, ...) do { char b[256]; int n = snprintf(b, sizeof b, fmt, __VA_ARGS__); TracyLogString(static_cast<tracy::MessageSeverity>(severity), color, CORI_PROFILER_STACK_DEPTH, n, b); } while(0)

#define CORI_PROFILER_MSG_CFP(part, color, fmt, ...) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { char b[256]; int n = snprintf(b, sizeof b, fmt, __VA_ARGS__); TracyLogString(tracy::MessageSeverity::Info, color, CORI_PROFILER_STACK_DEPTH, n, b); } } while(0)
#define CORI_PROFILER_MSG_SCFP(part, severity, color, fmt, ...) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { char b[256]; int n = snprintf(b, sizeof b, fmt, __VA_ARGS__); TracyLogString(static_cast<tracy::MessageSeverity>(severity), color, CORI_PROFILER_STACK_DEPTH, n, b); } } while(0)
#else
#define CORI_PROFILE_PART_ENABLED(part) false

#define CORI_PROFILE_FUNCTION()
#define CORI_PROFILE_FUNCTION_C(color)

#define CORI_PROFILE_FUNCTION_P(part)
#define CORI_PROFILE_FUNCTION_CP(part, color)

#define CORI_PROFILE_SCOPE(name)
#define CORI_PROFILE_SCOPE_C(name, color)

#define CORI_PROFILE_SCOPE_P(part, name)
#define CORI_PROFILE_SCOPE_CP(part, name, color)

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME(name)
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_C(name, color)

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_P(part, name)
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_CP(part, name, color)

#define CORI_PROFILER_FRAME_START()

#define CORI_PROFILER_ZONE_TEXT(text)
#define CORI_PROFILER_ZONE_TEXT_F(fmt, ...)

#define CORI_PROFILER_ZONE_TEXT_P(part, text)
#define CORI_PROFILER_ZONE_TEXT_FP(part, fmt, ...)

#define CORI_PROFILER_ZONE_VALUE(value)

#define CORI_PROFILER_MSG_CF(color, fmt, ...)
#define CORI_PROFILER_MSG_SCF(severity, color, fmt, ...)

#define CORI_PROFILER_MSG_CFP(part, color, fmt, ...)
#define CORI_PROFILER_MSG_SCFP(part, severity, color, fmt, ...)
#endif


namespace Cori {
	namespace ProfileColors {
		inline constexpr uint32_t Load     = 0xE0A020; // amber   - load orchestration
		inline constexpr uint32_t Decode   = 0xE06010; // orange  - CPU parse/decode
		inline constexpr uint32_t Register = 0x20A0A0; // teal    - slot registration
		inline constexpr uint32_t Finalize = 0x2080E0; // blue    - render-thread finalize
		inline constexpr uint32_t Upload   = 0x20B0C0; // cyan    - GPU streaming upload
		inline constexpr uint32_t Loaded   = 0x30D030; // green   - asset ready / visible
		inline constexpr uint32_t Missing  = 0xC040C0; // magenta - MISSING placeholder
		inline constexpr uint32_t White    = 0xB0B0B0; // grey    - WHITE placeholder
		inline constexpr uint32_t Destroy  = 0xC03030; // red     - destroy / failure
		inline constexpr uint32_t Stale    = 0xE08020; // orange  - stale / invalid drops
		inline constexpr uint32_t Process  = 0x607890; // slate   - per-frame pump
		inline constexpr uint32_t Refcount = 0xD0C020; // gold    - ref add / remove
		inline constexpr uint32_t Bind     = 0x6090C0; // steel   - asset bind / generation
		inline constexpr uint32_t Alloc    = 0x40D060; // green   - handle allocation
		inline constexpr uint32_t Free     = 0xB03030; // dk red  - handle free
	}
}


