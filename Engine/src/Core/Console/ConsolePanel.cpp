#include "ConsolePanel.hpp"
#include "Utility/ImGuiHelpers.hpp"
namespace {
	ImVec4 LevelMsgColor(const Cori::LogLevel level) {
		switch (level) {
		case Cori::LogLevel::eTrace: return Cori::Utility::Hex24ToImVec4(0xcde9ea);
		case Cori::LogLevel::eDebug: return Cori::Utility::Hex24ToImVec4(0x3e62cf);
		case Cori::LogLevel::eInfo:  return Cori::Utility::Hex24ToImVec4(0xabfc2f);
		case Cori::LogLevel::eWarm:  return Cori::Utility::Hex24ToImVec4(0xf6a003);
		case Cori::LogLevel::eError: return Cori::Utility::Hex24ToImVec4(0xe27de3);
		case Cori::LogLevel::eFatal: return Cori::Utility::Hex24ToImVec4(0xea030a);
		default: return {1.00f, 1.00f, 1.00f, 1.00f};
		}
	}

	ImVec4 LevelPrefixColor(const Cori::LogLevel level) {
		switch (level) {
		case Cori::LogLevel::eTrace: return Cori::Utility::Hex24ToImVec4(0xaa98af);
		case Cori::LogLevel::eDebug: return Cori::Utility::Hex24ToImVec4(0x00fdfd);
		case Cori::LogLevel::eInfo:  return Cori::Utility::Hex24ToImVec4(0x0bc710);
		case Cori::LogLevel::eWarm:  return Cori::Utility::Hex24ToImVec4(0xeeee05);
		case Cori::LogLevel::eError: return Cori::Utility::Hex24ToImVec4(0xed586e);
		case Cori::LogLevel::eFatal: return Cori::Utility::Hex24ToImVec4(0xff0065);
		default: return {1.00f, 1.00f, 1.00f, 1.00f};
		}
	}
}

namespace Cori {

	void ConsolePanel::Pull() {
		if (m_FilterDirty) {
			return;
		}

		uint64_t bufferTail = 0;

		LogBuffer::Consume(m_Head, bufferTail, [this](const uint64_t sequence, const LogRecord& record) {
			if (sequence >= m_Tail) {
				PushVisible(sequence, record);
			}
		});

		m_Tail = std::max(m_Tail, bufferTail);

		if (m_HasSelection) {
			const bool anchorEvicted = m_SelectAnchor.m_Sequence < m_Tail;
			const bool headEvicted = m_SelectHead.m_Sequence < m_Tail;

			if (anchorEvicted && headEvicted) {
				ClearSelection();
			}
			else if (anchorEvicted) {
				m_SelectAnchor = TextCursor{ .m_Sequence = m_Tail, .m_Char = 0 };
			}
			else if (headEvicted) {
				m_SelectHead = TextCursor{ .m_Sequence = m_Tail, .m_Char = 0 };
			}
		}

		DropEvicted();
	}

	void ConsolePanel::RetireOldestVisible() {
		const VisibleEntry& entry = GetVisible(0);
		const auto levelIndex = static_cast<uint64_t>(entry.m_Level);

		if (!entry.m_Continuation && levelIndex < m_LevelCounts.size() && m_LevelCounts[levelIndex] > 0) {
			--m_LevelCounts[levelIndex];
		}

		++m_VisibleTail;
	}

	void ConsolePanel::DropEvicted() {
		while (m_VisibleTail < m_VisibleHead && GetVisible(0).m_Sequence < m_Tail) {
			RetireOldestVisible();
		}
	}

	void ConsolePanel::PushVisible(const uint64_t sequence, const LogRecord& record) {
		if (!PassesFilter(record)) {
			return;
		}

		if (GetVisibleCount() >= LogBuffer::s_Capacity) {
			RetireOldestVisible();
		}

		m_Visible[m_VisibleHead % LogBuffer::s_Capacity] = VisibleEntry{ .m_Sequence = sequence, .m_Level = record.m_Level, .m_Continuation = record.m_Continuation };
		++m_VisibleHead;

		const auto levelIndex = static_cast<uint64_t>(record.m_Level);
		if (!record.m_Continuation && levelIndex < m_LevelCounts.size()) {
			++m_LevelCounts[levelIndex];
		}
	}

	void ConsolePanel::ClearDisplay() {
		m_Tail = m_Head;
		m_VisibleTail = 0;
		m_VisibleHead = 0;
		m_LevelCounts.fill(0);
		m_MaxRowWidth = 0.0f;
		m_FilterDirty = false;

		ClearSelection();
	}

