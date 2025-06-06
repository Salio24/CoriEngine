#pragma once

namespace Cori {
	namespace Profiling {
		enum class RawEventType : uint8_t {
			ScopeBegin,
			ScopeEnd
		};

		struct RawProfileEvent {
			uint64_t RawTimestamp;
			const char* Name;
			int Line;
			RawEventType Type;
		};

		struct ProfileResult {
			std::string Name;
			std::string Category;
			long long StartTimeMicros;
			long long EndTimeMicros;
			uint32_t ThreadID;
			int Line;
		};

		inline constexpr size_t MAX_PROFILE_EVENTS_PER_FRAME = 100000;

		class TimeProfiler {
		public:
			static TimeProfiler& Get();

			TimeProfiler(const TimeProfiler&) = delete;
			TimeProfiler& operator=(const TimeProfiler&) = delete;
			TimeProfiler(TimeProfiler&&) = delete;
			TimeProfiler& operator=(TimeProfiler&&) = delete;

			void RequestNextFrameProfile();
			void FrameStart();
			void FrameEnd();

			long long GetMicroseconds();

			void RecordRawEvent(RawEventType type, const char* name, int line);

		private:
			TimeProfiler();
			~TimeProfiler() = default;

			long long ConvertRawToMicroseconds(uint64_t rawTimestamp);

			void ProcessAndWriteFrameData(uint64_t frameIDBeingProcessed, uint32_t rawEventCount);
			void WriteJsonEvent(std::ostream& out, const ProfileResult& result, size_t& jsonEventCounter);

			std::array<RawProfileEvent, MAX_PROFILE_EVENTS_PER_FRAME> m_RawEvents;
			std::atomic<uint32_t> m_RawEventIndex{ 0 };

			std::ofstream m_FrameOutputStream;

			bool m_FrameCaptureActive{ false };
			bool m_ProfileNextFrameRequest{ false };

			uint64_t m_CurrentFrameID{ 0 };
			uint64_t m_InitialTimestamp{ 0 };
			uint64_t m_Frequency{ 0 };
			std::mutex m_Mutex;
		};

		class ScopedTimer {
		public:
			ScopedTimer(const char* name, int line);
			~ScopedTimer();

			ScopedTimer(const ScopedTimer&) = delete;
			ScopedTimer& operator=(const ScopedTimer&) = delete;
			ScopedTimer(ScopedTimer&&) = delete;
			ScopedTimer& operator=(ScopedTimer&&) = delete;

		private:
			const char* m_Name;
			int m_Line;
		};

	}
}

#ifdef CORI_ENABLE_PROFILING
#define CORI_PROFILE_SCOPE_LINE2(name, line) ::Cori::Profiling::ScopedTimer timer##line(name, line)
#define CORI_PROFILE_SCOPE_LINE(name, line) CORI_PROFILE_SCOPE_LINE2(name, line)
#define CORI_PROFILE_FUNCTION() CORI_PROFILE_SCOPE_LINE(__PRETTY_FUNCTION__, __LINE__)
#define CORI_PROFILE_SCOPE(name) CORI_PROFILE_SCOPE_LINE(name, __LINE__)

#define CORI_PROFILE_REQUEST_NEXT_FRAME() ::Cori::Profiling::TimeProfiler::Get().RequestNextFrameProfile()
#define CORI_PROFILER_FRAME_START()   ::Cori::Profiling::TimeProfiler::Get().FrameStart()
#define CORI_PROFILER_FRAME_END()     ::Cori::Profiling::TimeProfiler::Get().FrameEnd()
#else
#define CORI_PROFILE_FUNCTION()
#define CORI_PROFILE_SCOPE(name)
#define CORI_PROFILE_REQUEST_NEXT_FRAME()
#define CORI_PROFILER_FRAME_START()
#define CORI_PROFILER_FRAME_END()
#endif