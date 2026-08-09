#include "Console.hpp"
#include "FileSystem/FileManager.hpp"
#include "FileSystem/PathManager.hpp"
#include <charconv>
#include <cstring>
#include <cctype>

namespace {
	using namespace Cori;
	using namespace Cori::Core;

	struct CVarBlock {
		std::span<const CVarDesc> m_Schema;
		Byte* m_Live{ nullptr };
		std::vector<Byte> m_Pending;
		std::string_view m_Struct;
		ApplyTier m_Tier{ ApplyTier::eLive };
	};

	struct LookupEntry {
		const CVarDesc* m_Desc{ nullptr };
		const ConsoleCommand* m_Command{ nullptr };
		uint32_t m_Block{ 0 };
	};

	std::vector<CVarBlock>& Blocks() {
		static std::vector<CVarBlock> blocks;
		return blocks;
	}

	std::unordered_map<std::string_view, LookupEntry>& Lookup() {
		static std::unordered_map<std::string_view, LookupEntry> lookup;
		return lookup;
	}

	constinit bool s_LookupDirty{ true };
	constinit bool s_Initialized{ false };
	constinit bool s_CheatsEnabled{ true };
	constinit bool s_ClearRequested{ false };

	std::thread::id& MainThreadId() {
		static std::thread::id id;
		return id;
	}

