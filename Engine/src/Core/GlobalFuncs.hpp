#pragma once
#include "Logger.hpp"
#include  "Profiling/Profiler.hpp"

namespace Cori {
	static void SetThreadName(const std::string& name) {
		#ifdef DEBUG_BUILD
		Logger::SetThreadName(name);
			#ifdef CORI_ENABLE_PROFILING
				tracy::SetThreadName(name.c_str());
			#endif
		#endif
	}
}