#pragma once
#include "Utility/CleanTypeName.hpp"
#include "LogTag.hpp"

#if defined(__clang__)
#define BUGTRAP __builtin_debugtrap()
#elif defined(__GNUC__)
#define BUGTRAP __builtin_trap()
#else
#include <cstdlib>
#define BUGTRAP std::abort()
#endif

namespace Cori {

	[[nodiscard]] constexpr spdlog::level::level_enum ToSpdlogLevel(const LogLevel level) noexcept {
		switch (level) {
		case LogLevel::eTrace: return spdlog::level::trace;
		case LogLevel::eDebug: return spdlog::level::debug;
		case LogLevel::eInfo:  return spdlog::level::info;
		case LogLevel::eWarm:  return spdlog::level::warn;
		case LogLevel::eError: return spdlog::level::err;
		case LogLevel::eFatal: return spdlog::level::critical;
		case LogLevel::eOff:   return spdlog::level::off;
		}
		return spdlog::level::off;
	}

	[[nodiscard]] constexpr LogLevel FromSpdlogLevel(const spdlog::level::level_enum level) noexcept {
		switch (level) {
		case spdlog::level::trace:    return LogLevel::eTrace;
		case spdlog::level::debug:    return LogLevel::eDebug;
		case spdlog::level::info:     return LogLevel::eInfo;
		case spdlog::level::warn:     return LogLevel::eWarm;
		case spdlog::level::err:      return LogLevel::eError;
		case spdlog::level::critical: return LogLevel::eFatal;
		default:                      return LogLevel::eOff;
		}
	}

	/**
	 * @brief Logger is the first thing that is initialized during startup. Does console, terminal and file logging.
	 */
	class Logger {
	public:
		enum class Channel : uint8_t {
			eCore,
			eClient
		};

		/**
		 * @brief Available logger tags in a hierarchical tree. These are used in the tagged logging calls.
		 * @details Each tag owns a runtime severity floor, see LogTag. Call sites list the whole chain from the
		 * root down, so raising the floor on any ancestor silences everything under it.
		 */
		struct Tags {
			struct Graphics {
				static inline LogTag Self{ "Graphics" };

				static inline LogTag OpenGL{ "OpenGL", &Self };

				struct Vulkan {
					static inline LogTag Self{ "Vulkan", &Graphics::Self };
					static inline LogTag Vma{ "VMA", &Self };
					static inline LogTag ValidationLayers{ "Validation Layers", &Self };
					static inline LogTag DeviceFault{ "Device Fault", &Self };
					static inline LogTag Aftermath{ "Nsight Aftermath", &Self };
					static inline LogTag ResourceTracker{ "Resource Tracker", &Self };
					static inline LogTag RenderGraph{ "Render Graph", &Self };
					static inline LogTag UploadSubsystem{ "Upload Subsystem", &Self };
					static inline LogTag VirtualBuffer{ "Virtual Buffer", &Self };
					static inline LogTag VirtualBufferAllocator{ "Virtual Buffer Allocator", &Self };
					static inline LogTag DeletionQueue{ "Deletion Queue", &Self };
					static inline LogTag StreamingLine{ "Streaming Line", &Self };
					static inline LogTag TextureManager{ "Texture Manager", &Self };
					static inline LogTag ShaderManager{ "Shader Manager", &Self };
					static inline LogTag MaterialSystem{ "Material System", &Self };
					static inline LogTag ShaderEffectManager{ "Shader Effect Manager", &Self };
					static inline LogTag MeshManager{ "Mesh Manager", &Self };
					static inline LogTag Renderer{ "Renderer", &Self };
				};

				static inline LogTag Font{ "Font", &Self };



				static inline LogTag Image{ "Image", &Self };
				static inline LogTag Camera{ "Camera", &Self };

			};

			struct Audio {
				static inline LogTag Self{ "Audio" };

				static inline LogTag Sound{ "Sound", &Self };
				static inline LogTag Track{ "Track", &Self };
				static inline LogTag Mixer{ "Mixer", &Self };
			};

			struct Core {
				static inline LogTag Self{ "Core" };

				struct Factory {
					static inline LogTag Self{ "Factory", &Core::Self };

					static inline LogTag SelfFactory{ "Self Factory", &Self };
					static inline LogTag Register{ "Register", &Self };
					static inline LogTag Shared{ "Shared", &Self };
					static inline LogTag Unique{ "Unique", &Self };
				};

