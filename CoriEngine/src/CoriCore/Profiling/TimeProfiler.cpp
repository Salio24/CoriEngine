#include "TimeProfiler.hpp"

namespace Cori {
	namespace Profiling {

		TimeProfiler& TimeProfiler::Get() {
			static TimeProfiler instance;
			return instance;
		}

		TimeProfiler::TimeProfiler() {
			m_InitialTimestamp = SDL_GetPerformanceCounter();
			m_Frequency = SDL_GetPerformanceFrequency();
			if (m_Frequency == 0) {
				CORI_CORE_ERROR("TimeProfiler: Frequency is zero. Time profiling will not work.");
			}
			else {
				CORI_CORE_TRACE("TimeProfiler: SDL Timers initialized for profiling. Frequency: {0}", m_Frequency);
			}
		}

		long long TimeProfiler::GetMicroseconds() {
			TimeProfiler& self = TimeProfiler::Get();
			return static_cast<long long>(static_cast<double>(SDL_GetPerformanceCounter() - self.m_InitialTimestamp) / static_cast<double>(self.m_Frequency) * 1000000.0f);
		}

		long long TimeProfiler::ConvertRawToMicroseconds(uint64_t rawTimestamp) {
			if (m_Frequency == 0) return 0;
			double seconds = static_cast<double>(rawTimestamp - m_InitialTimestamp) / static_cast<double>(m_Frequency);
			return static_cast<long long>(seconds * 1000000.0);
		}

		void TimeProfiler::RequestNextFrameProfile() {
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_ProfileNextFrameRequest = true;
			CORI_CORE_INFO("TimeProfiler: Next frame profiling requested.");
		}

		void TimeProfiler::FrameStart() {
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_CurrentFrameID++;

			if (m_ProfileNextFrameRequest) {

				m_FrameOutputStream.open("frame_" + std::to_string(m_CurrentFrameID) + "_timeTrace.json");
				if (!m_FrameOutputStream) {
					CORI_CORE_ERROR("TimeProfiler: Could not create timeTrace file for frame: {0}", m_CurrentFrameID);
					m_FrameCaptureActive = false;
				}
				else {
					m_FrameOutputStream << "{\"otherData\": {},\"traceEvents\":[";
					m_RawEventIndex.store(0, std::memory_order_relaxed);
					m_FrameCaptureActive = true;
					CORI_CORE_INFO("TimeProfiler: Frame profiling started for frame: {0}", m_CurrentFrameID);
				}
				m_ProfileNextFrameRequest = false;
			}
		}

		void TimeProfiler::FrameEnd() {
			if (m_FrameCaptureActive) {
				uint32_t rawEventCountToProcess;
				{
					std::lock_guard<std::mutex> lock(m_Mutex);
					if (!m_FrameCaptureActive) return;
					m_FrameCaptureActive = false;
					rawEventCountToProcess = m_RawEventIndex.load(std::memory_order_relaxed);
				}

				CORI_CORE_INFO("TimeProfiler: Raw event capture ended for Frame: {0}. Processing: {1} scopes", m_CurrentFrameID, (rawEventCountToProcess / 2.0f));
				ProcessAndWriteFrameData(m_CurrentFrameID, rawEventCountToProcess);
			}
		}

