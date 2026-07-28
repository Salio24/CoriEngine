#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000

#ifdef CORI_VMA_LEAK_INSTRUMENTATION
	#include "VmaLeakLog.hpp"

	#define VMA_LEAK_LOG_FORMAT(format, ...) do { int n = snprintf(nullptr, 0, format, __VA_ARGS__); std::string m(n, '\0'); snprintf(m.data(), n + 1, format, __VA_ARGS__); ::Cori::Graphics::VmaLeakLog::BumpLeakedAllocationCounter(); CORI_CORE_ERROR_TAGGED({ ::Cori::Logger::Tags::Graphics::Self, ::Cori::Logger::Tags::Graphics::Vulkan::Self, ::Cori::Logger::Tags::Graphics::Vulkan::Vma }, "{}", m);  } while (false)

	#define VMA_ASSERT_LEAK(expr) do { if (!(expr)) { ::Cori::Graphics::VmaLeakLog::BumpLeakAssertCounter(); CORI_CORE_ERROR_TAGGED({ ::Cori::Logger::Tags::Graphics::Self, ::Cori::Logger::Tags::Graphics::Vulkan::Self, ::Cori::Logger::Tags::Graphics::Vulkan::Vma }, "VMA leak assertion '{}'. Memory was still allocated when the owning block was destroyed.", #expr); } } while (false)

	#ifdef CORI_VMA_VERBOSE_ALLOCATION_LOGGING
		#define VMA_DEBUG_LOG_FORMAT(format, ...) do { int n = snprintf(nullptr, 0, format, __VA_ARGS__); std::string m(n, '\0'); snprintf(m.data(), n + 1, format, __VA_ARGS__); ::Cori::Graphics::VmaLeakLog::BumpLeakedAllocationCounter(); CORI_CORE_TRACE_TAGGED({ ::Cori::Logger::Tags::Graphics::Self, ::Cori::Logger::Tags::Graphics::Vulkan::Self, ::Cori::Logger::Tags::Graphics::Vulkan::Vma }, "{}", m);  } while (false)
	#endif
#endif

#include "vk_mem_alloc.hpp"
