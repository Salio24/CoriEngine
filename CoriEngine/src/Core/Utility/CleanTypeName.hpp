#ifndef CLEANTYPENAME_HPP
#define CLEANTYPENAME_HPP

#if defined(PLATFORM_LINUX) || defined(PLATFORM_MINGW)
#ifndef DEMANGLE_BUFFER_SIZE
#define DEMANGLE_BUFFER_SIZE 128
#endif
#include <cxxabi.h>

namespace Cori {
	namespace internal {
		inline const char* Demangle(const char* mangledName, char* buffer, size_t size) {
				int status = 0;

				char* demangledName = abi::__cxa_demangle(mangledName, buffer, &size, &status);

				if (status == 0) {
					return demangledName;
				}

				return mangledName;
		}
	}
}


#define CORI_DEMANGLE(name) \
	([](const char* mangled) -> const char* { \
		thread_local char buffer[DEMANGLE_BUFFER_SIZE]; \
		return Cori::internal::Demangle(mangled, buffer, sizeof(buffer)); \
	})(name)

#define CORI_CLEAN_TYPE_NAME(tn) CORI_DEMANGLE(typeid(tn).name())

#else

#define CORI_DEMANGLE(name) name

#define CORI_CLEAN_TYPE_NAME(tn) typeid(tn).name()

#endif

#endif
