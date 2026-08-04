#pragma once
#include "Core/LogBuffer.hpp"

namespace Cori {
	/**
	 * @brief The engine side console window: the captured log with level, channel and text filters, plus the
	 * category tree that drives per tag severity floors.
	 * @details Holds no records of its own. It filters each line once as it arrives, remembers only the
	 * sequence numbers that passed, and fetches the handful of records it is about to draw back out of
	 * LogBuffer each frame.
	 * \n The two filters work at different points and both are useful. The toolbar filters are applied to lines
	 * that were already captured, so they can be changed freely to look back through history. The category
	 * floors in the tag tree filter at the source, which is what actually removes cost from the logging path,
	 * but they only affect lines logged from that point on.
	 */
	class ConsolePanel {
	public:
		static constexpr const char* s_DefaultName{ "Console" };

		/**
		 * @brief Pulls whatever is new out of LogBuffer and draws the window.
		 * @param open Optional visibility flag. When it points at false the pull still happens but nothing is drawn, which keeps a hidden console current without the caller doing anything.
		 * @param name Window title, also its ImGui id, so give each console its own if for whatever reason there is more than one.
		 * @note Draw is only usable in Layer OnImGuiRender.
		 */
		void Draw(bool* open = nullptr, const char* name = s_DefaultName);

	private:
		struct TagNode {
			const LogTag* m_Tag{ nullptr };
			std::string m_Label;
			std::vector<uint32_t> m_Children;
		};

		struct VisibleEntry {
			uint64_t m_Sequence{ 0 };
			LogLevel m_Level{ LogLevel::eInfo };
			bool m_Continuation{ false };
		};

		struct TextCursor {
			uint64_t m_Sequence{ 0 };
			uint32_t m_Char{ 0 };

			[[nodiscard]] bool operator<(const TextCursor& other) const {
				return m_Sequence != other.m_Sequence ? m_Sequence < other.m_Sequence : m_Char < other.m_Char;
			}

			[[nodiscard]] bool operator==(const TextCursor& other) const {
				return m_Sequence == other.m_Sequence && m_Char == other.m_Char;
			}
		};

		void Pull();

		void ClearDisplay();

		void DrawToolbar();

		void DrawTagPane(float bodyHeight);

		void DrawTagNode(uint32_t index);

		void DrawEntries(float bodyHeight);

		void DrawPrompt();

		void DrawEntry(uint64_t row, const LogRecord& record, ImDrawList* drawList, float lineHeight, float rowWidth);

		void HandleSelectionShortcuts();

		void ClearSelection();

		[[nodiscard]] std::pair<TextCursor, TextCursor> GetSelectionRange() const;

		[[nodiscard]] static uint32_t CharIndexAt(std::string_view line, float localX);

		uint64_t BuildLine(const LogRecord& record);

		void RebuildVisible();

		void DropEvicted();

		void RetireOldestVisible();

		void PushVisible(uint64_t sequence, const LogRecord& record);

		[[nodiscard]] uint64_t GetVisibleCount() const { return m_VisibleHead - m_VisibleTail; }

		[[nodiscard]] uint64_t GetHeldCount() const { return m_Head - m_Tail; }

		[[nodiscard]] const VisibleEntry& GetVisible(const uint64_t row) const { return m_Visible[(m_VisibleTail + row) % LogBuffer::s_Capacity]; }

		[[nodiscard]] uint64_t CountSelectedRows() const;

		void RebuildTagTree();

		[[nodiscard]] bool PassesFilter(const LogRecord& record);

		void CopyToClipboard();

		[[nodiscard]] static bool LevelCombo(const char* label, LogLevel& level, const float width, const LogLevel highest);

		std::vector<VisibleEntry> m_Visible{ LogBuffer::s_Capacity };


		uint64_t m_Head{ 0 };
		uint64_t m_Tail{ 0 };

		uint64_t m_VisibleTail{ 0 };
		uint64_t m_VisibleHead{ 0 };

		std::vector<uint64_t> m_VisibleRowsThisFrame;
		std::vector<LogRecord> m_RowRecords;

		std::vector<TagNode> m_TagNodes;
		std::vector<uint32_t> m_TagRoots;

		ImGuiTextFilter m_TextFilter;
		std::string m_FilterScratch;

		TextCursor m_SelectAnchor;
		TextCursor m_SelectHead;
		bool m_HasSelection{ false };
		bool m_Selecting{ false };

		std::array<char, 512> m_InputBuffer{};

		std::string m_RowScratch;

		float m_MaxRowWidth{ 0.0f };

		std::array<uint32_t, 6> m_LevelCounts{};

		LogLevel m_MinLevel{ LogLevel::eTrace };

		bool m_ShowCore{ true };
		bool m_ShowClient{ true };
		bool m_ShowTime{ true };
		bool m_ShowThread{ true };
		bool m_ShowChannel{ true };
		bool m_ShowTags{ true };
		bool m_AutoScroll{ true };
		bool m_ShowTagPane{ false };
		bool m_FilterDirty{ true };
	};
}