	bool ConsolePanel::PassesFilter(const LogRecord& record) {
		if (record.m_Level < m_MinLevel) {
			return false;
		}

		if (record.m_CoreChannel ? !m_ShowCore : !m_ShowClient) {
			return false;
		}

		if (!m_TextFilter.IsActive()) {
			return true;
		}

		m_FilterScratch = record.m_Tags;
		m_FilterScratch.append(record.m_Message);

		return m_TextFilter.PassFilter(m_FilterScratch.c_str(), m_FilterScratch.c_str() + m_FilterScratch.size());
	}

	void ConsolePanel::RebuildVisible() {
		m_VisibleTail = 0;
		m_VisibleHead = 0;
		m_LevelCounts.fill(0);
		m_MaxRowWidth = 0.0f;

		uint64_t cursor = m_Tail;
		uint64_t bufferTail = 0;

		LogBuffer::Consume(cursor, bufferTail, [this](const uint64_t sequence, const LogRecord& record) {
			PushVisible(sequence, record);
		});

		m_Tail = std::max(m_Tail, bufferTail);
		m_Head = std::max(m_Head, cursor);

		m_FilterDirty = false;

		ClearSelection();
	}

	void ConsolePanel::RebuildTagTree() {
		m_TagNodes.clear();
		m_TagRoots.clear();

		std::unordered_map<const LogTag*, uint32_t> indexOf;

		LogTag::ForEach([&](const LogTag& tag) {
			indexOf.emplace(&tag, static_cast<uint32_t>(m_TagNodes.size()));
			m_TagNodes.push_back(TagNode{ .m_Tag = &tag, .m_Label = std::string(tag.GetName()), .m_Children = {} });
		});

		for (uint32_t i = 0; i < m_TagNodes.size(); ++i) {
			const LogTag* parent = m_TagNodes[i].m_Tag->GetParent();
			const auto it = parent != nullptr ? indexOf.find(parent) : indexOf.end();

			if (it == indexOf.end()) {
				m_TagRoots.push_back(i);
			}
			else {
				m_TagNodes[it->second].m_Children.push_back(i);
			}
		}

		const auto byLabel = [this](const uint32_t lhs, const uint32_t rhs) {
			return m_TagNodes[lhs].m_Label < m_TagNodes[rhs].m_Label;
		};

		std::ranges::sort(m_TagRoots, byLabel);

		for (TagNode& node : m_TagNodes) {
			std::ranges::sort(node.m_Children, byLabel);
		}
	}

	void ConsolePanel::Draw(bool* open, const char* name) {
		CORI_PROFILE_FUNCTION();
		Pull();

		if (open != nullptr && !*open) {
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(980.0f, 420.0f), ImGuiCond_FirstUseEver);

		if (ImGui::Begin(name, open, ImGuiWindowFlags_NoCollapse)) {
			if (m_FilterDirty) {
				RebuildVisible();
			}

			DrawToolbar();

			ImGui::Separator();

			const float completionHeight = CompletionHeight();
			const float bodyHeight = -(ImGui::GetFrameHeightWithSpacing() + (completionHeight > 0.0f ? completionHeight + ImGui::GetStyle().ItemSpacing.y : 0.0f));

			if (m_ShowTagPane) {
				DrawTagPane(bodyHeight);
				ImGui::SameLine();
			}

			DrawEntries(bodyHeight);

			DrawCompletions(completionHeight);

			DrawPrompt();
		}

		ImGui::End();
	}