	void RebuildLookup() {
		std::unordered_map<std::string_view, LookupEntry>& lookup = Lookup();
		lookup.clear();

		const std::vector<CVarBlock>& blocks = Blocks();
		for (uint32_t blockIndex = 0; blockIndex < blocks.size(); ++blockIndex) {
			for (const CVarDesc& desc : blocks[blockIndex].m_Schema) {
				const auto [it, inserted] = lookup.try_emplace(desc.m_Name, LookupEntry{ .m_Desc = &desc, .m_Command = nullptr, .m_Block = blockIndex });
				if (!inserted) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "Duplicate console name '{}', the one from '{}' is kept and the one from '{}' is unreachable.", desc.m_Name, it->second.m_Desc != nullptr ? it->second.m_Desc->m_Struct : std::string_view{ "?" }, desc.m_Struct);
				}
			}
		}

		ConsoleCommand::ForEach([&lookup](const ConsoleCommand& command) {
			const auto [it, inserted] = lookup.try_emplace(command.GetName(), LookupEntry{ .m_Desc = nullptr, .m_Command = &command, .m_Block = 0 });
			if (!inserted) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "Console command '{}' collides with an already registered name, it is unreachable.", command.GetName());
			}
		});

		s_LookupDirty = false;
	}

	const LookupEntry* FindEntry(const std::string_view name) {
		if (s_LookupDirty) {
			RebuildLookup();
		}
		const std::unordered_map<std::string_view, LookupEntry>& lookup = Lookup();
		const auto it = lookup.find(name);
		return it == lookup.end() ? nullptr : &it->second;
	}

	[[nodiscard]] double ReadField(const Byte* base, const CVarDesc& desc) {
		const void* field = base + desc.m_Offset;
		switch (desc.m_Type) {
		case CVarType::eBool:   return *static_cast<const bool*>(field) ? 1.0 : 0.0;
		case CVarType::eInt8:   return static_cast<double>(*static_cast<const int8_t*>(field));
		case CVarType::eInt16:  return static_cast<double>(*static_cast<const int16_t*>(field));
		case CVarType::eInt32:  return static_cast<double>(*static_cast<const int32_t*>(field));
		case CVarType::eInt64:  return static_cast<double>(*static_cast<const int64_t*>(field));
		case CVarType::eUInt8:  return static_cast<double>(*static_cast<const uint8_t*>(field));
		case CVarType::eUInt16: return static_cast<double>(*static_cast<const uint16_t*>(field));
		case CVarType::eUInt32: return static_cast<double>(*static_cast<const uint32_t*>(field));
		case CVarType::eUInt64: return static_cast<double>(*static_cast<const uint64_t*>(field));
		case CVarType::eFloat:  return static_cast<double>(*static_cast<const float*>(field));
		case CVarType::eDouble: return *static_cast<const double*>(field);
		}
		return 0.0;
	}

	void WriteField(Byte* base, const CVarDesc& desc, const double value) {
		void* field = base + desc.m_Offset;
		switch (desc.m_Type) {
		case CVarType::eBool:   *static_cast<bool*>(field)     = value != 0.0; break;
		case CVarType::eInt8:   *static_cast<int8_t*>(field)   = static_cast<int8_t>(value); break;
		case CVarType::eInt16:  *static_cast<int16_t*>(field)  = static_cast<int16_t>(value); break;
		case CVarType::eInt32:  *static_cast<int32_t*>(field)  = static_cast<int32_t>(value); break;
		case CVarType::eInt64:  *static_cast<int64_t*>(field)  = static_cast<int64_t>(value); break;
		case CVarType::eUInt8:  *static_cast<uint8_t*>(field)  = static_cast<uint8_t>(value); break;
		case CVarType::eUInt16: *static_cast<uint16_t*>(field) = static_cast<uint16_t>(value); break;
		case CVarType::eUInt32: *static_cast<uint32_t*>(field) = static_cast<uint32_t>(value); break;
		case CVarType::eUInt64: *static_cast<uint64_t*>(field) = static_cast<uint64_t>(value); break;
		case CVarType::eFloat:  *static_cast<float*>(field)    = static_cast<float>(value); break;
		case CVarType::eDouble: *static_cast<double*>(field)   = value; break;
		}
	}

	[[nodiscard]] bool IsFloatingPoint(const CVarType type) {
		return type == CVarType::eFloat || type == CVarType::eDouble;
	}

	[[nodiscard]] bool IsUnsigned(const CVarType type) {
		return type == CVarType::eUInt8 || type == CVarType::eUInt16 || type == CVarType::eUInt32 || type == CVarType::eUInt64;
	}

	/**
	 * @brief Renders a raw value the way the console prints it and the way the archive stores it.
	 * @note A float field is narrowed back to float before formatting, otherwise the widening to double turns a
	 * declared 0.005f into 0.004999999888241291 in both the console and the settings file.
	 */
	[[nodiscard]] std::string FormatRaw(const CVarDesc& desc, const double value) {
		if (desc.IsEnum()) {
			const int64_t raw = static_cast<int64_t>(value);
			for (const EnumEntry& entry : desc.m_Enumerators) {
				if (entry.m_Value == raw) {
					return std::string(entry.m_Name);
				}
			}
			return std::format("{}", raw);
		}

		switch (desc.m_Type) {
		case CVarType::eBool:   return value != 0.0 ? "true" : "false";
		case CVarType::eFloat:  return std::format("{}", static_cast<float>(value));
		case CVarType::eDouble: return std::format("{}", value);
		default: break;
		}

		if (IsUnsigned(desc.m_Type)) {
			return std::format("{}", static_cast<uint64_t>(value));
		}
		return std::format("{}", static_cast<int64_t>(value));
	}

	[[nodiscard]] bool EqualsIgnoreCase(const std::string_view a, const std::string_view b) {
		if (a.size() != b.size()) {
			return false;
		}
		for (uint64_t i = 0; i < a.size(); ++i) {
			if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] std::optional<double> ParseRaw(const CVarDesc& desc, const std::string_view text) {
		if (text.empty()) {
			return std::nullopt;
		}

		if (desc.IsEnum()) {
			for (const EnumEntry& entry : desc.m_Enumerators) {
				if (EqualsIgnoreCase(entry.m_Name, text)) {
					return static_cast<double>(entry.m_Value);
				}
			}
			int64_t raw = 0;
			if (std::from_chars(text.data(), text.data() + text.size(), raw).ec == std::errc{}) {
				for (const EnumEntry& entry : desc.m_Enumerators) {
					if (entry.m_Value == raw) {
						return static_cast<double>(raw);
					}
				}
			}
			return std::nullopt;
		}

		if (desc.m_Type == CVarType::eBool) {
			if (EqualsIgnoreCase(text, "true") || EqualsIgnoreCase(text, "on") || EqualsIgnoreCase(text, "yes") || text == "1") {
				return 1.0;
			}
			if (EqualsIgnoreCase(text, "false") || EqualsIgnoreCase(text, "off") || EqualsIgnoreCase(text, "no") || text == "0") {
				return 0.0;
			}
			return std::nullopt;
		}

		if (IsFloatingPoint(desc.m_Type)) {
			double parsed = 0.0;
			if (std::from_chars(text.data(), text.data() + text.size(), parsed).ec != std::errc{}) {
				return std::nullopt;
			}
			return parsed;
		}

		if (IsUnsigned(desc.m_Type)) {
			uint64_t parsed = 0;
			if (std::from_chars(text.data(), text.data() + text.size(), parsed).ec != std::errc{}) {
				return std::nullopt;
			}
			return static_cast<double>(parsed);
		}

		int64_t parsed = 0;
		if (std::from_chars(text.data(), text.data() + text.size(), parsed).ec != std::errc{}) {
			return std::nullopt;
		}
		return static_cast<double>(parsed);
	}

	[[nodiscard]] std::string_view Trim(std::string_view text) {
		while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
			text.remove_prefix(1);
		}
		while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
			text.remove_suffix(1);
		}
		return text;
	}

	void Tokenize(const std::string_view line, std::vector<std::string_view>& out) {
		out.clear();

		uint64_t i = 0;
		while (i < line.size()) {
			while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
				++i;
			}
			if (i >= line.size()) {
				break;
			}

			if (line[i] == '"') {
				++i;
				const uint64_t start = i;
				while (i < line.size() && line[i] != '"') {
					++i;
				}
				out.push_back(line.substr(start, i - start));
				if (i < line.size()) {
					++i;
				}
			}
			else {
				const uint64_t start = i;
				while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
					++i;
				}
				out.push_back(line.substr(start, i - start));
			}
		}
	}

	[[nodiscard]] std::string SuggestionFor(const std::string_view name) {
		if (s_LookupDirty) {
			RebuildLookup();
		}

		std::vector<std::string_view> candidates;
		for (const auto& [key, entry] : Lookup()) {
			if (key.size() >= name.size() && EqualsIgnoreCase(key.substr(0, name.size()), name)) {
				candidates.push_back(key);
			}
		}

		if (candidates.empty()) {
			return {};
		}

		std::ranges::sort(candidates);
		if (candidates.size() == 1) {
			return std::format(" Did you mean '{}'?", candidates.front());
		}
		return std::format(" {} names start with '{}', try `find {}`.", candidates.size(), name, name);
	}

	[[nodiscard]] std::optional<LogLevel> ParseLogLevel(const std::string_view text) {
		for (uint8_t i = 0; i <= static_cast<uint8_t>(LogLevel::eOff); ++i) {
			const LogLevel level = static_cast<LogLevel>(i);
			if (EqualsIgnoreCase(text, LogLevelName(level))) {
				return level;
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] const LogTag* FindLogTag(const std::string_view name) {
		const LogTag* found = nullptr;
		LogTag::ForEach([&found, name](const LogTag& tag) {
			if (found == nullptr && EqualsIgnoreCase(tag.GetName(), name)) {
				found = &tag;
			}
		});
		return found;
	}

	[[nodiscard]] std::filesystem::path ArchivePath() {
		return FileSystem::PathManager::GetAliasedPath("USER_DATA") / "settings/cvars.json";
	}

	[[nodiscard]] std::string DescribeCVar(const CVarDesc& desc) {
		std::string text = std::format("{} = {}", desc.m_Name, Console::FormatValue(desc));

		if (desc.m_Tier == ApplyTier::eRestart) {
			const std::string pending = Console::FormatPendingValue(desc);
			if (pending != Console::FormatValue(desc)) {
				text += std::format(" (next run: {})", pending);
			}
		}

		text += std::format("  [default {}", Console::FormatDefault(desc));
		if (desc.m_HasRange) {
			text += std::format(", range {} to {}", desc.m_Min, desc.m_Max);
		}
		if (desc.m_Tier != ApplyTier::eLive) {
			text += std::format(", {}", "live");
		}
		if (HasFlag(desc.m_Flags, CVarFlags::eReadOnly)) {
			text += ", read only";
		}
		if (HasFlag(desc.m_Flags, CVarFlags::eCheat)) {
			text += ", cheat";
		}
		if (HasFlag(desc.m_Flags, CVarFlags::eArchive)) {
			text += ", archived";
		}
		text += "]";

		if (desc.IsEnum()) {
			text += "\n  values: ";
			bool first = true;
			for (const EnumEntry& entry : desc.m_Enumerators) {
				if (!first) {
					text += ", ";
				}
				text += entry.m_Name;
				first = false;
			}
		}

		if (!desc.m_Help.empty()) {
			text += "\n  " + std::string(desc.m_Help);
		}
		return text;
	}

	[[nodiscard]] std::string_view CVarTypeName(const CVarType type) {
		switch (type) {
		case CVarType::eBool:   return "bool";
		case CVarType::eInt8:   return "int8";
		case CVarType::eInt16:  return "int16";
		case CVarType::eInt32:  return "int32";
		case CVarType::eInt64:  return "int64";
		case CVarType::eUInt8:  return "uint8";
		case CVarType::eUInt16: return "uint16";
		case CVarType::eUInt32: return "uint32";
		case CVarType::eUInt64: return "uint64";
		case CVarType::eFloat:  return "float";
		case CVarType::eDouble: return "double";
		}
		return "?";
	}

	[[nodiscard]] bool StartsWithIgnoreCase(const std::string_view text, const std::string_view prefix) {
		return text.size() >= prefix.size() && EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
	}

	void AddNameCandidates(const std::string_view word, std::vector<ConsoleCompletion>& out) {
		if (s_LookupDirty) {
			RebuildLookup();
		}

		for (const auto& [name, entry] : Lookup()) {
			if (!StartsWithIgnoreCase(name, word)) {
				continue;
			}

			if (entry.m_Desc != nullptr) {
				out.push_back(ConsoleCompletion{ .m_Text = name, .m_Detail = Console::FormatValue(*entry.m_Desc) });
			}
			else {
				out.push_back(ConsoleCompletion{ .m_Text = name, .m_Detail = "command" });
			}
		}
	}

	/**
	 * @brief Offers the values a specific variable accepts, which is only a closed set for enums and bools.
	 */
	void AddValueCandidates(const CVarDesc& desc, const std::string_view word, std::vector<ConsoleCompletion>& out) {
		const std::string current = Console::FormatValue(desc);

		if (desc.IsEnum()) {
			for (const EnumEntry& entry : desc.m_Enumerators) {
				if (StartsWithIgnoreCase(entry.m_Name, word)) {
					out.push_back(ConsoleCompletion{ .m_Text = entry.m_Name, .m_Detail = entry.m_Name == current ? "current" : std::format("{}", entry.m_Value) });
				}
			}
			return;
		}

		if (desc.m_Type == CVarType::eBool) {
			for (const std::string_view value : { std::string_view("true"), std::string_view("false") }) {
				if (StartsWithIgnoreCase(value, word)) {
					out.push_back(ConsoleCompletion{ .m_Text = value, .m_Detail = value == current ? "current" : "" });
				}
			}
		}
	}

	void AddLogTagCandidates(const std::string_view word, std::vector<ConsoleCompletion>& out) {
		LogTag::ForEach([word, &out](const LogTag& tag) {
			if (StartsWithIgnoreCase(tag.GetName(), word)) {
				out.push_back(ConsoleCompletion{ .m_Text = tag.GetName(), .m_Detail = std::string(LogLevelName(tag.GetFloor())) });
			}
		});
	}

	void AddLogLevelCandidates(const std::string_view word, std::vector<ConsoleCompletion>& out) {
		for (uint8_t i = 0; i <= static_cast<uint8_t>(LogLevel::eOff); ++i) {
			const std::string_view name = LogLevelName(static_cast<LogLevel>(i));
			if (StartsWithIgnoreCase(name, word)) {
				out.push_back(ConsoleCompletion{ .m_Text = name, .m_Detail = "" });
			}
		}
	}
}

namespace Cori {
	namespace Core {

		namespace Internal {
			void RegisterCVarBlock(const std::span<const CVarDesc> schema, void* live, const uint64_t liveSize, const std::string_view structName, const ApplyTier tier) {
				if (live == nullptr || schema.empty()) {
					return;
				}

				CVarBlock block{
					.m_Schema = schema,
					.m_Live = static_cast<Byte*>(live),
					.m_Pending = std::vector<Byte>(liveSize),
					.m_Struct = structName,
					.m_Tier = tier
				};

				std::memcpy(block.m_Pending.data(), block.m_Live, liveSize);

				Blocks().push_back(std::move(block));
				s_LookupDirty = true;
			}
		}

		void Console::AssertMainThread() {
			CORI_CORE_ASSERT(MainThreadId() == std::thread::id{} || std::this_thread::get_id() == MainThreadId(), "The console was written from a thread other than the one that called Console::Init. Route the call through Application::SubmitMainTask, a settings struct has exactly one writer by design.");
		}

		void Console::Init() {
			MainThreadId() = std::this_thread::get_id();
			RebuildLookup();
			s_Initialized = true;

			uint64_t cvarCount = 0;
			for (const CVarBlock& block : Blocks()) {
				cvarCount += block.m_Schema.size();
			}

			uint64_t commandCount = 0;
			ConsoleCommand::ForEach([&commandCount](const ConsoleCommand&) { ++commandCount; });

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "Console initialized with {} variables across {} settings structs and {} commands.", cvarCount, Blocks().size(), commandCount);

			const std::expected<void, CoriError<>> loaded = LoadArchive();
			if (!loaded) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "The settings archive was not applied. {}", loaded.error().what());
			}
		}

		void Console::Shutdown() {
			if (!s_Initialized) {
				return;
			}

			const std::expected<void, CoriError<>> saved = SaveArchive();
			if (!saved) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "The settings archive was not written. {}", saved.error().what());
			}

			s_Initialized = false;
		}

		bool Console::GetStatus() {
			return s_Initialized;
		}

		const CVarDesc* Console::Find(const std::string_view name) {
			const LookupEntry* entry = FindEntry(name);
			return entry != nullptr ? entry->m_Desc : nullptr;
		}

		std::string Console::FormatValue(const CVarDesc& desc) {
			const LookupEntry* entry = FindEntry(desc.m_Name);
			if (entry == nullptr || entry->m_Desc == nullptr) {
				return "?";
			}
			return FormatRaw(desc, ReadField(Blocks()[entry->m_Block].m_Live, desc));
		}

		std::string Console::FormatPendingValue(const CVarDesc& desc) {
			const LookupEntry* entry = FindEntry(desc.m_Name);
			if (entry == nullptr || entry->m_Desc == nullptr) {
				return "?";
			}
			return FormatRaw(desc, ReadField(Blocks()[entry->m_Block].m_Pending.data(), desc));
		}

		std::string Console::FormatDefault(const CVarDesc& desc) {
			return FormatRaw(desc, desc.m_Default);
		}

		ConsoleResult Console::SetValue(const CVarDesc& desc, const std::string_view text) {
			AssertMainThread();

			if (HasFlag(desc.m_Flags, CVarFlags::eReadOnly)) {
				return std::unexpected(std::format("'{}' is read only.", desc.m_Name));
			}

			if (HasFlag(desc.m_Flags, CVarFlags::eCheat) && !s_CheatsEnabled) {
				return std::unexpected(std::format("'{}' is cheat protected, run `cheats 1` first.", desc.m_Name));
			}

			const std::optional<double> parsed = ParseRaw(desc, text);
			if (!parsed) {
				if (desc.IsEnum()) {
					std::string values;
					for (const EnumEntry& entry : desc.m_Enumerators) {
						if (!values.empty()) {
							values += ", ";
						}
						values += entry.m_Name;
					}
					return std::unexpected(std::format("'{}' is not one of the values '{}' accepts: {}.", text, desc.m_Name, values));
				}
				return std::unexpected(std::format("'{}' could not be read as a value for '{}'.", text, desc.m_Name));
			}

			double value = *parsed;
			bool clamped = false;
			if (desc.m_HasRange) {
				const double bounded = std::clamp(value, desc.m_Min, desc.m_Max);
				clamped = bounded != value;
				value = bounded;
			}

			const LookupEntry* entry = FindEntry(desc.m_Name);
			if (entry == nullptr) {
				return std::unexpected(std::format("'{}' is no longer registered.", desc.m_Name));			}

			CVarBlock& block = Blocks()[entry->m_Block];
			
			WriteField(block.m_Pending.data(), desc, value);

			std::string result;
			switch (desc.m_Tier) {
			case ApplyTier::eLive: {
				const double previous = ReadField(block.m_Live, desc);
				WriteField(block.m_Live, desc, value);
				result = std::format("{} = {}", desc.m_Name, FormatRaw(desc, value));
				
				if (desc.m_OnChange != nullptr && previous != value) {
					desc.m_OnChange(desc);
				}
				break;
			}
			case ApplyTier::eRestart:
				result = std::format("{} = {} (takes effect on the next run, currently {})", desc.m_Name, FormatRaw(desc, value), FormatValue(desc));
				break;
			}

			if (clamped) {
				result += std::format(" [clamped into {} to {}]", desc.m_Min, desc.m_Max);
			}
			return result;
		}

		ConsoleResult Console::ResetValue(const CVarDesc& desc) {
			return SetValue(desc, FormatRaw(desc, desc.m_Default));
		}

		void Console::SetCheatsEnabled(const bool enabled) {
			s_CheatsEnabled = enabled;
		}

		bool Console::GetCheatsEnabled() {
			return s_CheatsEnabled;
		}

		bool Console::ConsumeClearRequest() {
			const bool requested = s_ClearRequested;
			s_ClearRequested = false;
			return requested;
		}

		void Console::ForEachCVar(const std::function<void(const CVarDesc&)>& f) {
			for (const CVarBlock& block : Blocks()) {
				for (const CVarDesc& desc : block.m_Schema) {
					f(desc);
				}
			}
		}

		void Console::Complete(std::string_view line, uint64_t caret, std::vector<ConsoleCompletion>& out, std::string_view& wordOut) {
			out.clear();

			caret = std::min(caret, line.size());
			
			uint64_t wordBegin = caret;
			while (wordBegin > 0) {
				const char previous = line[wordBegin - 1];
				if (previous == ' ' || previous == '\t' || previous == '"') {
					break;
				}
				--wordBegin;
			}

			const std::string_view word = line.substr(wordBegin, caret - wordBegin);
			wordOut = word;

			std::vector<std::string_view> preceding;
			Tokenize(line.substr(0, wordBegin), preceding);
			const uint64_t slot = preceding.size();

			if (slot == 0) {
				AddNameCandidates(word, out);
			}
			else {
				const std::string_view leading = preceding.front();
				const LookupEntry* entry = FindEntry(leading);

				if (entry != nullptr && entry->m_Desc != nullptr) {
					if (slot == 1) {
						AddValueCandidates(*entry->m_Desc, word, out);
					}
				}
				else if (EqualsIgnoreCase(leading, "log.level")) {
					if (slot == 1) {
						AddLogTagCandidates(word, out);
					}
					else if (slot == 2) {
						AddLogLevelCandidates(word, out);
					}
				}
				else if (EqualsIgnoreCase(leading, "log.level.all")) {
					if (slot == 1) {
						AddLogLevelCandidates(word, out);
					}
				}
				else if (EqualsIgnoreCase(leading, "set")) {
					if (slot == 1) {
						AddNameCandidates(word, out);
					}
					else if (slot == 2) {
						if (const CVarDesc* target = Find(preceding[1])) {
							AddValueCandidates(*target, word, out);
						}
					}
				}
				else if (EqualsIgnoreCase(leading, "reset") || EqualsIgnoreCase(leading, "help") || EqualsIgnoreCase(leading, "find")) {
					if (slot == 1) {
						AddNameCandidates(word, out);
					}
				}
			}

			std::ranges::sort(out, [](const ConsoleCompletion& lhs, const ConsoleCompletion& rhs) {
				return lhs.m_Text < rhs.m_Text;
			});
		}

		std::string Console::ContextUsage(std::string_view line, uint64_t caret) {
			caret = std::min(caret, line.size());

			uint64_t wordBegin = caret;
			while (wordBegin > 0) {
				const char previous = line[wordBegin - 1];
				if (previous == ' ' || previous == '\t' || previous == '"') {
					break;
				}
				--wordBegin;
			}

			if (const std::string_view word = line.substr(wordBegin, caret - wordBegin); !word.empty()) {
				if (std::string usage = Usage(word); !usage.empty()) {
					return usage;
				}
			}

			std::vector<std::string_view> preceding;
			Tokenize(line.substr(0, wordBegin), preceding);

			for (const std::string_view token : std::views::reverse(preceding)) {
				if (std::string usage = Usage(token); !usage.empty()) {
					return usage;
				}
			}

			return {};
		}

		std::string Console::Usage(const std::string_view name) {
			const LookupEntry* entry = FindEntry(name);
			if (entry == nullptr) {
				return {};
			}

			if (entry->m_Command != nullptr) {
				return std::string(entry->m_Command->GetHelp());
			}

			const CVarDesc& desc = *entry->m_Desc;

			std::string text = std::string(desc.m_Name);
			text += "  ";

			if (desc.IsEnum()) {
				text += "enum, one of: ";
				bool first = true;
				for (const EnumEntry& enumerator : desc.m_Enumerators) {
					if (!first) {
						text += ", ";
					}
					text += enumerator.m_Name;
					first = false;
				}
			}
			else if (desc.m_Type == CVarType::eBool) {
				text += "bool, accepts true/false, on/off, yes/no, 1/0";
			}
			else if (desc.m_HasRange) {
				text += std::format("{} in [{}, {}]", CVarTypeName(desc.m_Type), FormatRaw(desc, desc.m_Min), FormatRaw(desc, desc.m_Max));
			}
			else {
				text += CVarTypeName(desc.m_Type);
			}

			text += std::format("   (now {}, default {})", FormatValue(desc), FormatDefault(desc));

			if (desc.m_Tier == ApplyTier::eRestart) {
				text += " [restart]";
			}
			if (HasFlag(desc.m_Flags, CVarFlags::eReadOnly)) {
				text += " [read only]";
			}
			if (HasFlag(desc.m_Flags, CVarFlags::eCheat)) {
				text += " [cheat]";
			}

			if (!desc.m_Help.empty()) {
				text += "\n  " + std::string(desc.m_Help);
			}
			return text;
		}

		ConsoleResult Console::Execute(std::string_view line) {
			AssertMainThread();

			line = Trim(line);
			if (line.empty()) {
				return std::string{};
			}

			std::vector<std::string_view> tokens;
			Tokenize(line, tokens);
			if (tokens.empty()) {
				return std::string{};
			}

			const std::string_view name = tokens.front();
			const ConsoleArgs args = ConsoleArgs(tokens).subspan(1);

			const LookupEntry* entry = FindEntry(name);
			if (entry == nullptr) {
				return std::unexpected(std::format("Unknown command or variable '{}'.{}", name, SuggestionFor(name)));
			}

			if (entry->m_Command != nullptr) {
				return entry->m_Command->Invoke(args);
			}

			if (args.empty()) {
				return DescribeCVar(*entry->m_Desc);
			}

			return SetValue(*entry->m_Desc, args.front());
		}

		std::expected<void, CoriError<>> Console::SaveArchive() {
			std::map<std::string, std::string> values;

			for (const CVarBlock& block : Blocks()) {
				for (const CVarDesc& desc : block.m_Schema) {
					if (!HasFlag(desc.m_Flags, CVarFlags::eArchive)) {
						continue;
					}

					const Byte* source = desc.m_Tier == ApplyTier::eLive ? block.m_Live : block.m_Pending.data();
					const double value = ReadField(source, desc);

					if (value == desc.m_Default) {
						continue;
					}

					values.emplace(std::string(desc.m_Name), FormatRaw(desc, value));
				}
			}

			const std::filesystem::path path = ArchivePath();

			std::error_code createError;
			std::filesystem::create_directories(path.parent_path(), createError);
			if (createError) {
				return std::unexpected(CoriError<>(std::format("Could not create '{}': {}", path.parent_path().string(), createError.message())));
			}

			const auto written = glz::write<glz::opts{ .prettify = true }>(values);
			if (!written) {
				return std::unexpected(CoriError<>(std::format("Could not serialize the settings archive: {}", glz::format_error(written.error()))));
			}

			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			if (!file) {
				return std::unexpected(CoriError<>(std::format("Could not open '{}' for writing.", path.string())));
			}

			file.write(written->data(), static_cast<std::streamsize>(written->size()));
			if (!file) {
				return std::unexpected(CoriError<>(std::format("Could not write '{}'.", path.string())));
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "Saved {} non default settings to '{}'.", values.size(), path.string());
			return {};
		}

		std::expected<void, CoriError<>> Console::LoadArchive() {
			const std::filesystem::path path = ArchivePath();
			if (!std::filesystem::exists(path)) {
				return {};
			}

			const std::string buffer = FileSystem::FileManager::ReadTextFile(path);
			if (buffer.empty()) {
				return {};
			}

			std::map<std::string, std::string> values;
			if (const auto parseError = glz::read_json(values, buffer)) {
				return std::unexpected(CoriError<>(std::format("Could not parse '{}': {}", path.string(), glz::format_error(parseError, buffer))));
			}

			uint64_t applied = 0;
			for (const auto& [name, text] : values) {
				const LookupEntry* entry = FindEntry(name);
				if (entry == nullptr || entry->m_Desc == nullptr) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "The settings archive holds '{}', which no longer exists. It is ignored and will be dropped on the next save.", name);
					continue;
				}

				const CVarDesc& desc = *entry->m_Desc;
				const std::optional<double> parsed = ParseRaw(desc, text);
				if (!parsed) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "The settings archive holds '{}' for '{}', which is not a value it accepts. The default is kept.", text, name);
					continue;
				}

				const double value = desc.m_HasRange ? std::clamp(*parsed, desc.m_Min, desc.m_Max) : *parsed;

				CVarBlock& block = Blocks()[entry->m_Block];
				WriteField(block.m_Live, desc, value);
				WriteField(block.m_Pending.data(), desc, value);
				++applied;
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Console }, "Applied {} of {} settings from '{}'.", applied, values.size(), path.string());
			return {};
		}
	}
}

