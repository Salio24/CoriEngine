#include "TimeProfiler.hpp"
#include "SDL3/SDL_time.h"

namespace Cori {
	namespace Profiling {

		TimeProfiler::TimeProfiler() {
			m_InitialTimestamp = SDL_GetPerformanceCounter();
			m_Frequency = SDL_GetPerformanceFrequency();
			if (m_Frequency == 0) {
				CORI_CORE_ERROR("TimeProfiler: Frequency is zero. Time profiling will not work.");
			}
		}

		long long TimeProfiler::GetMicroseconds() {
			return static_cast<long long>(static_cast<double>(SDL_GetPerformanceCounter() - TimeProfiler::Get().m_InitialTimestamp) / static_cast<double>(TimeProfiler::Get().m_Frequency) * 1000000.0f);
		}

		void TimeProfiler::FrameStart() {
			m_CurrentFrameID++;
			if (m_FrameCaptureRequest) {
				m_FrameCaptureRequest = false;
				m_FrameCaptureActive = true;
			}

			if (m_FrameCaptureActive) {
				if (m_FrameCaptureStepFirstActive) {
					m_VectorSize = 0;

				}
				if (m_FrameCaptureStepSecondActive) {
				}
				if (m_FrameCaptureStepThirdActive) {

					// maybe add a current time in a filename?
					m_OutputStream.open("frame_" + std::to_string(m_CurrentFrameID) + "_timeTrace.json");
					if (m_OutputStream) {
						m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
						m_OutputStream.flush();
					}
					else {
						CORI_CORE_ERROR("TimeProfiler::FrameStart: Output stream invalid.");
					}
				}

			}

		}

		void TimeProfiler::FrameEnd() {
			if (m_FrameCaptureActive) {
				if (m_FrameCaptureStepThirdActive) {
					m_FrameCaptureStepThirdActive = false;

					for (const auto& result : m_ProfileResults) {
						WriteProfile(result);
					}

					if (m_OutputStream) {
						m_OutputStream << "]}";
						m_OutputStream.flush();
						m_OutputStream.close();
						CORI_CORE_INFO("TimeProfiler: Profile written for frame: {0}", m_CurrentFrameID);
					}
					else {
						CORI_CORE_ERROR("TimeProfiler::FrameEnd: Output stream invalid.");
					}

					m_ProfileResults.clear();
					m_ProfileResults.shrink_to_fit();
					m_CurrentEventCount = 0;
					m_FrameCaptureActive = false;
				}

				if (m_FrameCaptureStepSecondActive) {
					m_FrameCaptureStepSecondActive = false;
					m_FrameCaptureStepThirdActive = true;
				}

				if (m_FrameCaptureStepFirstActive) {
					m_FrameCaptureStepFirstActive = false;
					m_FrameCaptureStepSecondActive = true;
					m_ProfileResults.clear();
					m_ProfileResults.reserve(m_VectorSize);
				}
			}
		}

		void TimeProfiler::WriteProfile(const ProfileResult& result) {
			if (!m_OutputStream) { CORI_CORE_ERROR("Profiler::WriteEvent: Output stream invalid."); return; }

			if (m_CurrentEventCount > 0) m_OutputStream << ",";
			m_OutputStream << std::endl;
			std::string name = result.Name; std::replace(name.begin(), name.end(), '"', '\''); 
			long long durationMicros = result.EndTimeMicros - result.StartTimeMicros;
			m_OutputStream << "{\"cat\":\"" << result.Category << "\",";
			m_OutputStream << "\"dur\":" << durationMicros << ",";
			m_OutputStream << "\"name\":\"" << name << " (L" << result.Line << ")\",";
			m_OutputStream << "\"ph\":\"X\",";
			m_OutputStream << "\"pid\":0" << ",";
			m_OutputStream << "\"ts\":" << result.StartTimeMicros << ",";
			m_OutputStream << "\"tid\":" << result.ThreadID << "}";


			m_OutputStream.flush();
			m_CurrentEventCount++;

		}

		void ScopedTimer::Stop() {
			static TimeProfiler& self = TimeProfiler::Get();

			if (self.m_FrameCaptureStepSecondActive) {
				long long endTimeMicros = TimeProfiler::GetMicroseconds();
				uint32_t processID = 0;

				self.m_ProfileResults.push_back({
					m_Name,
					"function",
					m_StartTimeMicros,
					endTimeMicros,
					static_cast<uint32_t>(SDL_GetThreadID(NULL)),
					m_Line
					});
				m_Stopped = true;
			}
		}

	}
}