	void ConsolePanel::DrawPrompt() {
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Slash, false)) {
			ImGui::SetKeyboardFocusHere();

			m_SuppressPromptChar = true;
		}

		ImGui::SetNextItemWidth(-FLT_MIN);

		constexpr ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter;

		const bool submitted = ImGui::InputText("##input", m_InputBuffer.data(), m_InputBuffer.size(), flags, &ConsolePanel::PromptCallback, this);

		m_SuppressPromptChar = false;

		if (!ImGui::IsItemActive() && !submitted) {
			CloseCompletions();
		}

		if (!submitted) {
			return;
		}

		SubmitPrompt(m_InputBuffer.data());

		m_InputBuffer[0] = '\0';
		CloseCompletions();
		ImGui::SetKeyboardFocusHere(-1);
	}

	void ConsolePanel::CloseCompletions() {
		m_CompletionOpen = false;
		m_CompletionIndex = -1;
		m_AcceptIndex = -1;
		m_Cycling = false;
		m_Completions.clear();
		m_CompletionLine.clear();
		m_CompletionCaret = std::string::npos;
	}

	std::string ConsolePanel::RenderCandidate(const int32_t index) const {
		const std::string_view text = m_Completions[static_cast<uint64_t>(index)].m_Text;

		if (text.find(' ') == std::string_view::npos) {
			return std::string(text);
		}

		std::string quoted;
		quoted.reserve(text.size() + 2);
		quoted += '"';
		quoted.append(text);
		quoted += '"';
		return quoted;
	}

	void ConsolePanel::BeginCycle(const std::string_view line, uint64_t caret) {
		caret = std::min(caret, line.size());

		uint64_t begin = caret;
		while (begin > 0) {
			const char previous = line[begin - 1];
			if (previous == ' ' || previous == '\t' || previous == '"') {
				break;
			}
			--begin;
		}

		uint64_t end = caret;
		if (begin > 0 && line[begin - 1] == '"') {
			--begin;
			if (end < line.size() && line[end] == '"') {
				++end;
			}
		}

		m_CycleBegin = begin;
		m_CycleEnd = end;
		m_Cycling = true;
	}

	void ConsolePanel::ApplyCycleStep(ImGuiInputTextCallbackData* data) {
		const std::string_view line(data->Buf, static_cast<uint64_t>(data->BufTextLen));
		const std::string rendered = RenderCandidate(m_CompletionIndex);

		std::string result;
		result.append(line.substr(0, std::min(m_CycleBegin, line.size())));
		result.append(rendered);

		const uint64_t caret = result.size();
		result.append(line.substr(std::min(m_CycleEnd, line.size())));

		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, result.c_str(), result.c_str() + result.size());
		data->CursorPos = static_cast<int>(caret);
		data->SelectionStart = data->CursorPos;
		data->SelectionEnd = data->CursorPos;

		m_CycleEnd = m_CycleBegin + rendered.size();

		m_CompletionLine.assign(result);
		m_CompletionCaret = caret;
	}

	void ConsolePanel::RefreshCompletions(const std::string_view line, const uint64_t caret) {
		if (m_CompletionCaret == caret && m_CompletionLine == line) {
			return;
		}

		m_Cycling = false;

		m_CompletionLine.assign(line);
		m_CompletionCaret = caret;

		std::string_view word;
		Core::Console::Complete(line, caret, m_Completions, word);

		m_CompletionOpen = !m_Completions.empty() && !line.empty();
		m_CompletionIndex = -1;
	}

	float ConsolePanel::CompletionHeight() const {
		if (!m_CompletionOpen || m_Completions.empty()) {
			return 0.0f;
		}

		const auto rows = static_cast<float>(std::min(static_cast<int32_t>(m_Completions.size()), s_MaxVisibleCompletions));
		return rows * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y * 2.0f;
	}

	void ConsolePanel::DrawCompletions(const float height) {
		if (height <= 0.0f) {
			return;
		}

		if (ImGui::BeginChild("##completions", ImVec2(0.0f, height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoNavInputs)) {
			const float contentWidth = ImGui::GetContentRegionAvail().x;

			for (int32_t i = 0; i < static_cast<int32_t>(m_Completions.size()); ++i) {
				const Core::ConsoleCompletion& completion = m_Completions[i];
				const bool selected = i == m_CompletionIndex;

				m_CompletionScratch.assign(completion.m_Text.data(), completion.m_Text.size());

				ImGui::PushID(i);
				if (ImGui::Selectable(m_CompletionScratch.c_str(), selected)) {
					m_AcceptIndex = i;
				}

				if (!completion.m_Detail.empty()) {
					const float detailWidth = ImGui::CalcTextSize(completion.m_Detail.c_str()).x;
					ImGui::SameLine(contentWidth - detailWidth);
					ImGui::TextDisabled("%s", completion.m_Detail.c_str());
				}

				if (selected && m_ScrollToSelection) {
					ImGui::SetScrollHereY(0.5f);
				}
				ImGui::PopID();
			}

			m_ScrollToSelection = false;
		}

		ImGui::EndChild();
	}

	void ConsolePanel::MoveCompletionSelection(const int32_t delta) {
		const auto count = static_cast<int32_t>(m_Completions.size());
		if (count == 0) {
			return;
		}

		if (m_CompletionIndex < 0) {
			m_CompletionIndex = delta > 0 ? 0 : count - 1;
		}
		else {
			m_CompletionIndex = (m_CompletionIndex + delta + count) % count;
		}

		m_ScrollToSelection = true;
	}

	std::pair<std::string, uint64_t> ConsolePanel::BuildCompletedLine(const std::string_view line, uint64_t caret, const int32_t index) const {
		caret = std::min(caret, line.size());

		uint64_t wordBegin = caret;
		while (wordBegin > 0) {
			const char previous = line[wordBegin - 1];
			if (previous == ' ' || previous == '\t' || previous == '"') {
				break;
			}
			--wordBegin;
		}

		uint64_t tailBegin = caret;
		if (wordBegin > 0 && line[wordBegin - 1] == '"') {
			--wordBegin;
			if (tailBegin < line.size() && line[tailBegin] == '"') {
				++tailBegin;
			}
		}

		const std::string_view text = m_Completions[static_cast<uint64_t>(index)].m_Text;
		const bool needsQuotes = text.find(' ') != std::string_view::npos;

		std::string result;
		result.append(line.substr(0, wordBegin));

		if (needsQuotes) {
			result += '"';
			result.append(text);
			result += '"';
		}
		else {
			result.append(text);
		}
		result += ' ';

		const uint64_t newCaret = result.size();
		result.append(line.substr(tailBegin));

		return { result, newCaret };
	}

	void ConsolePanel::SubmitPrompt(std::string_view command) {
		while (!command.empty() && (command.front() == ' ' || command.front() == '\t')) {
			command.remove_prefix(1);
		}
		while (!command.empty() && (command.back() == ' ' || command.back() == '\t')) {
			command.remove_suffix(1);
		}

		if (command.empty()) {
			return;
		}

		CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eInfo, { Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "> {}", command);

		if (m_History.empty() || m_History.back() != command) {
			m_History.emplace_back(command);
			if (m_History.size() > s_MaxHistory) {
				m_History.erase(m_History.begin());
			}
		}
		m_HistoryPos = -1;

		const Core::ConsoleResult result = Core::Console::Execute(command);

		if (result) {
			if (!result->empty()) {
				CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eInfo, { Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "{}", *result);
			}
		}
		else {
			CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eError, { Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "{}", result.error());
		}

		if (Core::Console::ConsumeClearRequest()) {
			ClearDisplay();
		}
	}

	int ConsolePanel::PromptCallback(ImGuiInputTextCallbackData* data) {
		auto* panel = static_cast<ConsolePanel*>(data->UserData);

		switch (data->EventFlag) {
		case ImGuiInputTextFlags_CallbackCompletion: return panel->OnPromptCompletion(data);
		case ImGuiInputTextFlags_CallbackHistory:    return panel->OnPromptHistory(data);
		case ImGuiInputTextFlags_CallbackAlways:     return panel->OnPromptAlways(data);
		case ImGuiInputTextFlags_CallbackCharFilter: return panel->OnPromptCharFilter(data);
		default: return 0;
		}
	}

	int ConsolePanel::OnPromptCharFilter(ImGuiInputTextCallbackData* data) const {
		if (m_SuppressPromptChar && data->EventChar == '/') {
			return 1;
		}
		return 0;
	}

	int ConsolePanel::OnPromptAlways(ImGuiInputTextCallbackData* data) {
		const std::string_view line(data->Buf, static_cast<uint64_t>(data->BufTextLen));
		const auto caret = static_cast<uint64_t>(data->CursorPos);

		if (m_AcceptIndex >= 0 && m_AcceptIndex < static_cast<int32_t>(m_Completions.size())) {
			const auto [completed, newCaret] = BuildCompletedLine(line, caret, m_AcceptIndex);
			m_AcceptIndex = -1;

			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, completed.c_str(), completed.c_str() + completed.size());
			data->CursorPos = static_cast<int>(newCaret);
			data->SelectionStart = data->CursorPos;
			data->SelectionEnd = data->CursorPos;

			m_CompletionIndex = -1;
			m_Cycling = false;
			m_CompletionLine.clear();
			m_CompletionCaret = std::string::npos;
			return 0;
		}

		RefreshCompletions(line, caret);
		return 0;
	}

	int ConsolePanel::OnPromptCompletion(ImGuiInputTextCallbackData* data) {
		const std::string_view line(data->Buf, static_cast<uint64_t>(data->BufTextLen));
		const auto caret = std::min(static_cast<uint64_t>(data->CursorPos), line.size());

		uint64_t wordBegin = caret;
		while (wordBegin > 0) {
			const char previous = line[wordBegin - 1];
			if (previous == ' ' || previous == '\t' || previous == '"') {
				break;
			}
			--wordBegin;
		}

		const std::string_view word = line.substr(wordBegin, caret - wordBegin);

		if (m_CompletionOpen && m_Completions.size() > 1) {
			if (!m_Cycling) {
				BeginCycle(line, caret);
			}

			MoveCompletionSelection(1);
			ApplyCycleStep(data);
			return 0;
		}

		if (!word.empty()) {
			if (const std::string usage = Core::Console::Usage(word); !usage.empty()) {
				CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eInfo, { Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "{}", usage);

				const Core::CVarDesc* desc = Core::Console::Find(word);
				const bool hasValueList = desc != nullptr && (desc->IsEnum() || desc->m_Type == Core::CVarType::eBool);

				if (hasValueList && caret == line.size()) {
					data->InsertChars(data->CursorPos, " ");
				}
				return 0;
			}
		}

		if (m_Completions.empty()) {
			if (const std::string usage = Core::Console::ContextUsage(line, caret); !usage.empty()) {
				CORI_LOG_CORE_TAGGED(::Cori::LogLevel::eInfo, { Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "{}", usage);
			}
			return 0;
		}

		const auto [completed, newCaret] = BuildCompletedLine(line, caret, 0);

		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, completed.c_str(), completed.c_str() + completed.size());
		data->CursorPos = static_cast<int>(newCaret);
		data->SelectionStart = data->CursorPos;
		data->SelectionEnd = data->CursorPos;

		m_CompletionIndex = -1;
		m_Cycling = false;
		m_CompletionLine.clear();
		m_CompletionCaret = std::string::npos;
		return 0;
	}

	int ConsolePanel::OnPromptHistory(ImGuiInputTextCallbackData* data) {
		if (m_CompletionOpen && m_Completions.size() > 1) {
			const int32_t delta = data->EventKey == ImGuiKey_UpArrow ? -1 : 1;
			if (!m_Cycling) {
				BeginCycle(std::string_view(data->Buf, static_cast<uint64_t>(data->BufTextLen)), static_cast<uint64_t>(data->CursorPos));
			}

			MoveCompletionSelection(delta);
			ApplyCycleStep(data);
			return 0;
		}

		if (m_History.empty()) {
			return 0;
		}

		const int32_t previous = m_HistoryPos;
		const auto count = static_cast<int32_t>(m_History.size());

		if (data->EventKey == ImGuiKey_UpArrow) {
			if (m_HistoryPos < 0) {
				m_HistoryPos = count - 1;
			}
			else if (m_HistoryPos > 0) {
				--m_HistoryPos;
			}
		}
		else if (data->EventKey == ImGuiKey_DownArrow) {
			if (m_HistoryPos >= 0) {
				++m_HistoryPos;
				if (m_HistoryPos >= count) {
					m_HistoryPos = -1;
				}
			}
		}

		if (m_HistoryPos == previous) {
			return 0;
		}

		data->DeleteChars(0, data->BufTextLen);

		if (m_HistoryPos >= 0) {
			const std::string& entry = m_History[static_cast<uint64_t>(m_HistoryPos)];
			data->InsertChars(0, entry.c_str(), entry.c_str() + entry.size());
		}

		return 0;
	}

	void ConsolePanel::DrawToolbar() {
		LogLevel minLevel = m_MinLevel;
		if (LevelCombo("##minlevel", minLevel, 84.0f, LogLevel::eFatal)) {
			m_MinLevel = minLevel;
			m_FilterDirty = true;
		}
		ImGui::SetItemTooltip("Hide captured lines below this severity");

		ImGui::SameLine();
		if (ImGui::Checkbox("ENGINE", &m_ShowCore)) {
			m_FilterDirty = true;
		}

		ImGui::SameLine();
		if (ImGui::Checkbox("APP", &m_ShowClient)) {
			m_FilterDirty = true;
		}

		ImGui::SameLine();
		ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
		if (m_TextFilter.Draw("##filter", 240.0f)) {
			m_FilterDirty = true;
		}
		ImGui::SetItemTooltip("Substring filter over tags and message. Ctrl+F to jump here.\nComma separates terms, a leading - excludes.");

		ImGui::SameLine();
		ImGui::Checkbox("Categories", &m_ShowTagPane);

		ImGui::SameLine();
		ImGui::Checkbox("Follow", &m_AutoScroll);

		ImGui::SameLine();
		if (ImGui::Button("Columns")) {
			ImGui::OpenPopup("##columns");
		}

		if (ImGui::BeginPopup("##columns")) {
			ImGui::Checkbox("Time", &m_ShowTime);
			ImGui::Checkbox("Thread", &m_ShowThread);
			ImGui::Checkbox("Channel", &m_ShowChannel);
			ImGui::Checkbox("Tags", &m_ShowTags);
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (!m_HasSelection || m_SelectAnchor == m_SelectHead) {
			ImGui::TextDisabled("%zu / %zu", static_cast<uint64_t>(GetVisibleCount()), static_cast<uint64_t>(GetHeldCount()));
		}
		else {
			ImGui::TextDisabled("%zu selected / %zu / %zu", static_cast<uint64_t>(CountSelectedRows()), static_cast<uint64_t>(GetVisibleCount()), static_cast<uint64_t>(GetHeldCount()));
		}

		const auto levelBadge = [this](const LogLevel level) {
			const auto index = static_cast<uint64_t>(level);
			if (index >= m_LevelCounts.size() || m_LevelCounts[index] == 0) {
				return;
			}
			ImGui::SameLine();
			ImGui::TextColored(LevelMsgColor(level), "%s %u", LogLevelName(level), m_LevelCounts[index]);
		};

		levelBadge(LogLevel::eWarm);
		levelBadge(LogLevel::eError);
		levelBadge(LogLevel::eFatal);
	}

	void ConsolePanel::DrawTagPane(const float bodyHeight) {
		if (m_TagNodes.size() != LogTag::Count()) {
			RebuildTagTree();
		}

		if (ImGui::BeginChild("##categories", ImVec2(300.0f, bodyHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX)) {
			ImGui::TextUnformatted("Category floors");
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");

			if (ImGui::BeginItemTooltip()) {
				ImGui::TextUnformatted(
					"A message is dropped unless its severity reaches the floor of every tag it carries.\n"
					"Call sites list the whole chain, so raising a parent silences everything under it.\n"
					"This filters at the source and only affects lines logged from now on.");
				ImGui::EndTooltip();
			}

			if (ImGui::SmallButton("All Trace")) {
				LogTag::SetAllFloors(LogLevel::eTrace);
			}

			ImGui::SameLine();
			if (ImGui::SmallButton("All Off")) {
				LogTag::SetAllFloors(LogLevel::eOff);
			}

			ImGui::Separator();

			for (const uint32_t root : m_TagRoots) {
				DrawTagNode(root);
			}
		}

		ImGui::EndChild();
	}

	void ConsolePanel::DrawTagNode(const uint32_t index) {
		const TagNode& node = m_TagNodes[index];
		const LogTag& tag = *node.m_Tag;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
		if (node.m_Children.empty()) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		ImGui::PushID(static_cast<int>(index));

		const bool open = ImGui::TreeNodeEx(node.m_Label.c_str(), flags);

		constexpr float comboWidth = 76.0f;

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - comboWidth));

		LogLevel floor = tag.GetFloor();
		if (LevelCombo("##floor", floor, comboWidth, LogLevel::eOff)) {
			tag.SetFloor(floor);
		}

		if (open && !node.m_Children.empty()) {
			for (const uint32_t child : node.m_Children) {
				DrawTagNode(child);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	void ConsolePanel::DrawEntries(const float bodyHeight) {
		if (ImGui::BeginChild("##entries", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 1.0f));

			HandleSelectionShortcuts();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			const float lineHeight = ImGui::GetTextLineHeight();
			const float rowWidth = std::max(m_MaxRowWidth, ImGui::GetContentRegionAvail().x);

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(GetVisibleCount()), lineHeight + ImGui::GetStyle().ItemSpacing.y);

			while (clipper.Step()) {
				const auto first = static_cast<uint64_t>(clipper.DisplayStart);
				const auto last = static_cast<uint64_t>(clipper.DisplayEnd);

				m_VisibleRowsThisFrame.clear();
				for (uint64_t row = first; row < last; ++row) {
					m_VisibleRowsThisFrame.push_back(GetVisible(row).m_Sequence);
				}

				LogBuffer::Fetch(m_VisibleRowsThisFrame, m_RowRecords);

				for (uint64_t i = 0; i < m_VisibleRowsThisFrame.size(); ++i) {
					DrawEntry(first + i, m_RowRecords[i], drawList, lineHeight, rowWidth);
				}
			}

			clipper.End();

			if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				ClearSelection();
			}

			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				m_Selecting = false;
			}

			ImGui::PopStyleVar();

			if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}
		}

		ImGui::EndChild();
	}

	void ConsolePanel::DrawEntry(const uint64_t row, const LogRecord& record, ImDrawList* drawList, const float lineHeight, const float rowWidth) {
		const VisibleEntry& entry = GetVisible(row);

		const ImVec2 origin = ImGui::GetCursorScreenPos();

		ImGui::PushID(static_cast<int>(entry.m_Sequence));
		ImGui::InvisibleButton("##row", ImVec2(rowWidth, lineHeight));
		const bool pressed = ImGui::IsItemActivated();
		ImGui::PopID();

		if (record.m_Sequence != entry.m_Sequence) {
			return;
		}

		const uint64_t messageStart = BuildLine(record);
		const std::string_view line = m_RowScratch;

		const char* begin = m_RowScratch.data();
		const char* end = begin + m_RowScratch.size();

		if (pressed) {
			const uint32_t index = CharIndexAt(line, ImGui::GetIO().MousePos.x - origin.x);

			m_SelectAnchor = TextCursor{ .m_Sequence = entry.m_Sequence, .m_Char = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ? 0 : index };
			m_SelectHead = TextCursor{ .m_Sequence = entry.m_Sequence, .m_Char = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ? static_cast<uint32_t>(line.size()) : index };
			m_HasSelection = true;
			m_Selecting = !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		}
		else if (m_Selecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			const float mouseY = ImGui::GetIO().MousePos.y;

			if (mouseY >= origin.y && mouseY < origin.y + lineHeight) {
				m_SelectHead = TextCursor{ .m_Sequence = entry.m_Sequence, .m_Char = CharIndexAt(line, ImGui::GetIO().MousePos.x - origin.x) };
			}
		}

		if (m_HasSelection) {
			const auto [selectionBegin, selectionEnd] = GetSelectionRange();

			if (entry.m_Sequence >= selectionBegin.m_Sequence && entry.m_Sequence <= selectionEnd.m_Sequence) {
				const uint64_t from = entry.m_Sequence == selectionBegin.m_Sequence ? std::min<uint64_t>(selectionBegin.m_Char, line.size()) : 0;
				const uint64_t to = entry.m_Sequence == selectionEnd.m_Sequence ? std::min<uint64_t>(selectionEnd.m_Char, line.size()) : line.size();

				if (to > from || entry.m_Sequence != selectionEnd.m_Sequence) {
					const float x0 = origin.x + ImGui::CalcTextSize(begin, begin + from).x;
					const float x1 = origin.x + ImGui::CalcTextSize(begin, begin + to).x + (entry.m_Sequence != selectionEnd.m_Sequence ? ImGui::CalcTextSize(" ").x : 0.0f);

					drawList->AddRectFilled(ImVec2(x0, origin.y), ImVec2(x1, origin.y + lineHeight), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
				}
			}
		}

		float x = origin.x;

		if (messageStart > 0) {
			drawList->AddText(ImVec2(x, origin.y), ImGui::GetColorU32(LevelPrefixColor(record.m_Level)), begin, begin + messageStart);
			x += ImGui::CalcTextSize(begin, begin + messageStart).x;
		}

		if (messageStart < m_RowScratch.size()) {
			drawList->AddText(ImVec2(x, origin.y), ImGui::GetColorU32(LevelMsgColor(record.m_Level)), begin + messageStart, end);
			x += ImGui::CalcTextSize(begin + messageStart, end).x;
		}

		m_MaxRowWidth = std::max(m_MaxRowWidth, x - origin.x);
	}

	uint64_t ConsolePanel::BuildLine(const LogRecord& record) {
		m_RowScratch.clear();

		if (!record.m_Continuation) {
			const auto separate = [this] {
				if (!m_RowScratch.empty()) {
					m_RowScratch.push_back(' ');
				}
			};

			if (m_ShowTime) {
				separate();
				m_RowScratch.append(record.m_Time);
			}

			if (m_ShowChannel) {
				separate();
				m_RowScratch.append(record.m_CoreChannel ? "[ENGINE]" : "[APP   ]");
			}

			if (m_ShowThread) {
				separate();
				m_RowScratch.push_back('[');
				m_RowScratch.append(record.m_ThreadName);
				m_RowScratch.append(record.m_ThreadName.size() < 10 ? 10 - record.m_ThreadName.size() : 0, ' ');
				m_RowScratch.push_back(']');
			}

			if (m_ShowTags && !record.m_Tags.empty()) {
				separate();
				m_RowScratch.append(record.m_Tags);
			}

			if (!m_RowScratch.empty()) {
				m_RowScratch.append(": ");
			}
		}

		const uint64_t messageStart = m_RowScratch.size();
		m_RowScratch.append(record.m_Message);

		return messageStart;
	}

	uint32_t ConsolePanel::CharIndexAt(const std::string_view line, const float localX) {
		if (localX <= 0.0f || line.empty()) {
			return 0;
		}

		const char* begin = line.data();
		const char* end = begin + line.size();
		const char* remaining = nullptr;

		ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), localX, 0.0f, begin, end, &remaining);

		return static_cast<uint32_t>((remaining != nullptr ? remaining : end) - begin);
	}

	uint64_t ConsolePanel::CountSelectedRows() const {
		if (!m_HasSelection) {
			return 0;
		}

		const auto [selectionBegin, selectionEnd] = GetSelectionRange();

		const auto lowerBoundRow = [this](const uint64_t sequence) {
			uint64_t low = 0;
			uint64_t high = GetVisibleCount();

			while (low < high) {
				const uint64_t mid = low + (high - low) / 2;

				if (GetVisible(mid).m_Sequence < sequence) {
					low = mid + 1;
				}
				else {
					high = mid;
				}
			}

			return low;
		};

		return lowerBoundRow(selectionEnd.m_Sequence + 1) - lowerBoundRow(selectionBegin.m_Sequence);
	}

	std::pair<ConsolePanel::TextCursor, ConsolePanel::TextCursor> ConsolePanel::GetSelectionRange() const {
		return m_SelectHead < m_SelectAnchor ? std::pair{ m_SelectHead, m_SelectAnchor } : std::pair{ m_SelectAnchor, m_SelectHead };
	}

	void ConsolePanel::ClearSelection() {
		m_HasSelection = false;
		m_Selecting = false;
		m_SelectAnchor = TextCursor{};
		m_SelectHead = TextCursor{};
	}

	void ConsolePanel::HandleSelectionShortcuts() {
		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
			return;
		}

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A) && GetVisibleCount() > 0) {
			m_SelectAnchor = TextCursor{ .m_Sequence = GetVisible(0).m_Sequence, .m_Char = 0 };
			m_SelectHead = TextCursor{ .m_Sequence = GetVisible(GetVisibleCount() - 1).m_Sequence, .m_Char = std::numeric_limits<uint32_t>::max() };
			m_HasSelection = true;
			m_Selecting = false;
		}

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C)) {
			CopyToClipboard();
		}

		if (ImGui::Shortcut(ImGuiKey_Escape)) {
			ClearSelection();
		}
	}

	void ConsolePanel::CopyToClipboard() {
		const bool ranged = m_HasSelection && m_SelectAnchor != m_SelectHead;
		if (!ranged) {
			return;
		}

		TextCursor selectionBegin{};
		TextCursor selectionEnd{};

		const auto range = GetSelectionRange();
		selectionBegin = range.first;
		selectionEnd = range.second;

		std::vector<uint64_t> sequences;
		sequences.reserve(GetVisibleCount());

		for (uint64_t row = 0; row < GetVisibleCount(); ++row) {
			const uint64_t sequence = GetVisible(row).m_Sequence;

			if (ranged && (sequence < selectionBegin.m_Sequence || sequence > selectionEnd.m_Sequence)) {
				continue;
			}

			sequences.push_back(sequence);
		}

		std::vector<LogRecord> records;
		LogBuffer::Fetch(sequences, records);

		std::string out;
		out.reserve(records.size() * 128);

		for (uint64_t i = 0; i < records.size(); ++i) {
			if (records[i].m_Sequence != sequences[i]) {
				continue;
			}

			BuildLine(records[i]);

			uint64_t from = 0;
			uint64_t to = m_RowScratch.size();

			if (sequences[i] == selectionBegin.m_Sequence) {
				from = std::min<uint64_t>(selectionBegin.m_Char, m_RowScratch.size());
			}

			if (sequences[i] == selectionEnd.m_Sequence) {
				to = std::min<uint64_t>(selectionEnd.m_Char, m_RowScratch.size());
			}

			if (to > from) {
				out.append(m_RowScratch, from, to - from);
			}

			out.push_back('\n');
		}

		if (!out.empty()) {
			out.pop_back();
		}

		ImGui::SetClipboardText(out.c_str());
	}

	bool ConsolePanel::LevelCombo(const char* label, LogLevel& level, const float width, const LogLevel highest) {
		bool changed = false;

		ImGui::SetNextItemWidth(width);

		if (ImGui::BeginCombo(label, LogLevelName(level))) {
			for (uint8_t i = 0; i <= static_cast<uint8_t>(highest); ++i) {
				const auto candidate = static_cast<LogLevel>(i);
				const bool selected = candidate == level;

				if (ImGui::Selectable(LogLevelName(candidate), selected)) {
					level = candidate;
					changed = true;
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		return changed;
	}
}
