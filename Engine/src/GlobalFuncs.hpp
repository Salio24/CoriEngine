#pragma once
#include "Core/Logger.hpp"
#include  "Profiling/Profiler.hpp"

namespace Cori {
	static void SetThreadName(const std::string& name) {
		Logger::SetThreadName(name);
		#ifdef CORI_ENABLE_PROFILING
			tracy::SetThreadName(name.c_str());
		#endif
	}
}