#pragma once
#include <tracy/Tracy.hpp>


namespace Cori {
	namespace Profiling {

	}
}

#ifdef CORI_ENABLE_PROFILING
#define CORI_PROFILE_FUNCTION() ZoneScopedS(35)
#define CORI_PROFILE_SCOPE(name) ZoneScopedN(name)
#define CORI_PROFILER_FRAME_START() FrameMark
#else
#define CORI_PROFILE_FUNCTION()
#define CORI_PROFILE_SCOPE(name)
#define CORI_PROFILER_FRAME_START()
#endif


