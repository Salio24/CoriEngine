#pragma once
#include "CallStackDepthDef.hpp"

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
#include <type_traits>
namespace Cori {
	enum ProfilePlotFormat {
		eNumber = std::to_underlying(tracy::PlotFormatType::Number),
		eMemory = std::to_underlying(tracy::PlotFormatType::Memory),
		ePercentage = std::to_underlying(tracy::PlotFormatType::Percentage)
	};

	enum ProfileMessageSeverity {
		eTrace = std::to_underlying(tracy::MessageSeverity::Trace),   // Broadly track variable states and events in the software program.
		eDebug = std::to_underlying(tracy::MessageSeverity::Debug),   // Describes variable states and details about specific internal events in the software, that are useful for investigations.
		eInfo = std::to_underlying(tracy::MessageSeverity::Info),    // Describes normal events, which inform on the expected progress and state of your software.
		eWarning = std::to_underlying(tracy::MessageSeverity::Warning), // Describes potentially dangerous situations caused by unexpected events and states.
		eError = std::to_underlying(tracy::MessageSeverity::Error),   // Describes the occurrence of unexpected behavior. Does not interrupt the execution of the software.
		eFatal = std::to_underlying(tracy::MessageSeverity::Fatal),   // Describes a critical event that will lead to a software failure/crash.
		COUNT
	};

	namespace ProfileParts {
		inline constexpr uint64_t Mask = Core | Assets | RenderingLoop | RenderingAssets;
	}

	namespace ProfileDetail {
		inline constexpr uint64_t MessageBufferSize = 384;
		inline int32_t ClampFormatted(const int32_t written, const uint64_t capacity) {
			if (written < 0) {
				return 0;
			}

			return written >= static_cast<int32_t>(capacity) ? static_cast<int32_t>(capacity) - 1 : written;
		}

		template <typename T> requires std::is_arithmetic_v<T>
		[[nodiscard]] constexpr auto PlotValue(const T value) {
			if constexpr (std::is_floating_point_v<T>) {
				return static_cast<double>(value);
			} else {
				return static_cast<int64_t>(value);
			}
		}
	}
}

#define CORI_PROFILE_PART_ENABLED(part) ((Cori::ProfileParts::Mask & (part)) != 0)

#if CORI_PROFILER_STACK_DEPTH > 0

#define CORI_PROFILE_FUNCTION() ZoneScopedS(CORI_PROFILER_STACK_DEPTH)
#define CORI_PROFILE_FUNCTION_C(color) ZoneScopedCS(color, CORI_PROFILER_STACK_DEPTH)

#define CORI_PROFILE_FUNCTION_P(part) SuppressVarShadowWarning( ZoneNamedS(___tracy_scoped_zone, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) )
#define CORI_PROFILE_FUNCTION_CP(part, color) SuppressVarShadowWarning( ZoneNamedCS(___tracy_scoped_zone, color, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) )

#define CORI_PROFILE_SCOPE(name) ZoneScopedNS(name, CORI_PROFILER_STACK_DEPTH)
#define CORI_PROFILE_SCOPE_C(name, color) ZoneScopedNCS(name, color, CORI_PROFILER_STACK_DEPTH)

#define CORI_PROFILE_SCOPE_P(part, name) SuppressVarShadowWarning( ZoneNamedNS(___tracy_scoped_zone, name, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) )
#define CORI_PROFILE_SCOPE_CP(part, name, color) SuppressVarShadowWarning( ZoneNamedNCS(___tracy_scoped_zone, name, color, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) )

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME(name) ZoneScopedS(CORI_PROFILER_STACK_DEPTH); { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); }
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_C(name, color) ZoneScopedS(CORI_PROFILER_STACK_DEPTH); { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); ZoneColor(color); }

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_P(part, name) SuppressVarShadowWarning( ZoneNamedS(___tracy_scoped_zone, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) ); if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); }
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_CP(part, name, color) SuppressVarShadowWarning( ZoneNamedS(___tracy_scoped_zone, CORI_PROFILER_STACK_DEPTH, CORI_PROFILE_PART_ENABLED(part)) ); if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); ZoneColor(color); }

#else
#define CORI_PROFILE_FUNCTION() ZoneScoped
#define CORI_PROFILE_FUNCTION_C(color) ZoneScopedC(color)

#define CORI_PROFILE_FUNCTION_P(part) SuppressVarShadowWarning( ZoneNamed(___tracy_scoped_zone, CORI_PROFILE_PART_ENABLED(part)) )
#define CORI_PROFILE_FUNCTION_CP(part, color) SuppressVarShadowWarning( ZoneNamedC(___tracy_scoped_zone, color, CORI_PROFILE_PART_ENABLED(part)) )

#define CORI_PROFILE_SCOPE(name) ZoneScopedN(name)
#define CORI_PROFILE_SCOPE_C(name, color) ZoneScopedNC(name, color)

