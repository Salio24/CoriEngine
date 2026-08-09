#pragma once

#define CORI_IS_EMPTY_CSTR(str) (std::strcmp(str, "") == 0)

#define CORI_CONCAT_IMPL(a, b) a##b
#define CORI_CONCAT(a, b) CORI_CONCAT_IMPL(a, b)

#ifdef DEBUG_BUILD
	#define CORI_DEBUG_BOOL true
#else
	#define CORI_DEBUG_BOOL false
#endif

#if defined(_WIN64)
	#define PLATFORM_WINDOWS
	#if defined(__MINGW64__)
		#define PLATFORM_MINGW
	#endif
#else
	#define PLATFORM_LINUX
#endif

#if defined(_MSC_VER)
	#define COMPILER_MSVC

#elif defined(__clang__)
	#if defined(PLATFORM_WINDOWS) && !defined(PLATFORM_MINGW)
		#define COMPILER_LLVM_WINDOWS
	#elif defined(PLATFORM_WINDOWS) && defined(PLATFORM_MINGW)
		#define COMPILER_CLANG_MINGW
	#else
		#define COMPILER_CLANG_LINUX
	#endif
#elif defined(__GNUC__)
	#if defined(PLATFORM_WINDOWS) && defined(PLATFORM_MINGW)
		#define COMPILER_GCC_MINGW
	#else
		#define COMPILER_GCC_LINUX
	#endif
#else
	#error "Unsupported compiler"
#endif