		void TimeProfiler::ProcessAndWriteFrameData(uint64_t frameIDBeingProcessed, uint32_t rawEventCount) {
			if (!m_FrameOutputStream) {
				CORI_CORE_ERROR("TimeProfiler: Output stream for frame {0} not open in ProcessAndWriteFrameData.", frameIDBeingProcessed);
				return;
			}

			size_t jsonEventCounter = 0;

			std::vector<const RawProfileEvent*> eventStack;
			eventStack.reserve(1024);

			for (uint32_t i = 0; i < rawEventCount; ++i) {
				const RawProfileEvent& currentEvent = m_RawEvents[i];

				if (currentEvent.Type == RawEventType::ScopeBegin) {
					eventStack.push_back(&currentEvent);
				}
				else if (currentEvent.Type == RawEventType::ScopeEnd) {
					const RawProfileEvent* beginEventPtr = eventStack.back();
					eventStack.pop_back();

					if (beginEventPtr->Name == currentEvent.Name) {
						ProfileResult result;
						result.Name = beginEventPtr->Name;
						result.Category = "function/scope"; // add categories, later
						result.StartTimeMicros = ConvertRawToMicroseconds(beginEventPtr->RawTimestamp);
						result.EndTimeMicros = ConvertRawToMicroseconds(currentEvent.RawTimestamp);
						result.ThreadID = static_cast<uint32_t>(SDL_GetThreadID(NULL));
						result.Line = beginEventPtr->Line;

						WriteJsonEvent(m_FrameOutputStream, result, jsonEventCounter);
					}
				}
			}

			if (!eventStack.empty()) {
				CORI_CORE_WARN("TimeProfiler: {0} unclosed scopes at end of processing for Frame {1}!", eventStack.size(), frameIDBeingProcessed);
				for (const auto* unclosed : eventStack) {
					CORI_CORE_WARN("    Unclosed: {0} (L{1})", unclosed->Name, unclosed->Line);
				}
			}

			m_FrameOutputStream << std::endl << "]}" << std::endl;
			m_FrameOutputStream.flush();
			m_FrameOutputStream.close();
			
		}

		void TimeProfiler::WriteJsonEvent(std::ostream& out, const ProfileResult& result, size_t& jsonEventCounter) {
			if (!out) { return; }

			if (jsonEventCounter > 0) {
				out << ",";
			}
			out << std::endl;

			std::string name = result.Name;
			size_t pos = 0;
			while ((pos = name.find('"', pos)) != std::string::npos) {
				name.replace(pos, 1, "\\\"");
				pos += 2;
			}
			pos = 0;
			while ((pos = name.find('\\', pos)) != std::string::npos) {
				if (pos + 1 >= name.length() || name[pos + 1] != '"') {
					name.replace(pos, 1, "\\\\");
					pos += 2;
				}
				else {
					pos += 1;
				}
			}


			long long durationMicros = result.EndTimeMicros - result.StartTimeMicros;
			if (durationMicros < 0) durationMicros = 0;

			out << "{";
			out << "\"cat\":\"" << result.Category << "\",";
			out << "\"dur\":" << durationMicros << ",";
			out << "\"name\":\"" << name << " (L" << result.Line << ")\",";
			out << "\"ph\":\"X\",";
			out << "\"pid\":0,";
			out << "\"ts\":" << result.StartTimeMicros << ",";
			out << "\"tid\":" << result.ThreadID;
			out << "}";

			jsonEventCounter++;
		}

		void TimeProfiler::RecordRawEvent(RawEventType type, const char* name, int line) {
			if (!m_FrameCaptureActive) return;

			uint32_t index = m_RawEventIndex.fetch_add(1, std::memory_order_acquire);
			if (index < MAX_PROFILE_EVENTS_PER_FRAME) {
				RawProfileEvent& event = m_RawEvents[index];
				event.RawTimestamp = SDL_GetPerformanceCounter();
				event.Name = name;
				event.Line = line;
				event.Type = type;
			}
			else {
				static std::atomic<bool> overflow_logged_this_frame{ false };
				if (!overflow_logged_this_frame.exchange(true, std::memory_order_relaxed)) {
					CORI_CORE_ERROR("TimeProfiler: Trying to profile more than {0} scopes/function calls, are you sure you need that much? Preventing buffer overflow, some data will be lost. Frame: {1}", MAX_PROFILE_EVENTS_PER_FRAME, m_CurrentFrameID);
				}
			}
		}

		ScopedTimer::ScopedTimer(const char* name, int line)
			: m_Name(name), m_Line(line) {
			TimeProfiler::Get().RecordRawEvent(RawEventType::ScopeBegin, m_Name, m_Line);
		}

		ScopedTimer::~ScopedTimer() {
			TimeProfiler::Get().RecordRawEvent(RawEventType::ScopeEnd, m_Name, m_Line);
		}

	}
}