#define CORI_PROFILE_SCOPE_P(part, name) SuppressVarShadowWarning( ZoneNamedN(___tracy_scoped_zone, name, CORI_PROFILE_PART_ENABLED(part)) )
#define CORI_PROFILE_SCOPE_CP(part, name, color) SuppressVarShadowWarning( ZoneNamedNC(___tracy_scoped_zone, name, color, CORI_PROFILE_PART_ENABLED(part)) )

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME(name) ZoneScoped; { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); }
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_C(name, color) ZoneScoped; { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); ZoneColor(color); }

#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_P(part, name) SuppressVarShadowWarning( ZoneNamed(___tracy_scoped_zone, CORI_PROFILE_PART_ENABLED(part)) ); if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); }
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME_CP(part, name, color) SuppressVarShadowWarning( ZoneNamed(___tracy_scoped_zone, CORI_PROFILE_PART_ENABLED(part)) ); if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{name}; ZoneName(sv.data(), sv.size()); ZoneColor(color); }

#endif

#define CORI_PROFILER_FRAME_START() FrameMark

#define CORI_PROFILER_ZONE_TEXT(text) do { std::string_view sv{text}; ZoneText(sv.data(), sv.size()); } while(0)
#define CORI_PROFILER_ZONE_TEXT_F(fmt, ...) ZoneTextF(fmt, __VA_ARGS__)

#define CORI_PROFILER_ZONE_TEXT_P(part, text) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { std::string_view sv{text}; ZoneText(sv.data(), sv.size()); } } while(0)
#define CORI_PROFILER_ZONE_TEXT_FP(part, fmt, ...) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { ZoneTextF(fmt, __VA_ARGS__); } } while(0)

#define CORI_PROFILER_ZONE_VALUE(value) ZoneValue(value)
#define CORI_PROFILER_ZONE_VALUE_P(part, value) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { ZoneValue(value); } } while(0)


#define CORI_PROFILER_PLOT(name, value) do { TracyPlot(name, Cori::ProfileDetail::PlotValue(value)); } while(0)
#define CORI_PROFILER_PLOT_P(part, name, value) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { TracyPlot(name, Cori::ProfileDetail::PlotValue(value)); } } while(0)

#define CORI_PROFILER_PLOT_CONFIG(name, format, step, fill, color) do { TracyPlotConfig(name, static_cast<tracy::PlotFormatType>(format), step, fill, color); } while(0)
#define CORI_PROFILER_PLOT_CONFIG_P(part, name, format, step, fill, color) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { TracyPlotConfig(name, static_cast<tracy::PlotFormatType>(format), step, fill, color); } } while(0)

#define CORI_PROFILER_INTERNAL_FORMAT(b, fmt, ...) Cori::ProfileDetail::ClampFormatted(snprintf(b, sizeof(b), fmt, __VA_ARGS__), sizeof(b))

#define CORI_PROFILER_MSG_F(fmt, ...) do { char b[Cori::ProfileDetail::MessageBufferSize]; int n = CORI_PROFILER_INTERNAL_FORMAT(b, fmt, __VA_ARGS__); TracyLogString(tracy::MessageSeverity::Info, 0, CORI_PROFILER_STACK_DEPTH, n, b); } while(0)
#define CORI_PROFILER_MSG_CF(color, fmt, ...) do { char b[Cori::ProfileDetail::MessageBufferSize]; int n = CORI_PROFILER_INTERNAL_FORMAT(b, fmt, __VA_ARGS__); TracyLogString(tracy::MessageSeverity::Info, color, CORI_PROFILER_STACK_DEPTH, n, b); } while(0)
#define CORI_PROFILER_MSG_SCF(severity, color, fmt, ...) do { char b[Cori::ProfileDetail::MessageBufferSize]; int n = CORI_PROFILER_INTERNAL_FORMAT(b, fmt, __VA_ARGS__); TracyLogString(static_cast<tracy::MessageSeverity>(severity), color, CORI_PROFILER_STACK_DEPTH, n, b); } while(0)

#define CORI_PROFILER_MSG_CFP(part, color, fmt, ...) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { char b[Cori::ProfileDetail::MessageBufferSize]; int n = CORI_PROFILER_INTERNAL_FORMAT(b, fmt, __VA_ARGS__); TracyLogString(tracy::MessageSeverity::Info, color, CORI_PROFILER_STACK_DEPTH, n, b); } } while(0)
#define CORI_PROFILER_MSG_SCFP(part, severity, color, fmt, ...) do { if constexpr (CORI_PROFILE_PART_ENABLED(part)) { char b[Cori::ProfileDetail::MessageBufferSize]; int n = CORI_PROFILER_INTERNAL_FORMAT(b, fmt, __VA_ARGS__); TracyLogString(static_cast<tracy::MessageSeverity>(severity), color, CORI_PROFILER_STACK_DEPTH, n, b); } } while(0)

