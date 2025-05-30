#pragma once

namespace Cori {
	namespace Profiling {
		struct ProfileResult {
			std::string Name;
			std::string Category;
			long long StartTimeMicros;
			long long EndTimeMicros;
			uint32_t ThreadID;
			int Line;
		};

		class TimeProfiler {
		public:

			TimeProfiler();

			static TimeProfiler& Get() {
				static TimeProfiler instance;
				return instance;
			}

			static long long GetMicroseconds();


			void FrameStart();

			void FrameEnd();

			void WriteProfile(const ProfileResult& result);

			void RequestNextFrameProfile() {
				m_FrameCaptureRequest = true;
				m_FrameCaptureStepFirstActive = true;
			}

			bool m_FrameCaptureActive{ false };

			bool m_FrameCaptureStepFirstActive{ false };
			bool m_FrameCaptureStepSecondActive{ false };
			bool m_FrameCaptureStepThirdActive{ false };	

			std::vector<ProfileResult> m_ProfileResults;
			uint64_t m_VectorSize{ 0 };

		private:
			uint64_t m_InitialTimestamp{ 0 };
			uint64_t m_Frequency{ 0 };
			uint64_t m_CurrentFrameID{ 0 };
			uint64_t m_CurrentEventCount{ 0 };

			std::ofstream m_OutputStream;

			bool m_FrameCaptureRequest{ false };
		};

		class ScopedTimer {
		public:
			ScopedTimer(const char* name, int line)
				: m_Name(name), m_Line(line), m_Stopped(false) {
				static TimeProfiler& self = TimeProfiler::Get();

				if (self.m_FrameCaptureActive) {
					if (self.m_FrameCaptureStepFirstActive) {
						self.m_VectorSize++;
					}
					if (self.m_FrameCaptureStepSecondActive) {
						m_StartTimeMicros = TimeProfiler::GetMicroseconds();
					}
				}
			}

			~ScopedTimer() {
				if (!m_Stopped) {
					Stop();
				}
			}

			void Stop();

		private:

			const char* m_Name;
			int m_Line;
			long long m_StartTimeMicros;
			bool m_Stopped;
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