				struct Glaze {
					static inline LogTag Self{ "Glaze", &Core::Self };
					static inline LogTag GlazeWithFallback{ "GlazeWithFallback", &Self };
					static inline LogTag MissingKeys{ "MissingKeys", &Self };
				};

				static inline LogTag Logger{ "Logger", &Self };
				static inline LogTag Console{ "Console", &Self };
				static inline LogTag Layer{ "Layer", &Self };
				static inline LogTag LayerStack{ "LayerStack", &Self };
				static inline LogTag GameTimer{ "GameTimer", &Self };
				static inline LogTag Window{ "Window", &Self };
				static inline LogTag ImGui{ "ImGui", &Self };
				static inline LogTag UUID{ "UUID", &Self };
				static inline LogTag SceneManager{ "Scene Manager", &Self };
				static inline LogTag AssetManager{ "Asset Manager", &Self };

			};

			struct FileSystem {
				static inline LogTag Self{ "FileSystem" };

				static inline LogTag BinaryFileManager{ "Binary File Manager", &Self };
				static inline LogTag PathManager{ "Path Manager", &Self };

			};

			struct Math {
				static inline LogTag Self{ "Math" };

				static inline LogTag Function{ "Function", &Self };
			};

			struct World {
				static inline LogTag Self{ "World" };

				struct Scene {
					static inline LogTag Self{ "Scene", &World::Self };

				};

				struct Entity {
					static inline LogTag Self{ "Entity", &World::Self };

					static inline LogTag Trigger{ "Trigger", &Self };
					static inline LogTag StateMachine{ "State Machine", &Self };
					static inline LogTag QuadRenderer{ "Quad Renderer", &Self };
					static inline LogTag QuadAnimator{ "Quad Animator", &Self };

				};

				struct Systems {
					static inline LogTag Self{ "Systems", &World::Self };

					static inline LogTag Animation{ "Animation", &Self };
				};
			};

			struct Profiler {
				static inline LogTag Self{ "Profiler" };

				static inline LogTag InstanceMetrics{ "Instance Metrics", &Self };
			};

			struct Utility {
				static inline LogTag Self{ "Utility" };

				static inline LogTag UTF{ "UTF", &Self };
			};

			// Ungrouped tags
			static inline LogTag UnusedError{ "Unused Error" };
		};


		static void Init(bool async, bool fileWrite);

		static void SetThreadName(const std::string& name);

		static bool GetStatus();

		static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

		static void SetCoreLogLevel(LogLevel level);
		static void SetClientLogLevel(LogLevel level);

		static void SampleColors();

		template<typename T>
		static auto ColoredText(const T& text, const fmt::color c, const fmt::text_style s = fmt::text_style{}) {
			return fmt::styled(text, fmt::fg(c) | s);
		}

		template<typename T>
		static auto HighlightedText(const T& text, const fmt::color c, const fmt::text_style s = fmt::text_style{}) {
			return fmt::styled(text, fmt::bg(c) | s);
		}

		static std::string_view BoolAlpha(const bool b) {
			if (b) {
				return "True";
			}
			return "False";
		}


		template<LogLevel Level, Channel TargetChannel, typename... Args>
		static void LogTagged(const std::initializer_list<LogTagRef> tags, const fmt::format_string<Args...>& fmt, Args&&... args) {
			constexpr spdlog::level::level_enum spdlogLevel = ToSpdlogLevel(Level);

			const std::shared_ptr<spdlog::logger>& logger = GetChannelLogger<TargetChannel>();

			if (!logger || !logger->should_log(spdlogLevel) || !PassesTagFilter(Level, tags)) {
				return;
			}

			fmt::memory_buffer buffer;

			for (const LogTagRef tag : tags) {
				fmt::format_to(std::back_inserter(buffer), "[{}]", tag.get().GetName());
			}

			buffer.push_back(' ');

			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));