#define CORI_PROFILE_LOCKABLE(type, varname) TracyLockable(type, varname)
#define CORI_PROFILE_LOCKABLE_N(type, varname, desc) TracyLockableN(type, varname, desc)

#define CORI_PROFILE_SHARED_LOCKABLE(type, varname) TracySharedLockable(type, varname)
#define CORI_PROFILE_SHARED_LOCKABLE_N(type, varname, desc) TracySharedLockableN(type, varname, desc)

#define CORI_PROFILE_LOCKABLE_TYPE(type) LockableBase(type)
#define CORI_PROFILE_SHARED_LOCKABLE_TYPE(type) SharedLockableBase(type)

#define CORI_PROFILER_LOCK_MARK(varname) do { LockMark(varname); } while(0)

#define CORI_PROFILER_LOCK_NAME(varname, name) do { std::string_view sv{name}; LockableName(varname, sv.data(), sv.size()); } while(0)
#define CORI_PROFILER_LOCK_NAME_F(varname, fmt, ...) do { char b[Cori::ProfileDetail::MessageBufferSize]; int n = CORI_PROFILER_INTERNAL_FORMAT(b, fmt, __VA_ARGS__); LockableName(varname, b, static_cast<size_t>(n)); } while(0)

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
#define CORI_PROFILER_ZONE_VALUE_P(part, value)

#define CORI_PROFILER_PLOT(name, value)
#define CORI_PROFILER_PLOT_P(part, name, value)

#define CORI_PROFILER_PLOT_CONFIG(name, format, step, fill, color)
#define CORI_PROFILER_PLOT_CONFIG_P(part, name, format, step, fill, color)

#define CORI_PROFILER_INTERNAL_FORMAT(b, fmt, ...)

#define CORI_PROFILER_MSG_F(fmt, ...)
#define CORI_PROFILER_MSG_CF(color, fmt, ...)
#define CORI_PROFILER_MSG_SCF(severity, color, fmt, ...)

#define CORI_PROFILER_MSG_CFP(part, color, fmt, ...)
#define CORI_PROFILER_MSG_SCFP(part, severity, color, fmt, ...)

//The lockable macros must still declare a real mutex here - they are the declaration, not an
//annotation on one. Only the instrumentation drops out.
#define CORI_PROFILE_LOCKABLE(type, varname) type varname
#define CORI_PROFILE_LOCKABLE_N(type, varname, desc) type varname
#define CORI_PROFILE_SHARED_LOCKABLE(type, varname) type varname
#define CORI_PROFILE_SHARED_LOCKABLE_N(type, varname, desc) type varname

#define CORI_PROFILE_LOCKABLE_TYPE(type) type
#define CORI_PROFILE_SHARED_LOCKABLE_TYPE(type) type

#define CORI_PROFILER_LOCK_MARK(varname) do { (void)(varname); } while(0)
#define CORI_PROFILER_LOCK_NAME(varname, name) do { (void)(varname); } while(0)
#define CORI_PROFILER_LOCK_NAME_F(varname, fmt, ...) do { (void)(varname); } while(0)
#endif


namespace Cori {
	namespace ProfileColors {
		inline constexpr uint32_t Load     = 0xE0A020; // amber   - load orchestration
		inline constexpr uint32_t Worker   = 0x6048D0; // indigo  - off-thread worker task envelope
		inline constexpr uint32_t Decode   = 0xE06010; // orange  - CPU parse/decode
		inline constexpr uint32_t Register = 0x108070; // dk teal - slot registration
		inline constexpr uint32_t Finalize = 0x2080E0; // blue    - render-thread finalize
		inline constexpr uint32_t Upload   = 0x20B0C0; // cyan    - GPU streaming upload
		inline constexpr uint32_t Loaded   = 0x30D030; // green   - asset ready / visible
		inline constexpr uint32_t Missing  = 0xC040C0; // magenta - MISSING placeholder
		inline constexpr uint32_t White    = 0xB0B0B0; // grey    - WHITE placeholder
		inline constexpr uint32_t Destroy  = 0xC03030; // red     - destroy / failure
		inline constexpr uint32_t Stale    = 0xF0D820; // yellow  - stale / invalid drops
		inline constexpr uint32_t Process  = 0x607890; // slate   - per-frame pump
		inline constexpr uint32_t Refcount = 0x8A7CA0; // mauve   - ref add / remove
		inline constexpr uint32_t Bind     = 0x6090C0; // steel   - asset bind / generation
		inline constexpr uint32_t Alloc    = 0x90D020; // lime    - handle allocation
		inline constexpr uint32_t Free     = 0x703030; // maroon  - handle free
	}
}


