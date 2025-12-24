#ifndef MACROS_H
#define MACROS_H

#define CORI_PLACEHOLDERS(x) PLACEHOLDER_SELECT(x)
#define PLACEHOLDER_SELECT(x) PLACEHOLDER_IMPL_##x

#define PLACEHOLDER_IMPL_1 std::placeholders::_1
#define PLACEHOLDER_IMPL_2 std::placeholders::_2
#define PLACEHOLDER_IMPL_3 std::placeholders::_3
#define PLACEHOLDER_IMPL_4 std::placeholders::_4
#define PLACEHOLDER_IMPL_5 std::placeholders::_5
#define PLACEHOLDER_IMPL_6 std::placeholders::_6
#define PLACEHOLDER_IMPL_7 std::placeholders::_7
#define PLACEHOLDER_IMPL_8 std::placeholders::_8
#define PLACEHOLDER_IMPL_9 std::placeholders::_9
#define PLACEHOLDER_IMPL_10 std::placeholders::_10
#define PLACEHOLDER_IMPL_11 std::placeholders::_11
#define PLACEHOLDER_IMPL_12 std::placeholders::_12
#define PLACEHOLDER_IMPL_13 std::placeholders::_13
#define PLACEHOLDER_IMPL_14 std::placeholders::_14
#define PLACEHOLDER_IMPL_15 std::placeholders::_15
#define PLACEHOLDER_IMPL_16 std::placeholders::_16
#define PLACEHOLDER_IMPL_17 std::placeholders::_17
#define PLACEHOLDER_IMPL_18 std::placeholders::_18
#define PLACEHOLDER_IMPL_19 std::placeholders::_19

#define CORI_BIND_EVENT_FN(x, ...) std::bind(&x, this __VA_OPT__(,) __VA_ARGS__)

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

#endif
