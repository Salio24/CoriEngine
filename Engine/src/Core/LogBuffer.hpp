#pragma once
#include "LogTag.hpp"

namespace Cori {
	class LogBufferSink;
}

namespace Cori {
	[[nodiscard]] constexpr std::pair<std::string_view, std::string_view> SplitTagPrefix(const std::string_view payload) noexcept {
		uint64_t tagEnd = 0;
		while (tagEnd < payload.size() && payload[tagEnd] == '[') {
			const uint64_t close = payload.find(']', tagEnd);
			if (close == std::string_view::npos) {
				break;
			}
			tagEnd = close + 1;
		}
		return { payload.substr(0, tagEnd), payload.substr(tagEnd) };
	}

	struct LogRecord {
		uint64_t m_Sequence{ 0 };
		LogLevel m_Level{ LogLevel::eInfo };
		bool m_CoreChannel{ true };
		bool m_Continuation{ false };
		std::string m_Time;
		std::string m_ThreadName;
		std::string m_Tags;
		std::string m_Message;
	};

	class LogBuffer {
	public:
		static constexpr uint64_t s_Capacity{ 8192 };
	protected:
		template<typename F>
		static uint64_t Consume(uint64_t& inOutHead, uint64_t& outOldest, F&& visitor) {
			std::lock_guard lock(m_Mutex);

			outOldest = m_Oldest;

			const uint64_t from = std::max(inOutHead, m_Oldest);

			for (uint64_t sequence = from; sequence < m_NextHead; ++sequence) {
				visitor(sequence, std::as_const(m_Records[sequence % s_Capacity]));
			}

			inOutHead = m_NextHead;
			return m_NextHead - from;
		}

		static void Fetch(std::span<const uint64_t> sequences, std::vector<LogRecord>& out);

		friend Logger;
		friend LogBufferSink;
		friend class ConsolePanel;
		[[nodiscard]] static spdlog::sink_ptr MakeSink();
		static void Push(LogLevel level, bool coreChannel, bool continuation, std::string_view time, std::string_view threadName, std::string_view tags, std::string_view message);

	private:
		inline static CORI_PROFILE_LOCKABLE_N(std::mutex, m_Mutex, "LogBuffer");

		inline static std::vector<LogRecord> m_Records{ s_Capacity };
		inline static uint64_t m_NextHead{ 0 };
		inline static uint64_t m_Oldest{ 0 };
	};
}