			logger->log(spdlogLevel, spdlog::string_view_t(buffer.data(), buffer.size()));
		}

		template<LogLevel Level, Channel TargetChannel, typename... Args>
		static void Log(const fmt::format_string<Args...>& fmt, Args&&... args) {
			constexpr spdlog::level::level_enum spdlogLevel = ToSpdlogLevel(Level);

			const std::shared_ptr<spdlog::logger>& logger = GetChannelLogger<TargetChannel>();

			if (!logger || !logger->should_log(spdlogLevel)) {
				return;
			}

			fmt::memory_buffer buffer;

			fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));

			logger->log(spdlogLevel, spdlog::string_view_t(buffer.data(), buffer.size()));
		}
	protected:
		friend class ThreadNameFlagFormatter;
		friend class LogBufferSink;
		[[nodiscard]] static std::string GetThreadName(const uint64_t threadId);

	private:
		static void EnableVirtualTerminalProcessing();

		template<Channel TargetChannel>
		static const std::shared_ptr<spdlog::logger>& GetChannelLogger() {
			if constexpr (TargetChannel == Channel::eCore) {
				return s_CoreLogger;
			}
			else {
				return s_ClientLogger;
			}
		}

		static bool s_Initialized;

		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

inline const std::string CORI_SECOND_LINE_SPACING = "[" + std::string(43, '-') + "]: ";

//#define DISABLE_LOGGING

#define CORI_LOG_CORE(level, ...)        ::Cori::Logger::Log<level, ::Cori::Logger::Channel::eCore>(__VA_ARGS__)
#define CORI_LOG_CLIENT(level, ...)      ::Cori::Logger::Log<level, ::Cori::Logger::Channel::eClient>(__VA_ARGS__)
#define CORI_LOG_CORE_TAGGED(level, ...) ::Cori::Logger::LogTagged<level, ::Cori::Logger::Channel::eCore>(__VA_ARGS__)
#define CORI_LOG_CLIENT_TAGGED(level, ...) ::Cori::Logger::LogTagged<level, ::Cori::Logger::Channel::eClient>(__VA_ARGS__)

// vvv Engine Side
#if defined(DEBUG_BUILD) && !defined(DISABLE_LOGGING)

	#define CORI_CORE_TRACE(...) CORI_LOG_CORE(::Cori::LogLevel::eTrace, __VA_ARGS__)
	#define CORI_CORE_DEBUG(...) CORI_LOG_CORE(::Cori::LogLevel::eDebug, __VA_ARGS__)
	#define CORI_CORE_INFO(...)  CORI_LOG_CORE(::Cori::LogLevel::eInfo, __VA_ARGS__)

	#define CORI_CORE_TRACE_TAGGED(...) CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eTrace, __VA_ARGS__)
	#define CORI_CORE_DEBUG_TAGGED(...) CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eDebug, __VA_ARGS__)
	#define CORI_CORE_INFO_TAGGED(...)  CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eInfo, __VA_ARGS__)

	#define CORI_CORE_ASSERT(x, ...) if (!(x)) { (SPDLOG_LOGGER_CRITICAL(::Cori::Logger::GetCoreLogger(), "Assertion failed. Message: " __VA_ARGS__), ::Cori::Logger::GetCoreLogger()->critical("    Assertion: {} \n Stacktrace: \n{}", std::string(#x), std::to_string(std::stacktrace::current()))); spdlog::shutdown(); CORI_PROFILER_MSG_SCF(Cori::ProfileMessageSeverity::eFatal, 0xFFFF0000, "CORE ASSERT: %s", #x); BUGTRAP; }
	#define CORI_CORE_VERIFY(x, ...) (!(x) ? (SPDLOG_LOGGER_CRITICAL(::Cori::Logger::GetCoreLogger(), "Verify failed. Message: " __VA_ARGS__), ::Cori::Logger::GetCoreLogger()->critical("    Function: {} \n Verify: {}", __PRETTY_FUNCTION__, std::string(#x)), true) : false) //TODO: remove

#else

	#define CORI_CORE_TRACE(...)
	#define CORI_CORE_DEBUG(...)
	#define CORI_CORE_INFO(...)

	#define CORI_CORE_TRACE_TAGGED(...)
	#define CORI_CORE_DEBUG_TAGGED(...)
	#define CORI_CORE_INFO_TAGGED(...)

	#define CORI_CORE_ASSERT(x, ...)
	#define CORI_CORE_VERIFY(x, ...) (!x)

#endif

#define CORI_CORE_WARN(...)  CORI_LOG_CORE(::Cori::LogLevel::eWarm, __VA_ARGS__)
#define CORI_CORE_ERROR(...) CORI_LOG_CORE(::Cori::LogLevel::eError, __VA_ARGS__)
#define CORI_CORE_FATAL(...) CORI_LOG_CORE(::Cori::LogLevel::eFatal, __VA_ARGS__)