namespace {
	using namespace Cori;
	using namespace Cori::Core;

	CORI_CONSOLE_COMMAND(Help, "help", "help [name] - lists every command and variable, or explains one of them") {
		if (!args.empty()) {
			const std::string_view name = args.front();

			if (const CVarDesc* desc = Console::Find(name)) {
				return DescribeCVar(*desc);
			}

			std::string found;
			ConsoleCommand::ForEach([&found, name](const ConsoleCommand& command) {
				if (found.empty() && EqualsIgnoreCase(command.GetName(), name)) {
					found = std::format("{} - {}", command.GetName(), command.GetHelp());
				}
			});

			if (found.empty()) {
				return std::unexpected(std::format("Nothing is registered under '{}'.{}", name, SuggestionFor(name)));
			}
			return found;
		}

		std::vector<std::string> lines;
		ConsoleCommand::ForEach([&lines](const ConsoleCommand& command) {
			lines.push_back(std::format("  {}", command.GetHelp()));
		});
		std::ranges::sort(lines);

		std::string text = std::format("{} commands:", lines.size());
		for (const std::string& line : lines) {
			text += "\n" + line;
		}

		uint64_t cvarCount = 0;
		Console::ForEachCVar([&cvarCount](const CVarDesc&) { ++cvarCount; });
		text += "\n" + std::format("  {} variables, use `find <text>` to search them", cvarCount);
		return text;
	}

