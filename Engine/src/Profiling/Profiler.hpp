#pragma once

#ifdef CORI_ENABLE_PROFILING
#include <tracy/Tracy.hpp>
#define CORI_PROFILE_FUNCTION() ZoneScopedS(35)
#define CORI_PROFILE_SCOPE(name) ZoneScopedN(name)
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME(name) ZoneScoped; std::string_view sv{name}; tracy::SetZoneName(sv.data(), sv.size())
#define CORI_PROFILER_FRAME_START() FrameMark
#else
#define CORI_PROFILE_FUNCTION()
#define CORI_PROFILE_SCOPE(name)
#define CORI_PROFILE_SCOPE_DYNAMIC_NAME(name)
#define CORI_PROFILER_FRAME_START()
#endif