#define CORI_CORE_WARN_TAGGED(...)  CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eWarm, __VA_ARGS__)
#define CORI_CORE_ERROR_TAGGED(...) CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eError, __VA_ARGS__)
#define CORI_CORE_FATAL_TAGGED(...) CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eFatal, __VA_ARGS__)

#define CORI_CORE_CHECK(x, ...) (!(x) ? (SPDLOG_LOGGER_ERROR(::Cori::Logger::GetCoreLogger(), "Check failed. Message: " __VA_ARGS__), ::Cori::Logger::GetCoreLogger()->error("    Function: {} \n Check: {}", __PRETTY_FUNCTION__, std::string(#x)), true) : false) //TODO: remove
#define CORI_CORE_CHECK_EXPECTED(x) CORI_CORE_CHECK(x, "std::expected returned an error, message: {}", x.error().what()) //TODO: remove

// vvv User Side

#define CORI_ASSERT(x, ...) if (!(x)) { (SPDLOG_LOGGER_CRITICAL(::Cori::Logger::GetClientLogger(), "Assertion failed. Message: " __VA_ARGS__), ::Cori::Logger::GetClientLogger()->critical("    Assertion: {} \n Stacktrace: \n{}", std::string(#x), std::to_string(std::stacktrace::current()))); spdlog::shutdown(); CORI_PROFILER_MSG_SCF(Cori::ProfileMessageSeverity::eFatal, 0xFFFF0000, "CLIENT ASSERT: %s", #x); BUGTRAP; }
#define CORI_VERIFY(x, ...) (!(x) ? (SPDLOG_LOGGER_CRITICAL(::Cori::Logger::GetClientLogger(), "Verify failed. Message: " __VA_ARGS__), ::Cori::Logger::GetClientLogger()->critical("    Function: {} \n Verify: {}", __PRETTY_FUNCTION__, std::string(#x)), true) : false) //TODO: remove
#define CORI_CHECK(x, ...) (!(x) ? (SPDLOG_LOGGER_ERROR(::Cori::Logger::GetClientLogger(), "Check failed. Message: " __VA_ARGS__), ::Cori::Logger::GetClientLogger()->error("    Function: {} \n Check: {}", __PRETTY_FUNCTION__, std::string(#x)), true) : false) //TODO: remove
#define CORI_CHECK_EXPECTED(x) CORI_CHECK(x, "std::expected returned an error, message: {}", x.error().what()) //TODO: remove

#define CORI_TRACE(...)      CORI_LOG_CLIENT(::Cori::LogLevel::eTrace, __VA_ARGS__)
#define CORI_DEBUG(...)      CORI_LOG_CLIENT(::Cori::LogLevel::eDebug, __VA_ARGS__)
#define CORI_INFO(...)       CORI_LOG_CLIENT(::Cori::LogLevel::eInfo, __VA_ARGS__)
#define CORI_WARN(...)       CORI_LOG_CLIENT(::Cori::LogLevel::eWarm, __VA_ARGS__)
#define CORI_ERROR(...)      CORI_LOG_CLIENT(::Cori::LogLevel::eError, __VA_ARGS__)
#define CORI_FATAL(...)      CORI_LOG_CLIENT(::Cori::LogLevel::eFatal, __VA_ARGS__)

#define CORI_TRACE_TAGGED(...) CORI_LOG_CLIENT_TAGGED(::Cori::LogLevel::eTrace, __VA_ARGS__)
#define CORI_DEBUG_TAGGED(...) CORI_LOG_CLIENT_TAGGED(::Cori::LogLevel::eDebug, __VA_ARGS__)
#define CORI_INFO_TAGGED(...)  CORI_LOG_CLIENT_TAGGED(::Cori::LogLevel::eInfo, __VA_ARGS__)
#define CORI_WARN_TAGGED(...)  CORI_LOG_CLIENT_TAGGED(::Cori::LogLevel::eWarm, __VA_ARGS__)
#define CORI_ERROR_TAGGED(...) CORI_LOG_CLIENT_TAGGED(::Cori::LogLevel::eError, __VA_ARGS__)
#define CORI_FATAL_TAGGED(...) CORI_LOG_CLIENT_TAGGED(::Cori::LogLevel::eFatal, __VA_ARGS__)