	CORI_CONSOLE_COMMAND(Find, "find", "find <text> - lists every command and variable whose name or help contains the text") {
		if (args.empty()) {
			return std::unexpected("find needs something to search for, e.g. `find shadow`.");
		}

		const std::string_view needle = args.front();

		const auto contains = [needle](const std::string_view haystack) {
			if (needle.size() > haystack.size()) {
				return false;
			}
			for (uint64_t i = 0; i + needle.size() <= haystack.size(); ++i) {
				if (EqualsIgnoreCase(haystack.substr(i, needle.size()), needle)) {
					return true;
				}
			}
			return false;
		};

		std::vector<std::string> lines;

		Console::ForEachCVar([&lines, &contains](const CVarDesc& desc) {
			if (contains(desc.m_Name) || contains(desc.m_Help)) {
				lines.push_back(std::format("  {} = {}", desc.m_Name, Console::FormatValue(desc)));
			}
		});

		ConsoleCommand::ForEach([&lines, &contains](const ConsoleCommand& command) {
			if (contains(command.GetName()) || contains(command.GetHelp())) {
				lines.push_back(std::format("  {}", command.GetHelp()));
			}
		});

		if (lines.empty()) {
			return std::format("Nothing matches '{}'.", needle);
		}

		std::ranges::sort(lines);
		std::string text = std::format("{} matches for '{}':", lines.size(), needle);
		for (const std::string& line : lines) {
			text += "\n" + line;
		}
		return text;
	}

