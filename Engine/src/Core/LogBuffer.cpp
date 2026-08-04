#include "LogBuffer.hpp"
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/os.h>

namespace Cori {

	class LogBufferSink final : public spdlog::sinks::base_sink<spdlog::details::null_mutex> {
	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override {
			const std::string_view payload(msg.payload.data(), msg.payload.size());
			const auto [tags, remainder] = SplitTagPrefix(payload);

			std::string_view message = remainder;
			while (!message.empty() && (message.front() == ' ' || message.front() == '\t')) {
				message.remove_prefix(1);
			}

			const auto seconds = std::chrono::floor<std::chrono::seconds>(msg.time);
			const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time - seconds);
			const std::tm local = spdlog::details::os::localtime(std::chrono::system_clock::to_time_t(seconds));

			fmt::memory_buffer time;
			fmt::format_to(std::back_inserter(time), "[{:02}:{:02}:{:02}.{:03}]", local.tm_hour, local.tm_min, local.tm_sec, milliseconds.count());

			const std::string_view loggerName(msg.logger_name.data(), msg.logger_name.size());

			const LogLevel level = FromSpdlogLevel(msg.level);
			const bool coreChannel = loggerName == "ENGINE";
			const std::string_view timeView(time.data(), time.size());
			const std::string threadName = Logger::GetThreadName(msg.thread_id);

			bool continuation = false;

			while (true) {
				const size_t breakAt = message.find('\n');

				std::string_view line = message.substr(0, breakAt);
				if (!line.empty() && line.back() == '\r') {
					line.remove_suffix(1);
				}

				LogBuffer::Push(level, coreChannel, continuation, timeView, threadName, tags, line);

				if (breakAt == std::string_view::npos) {
					break;
				}

				message.remove_prefix(breakAt + 1);
				continuation = true;
			}
		}

		void flush_() override {}
	};

	void LogBuffer::Push(const LogLevel level, const bool coreChannel, const bool continuation, const std::string_view time, const std::string_view threadName, const std::string_view tags, const std::string_view message) {
		std::lock_guard lock(m_Mutex);

		LogRecord& slot = m_Records[m_NextHead % s_Capacity];

		slot.m_Sequence = m_NextHead;
		slot.m_Level = level;
		slot.m_CoreChannel = coreChannel;
		slot.m_Continuation = continuation;
		slot.m_Time = time;
		slot.m_ThreadName = threadName;
		slot.m_Tags = tags;
		slot.m_Message = message;

		++m_NextHead;

		if (m_NextHead - m_Oldest > s_Capacity) {
			m_Oldest = m_NextHead - s_Capacity;
		}
	}

	void LogBuffer::Fetch(const std::span<const uint64_t> sequences, std::vector<LogRecord>& out) {
		out.resize(sequences.size());

		std::lock_guard lock(m_Mutex);

		for (size_t i = 0; i < sequences.size(); ++i) {
			out[i] = m_Records[sequences[i] % s_Capacity];
		}
	}



	spdlog::sink_ptr LogBuffer::MakeSink() {
		return std::make_shared<LogBufferSink>();
	}
}