	CORI_CONSOLE_COMMAND(Reset, "reset", "reset <variable> - puts a variable back to the default baked into its declaration") {
		if (args.empty()) {
			return std::unexpected("reset needs a variable, e.g. `reset r.ShadowBias`.");
		}

		const CVarDesc* desc = Console::Find(args[0]);
		if (desc == nullptr) {
			return std::unexpected(std::format("There is no variable called '{}'.{}", args[0], SuggestionFor(args[0])));
		}
		return Console::ResetValue(*desc);
	}

	CORI_CONSOLE_COMMAND(Save, "cvar.save", "cvar.save - writes every archived variable that differs from its default to the user settings") {
		if (const std::expected<void, CoriError<>> saved = Console::SaveArchive(); !saved) {
			return std::unexpected(saved.error().what());
		}
		return "Settings saved.";
	}

	CORI_CONSOLE_COMMAND(Load, "cvar.load", "cvar.load - re-reads the user settings from disk") {
		if (const std::expected<void, CoriError<>> loaded = Console::LoadArchive(); !loaded) {
			return std::unexpected(loaded.error().what());
		}
		return "Settings loaded.";
	}

	CORI_CONSOLE_COMMAND(Cheats, "cheats", "cheats [0|1] - reports or sets whether cheat protected variables may be written") {
		if (args.empty()) {
			return std::format("Cheats are {}.", Console::GetCheatsEnabled() ? "enabled" : "disabled");
		}

		const bool enable = args[0] == "1" || EqualsIgnoreCase(args[0], "true") || EqualsIgnoreCase(args[0], "on");
		Console::SetCheatsEnabled(enable);
		return std::format("Cheats are now {}.", enable ? "enabled" : "disabled");
	}

	CORI_CONSOLE_COMMAND(LogLevel, "log.level", "log.level <tag> <level> - raises or lowers the severity floor of one log category") {
		if (args.size() < 2) {
			return std::unexpected("log.level needs a tag and a level, e.g. `log.level \"Render Graph\" warn`. Use `log.tags` to list them.");
		}

		const LogTag* tag = FindLogTag(args[0]);
		if (tag == nullptr) {
			return std::unexpected(std::format("There is no log category called '{}'. Use `log.tags` to list them.", args[0]));
		}

		const std::optional<LogLevel> level = ParseLogLevel(args[1]);
		if (!level) {
			return std::unexpected(std::format("'{}' is not a level. Use one of trace, debug, info, warn, error, fatal, off.", args[1]));
		}

		tag->SetFloor(*level);
		return std::format("'{}' now drops anything below {}.", tag->GetName(), LogLevelName(*level));
	}

	CORI_CONSOLE_COMMAND(LogLevelAll, "log.level.all", "log.level.all <level> - sets the severity floor of every log category at once") {
		if (args.empty()) {
			return std::unexpected("log.level.all needs a level, e.g. `log.level.all warn`.");
		}

		const std::optional<LogLevel> level = ParseLogLevel(args[0]);
		if (!level) {
			return std::unexpected(std::format("'{}' is not a level. Use one of trace, debug, info, warn, error, fatal, off.", args[0]));
		}

		LogTag::SetAllFloors(*level);
		return std::format("Every category now drops anything below {}.", LogLevelName(*level));
	}

	CORI_CONSOLE_COMMAND(LogTags, "log.tags", "log.tags - lists every log category and the severity floor it is currently set to") {
		std::vector<std::string> lines;
		LogTag::ForEach([&lines](const LogTag& tag) {
			lines.push_back(std::format("  {:<28} {}", tag.GetName(), LogLevelName(tag.GetFloor())));
		});
		std::ranges::sort(lines);

		std::string text = std::format("{} log categories:", lines.size());
		for (const std::string& line : lines) {
			text += "\n" + line;
		}
		return text;
	}

	CORI_CONSOLE_COMMAND(Clear, "clear", "clear - empties the console view, the captured log itself is untouched") {
		s_ClearRequested = true;
		return std::string{};
	}
}
