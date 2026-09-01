#include "ContentBrowser.hpp"
#include "GlobalFuncs.hpp"
#include "imgui_internal.h"
#include "LogTags.hpp"
#include "Core/Application.hpp"
#include "Utility/ImGuiHelpers.hpp"
#include "Utility/ImGuiScale.hpp"

namespace {
	constexpr std::string_view s_JsonExtension{ ".json" };

	constexpr uint64_t s_MeshTypeHash = Cori::Utility::HashString64(std::meta::identifier_of(^^Cori::Graphics::Mesh));
	constexpr uint64_t s_MaterialTypeHash = Cori::Utility::HashString64(std::meta::identifier_of(^^Cori::Graphics::Material));
	constexpr uint64_t s_TextureTypeHash = Cori::Utility::HashString64(std::meta::identifier_of(^^Cori::Graphics::Texture2));

	struct TypeDisplay {
		uint64_t hash;
		const char* name;
		ImU32 accent;
	};

	const std::array s_TypeDisplays{
		TypeDisplay{ Cori::Utility::HashString64("Mesh"), "Mesh", IM_COL32(0, 175, 255, 255) },
		TypeDisplay{ Cori::Utility::HashString64("Material"), "Material", IM_COL32(80, 200, 120, 255) },
		TypeDisplay{ Cori::Utility::HashString64("Texture2"), "Texture", IM_COL32(240, 100, 90, 255) },
		TypeDisplay{ Cori::Utility::HashString64("ShaderEffect"), "Shader Effect", IM_COL32(200, 140, 240, 255) },
		TypeDisplay{ Cori::Utility::HashString64("VertFragShaderPair"), "Shader", IM_COL32(240, 190, 70, 255) },
		TypeDisplay{ Cori::Utility::HashString64("ComputeShader"), "Compute Shader", IM_COL32(240, 150, 70, 255) }
	};

	const char* TypeName(const uint64_t typeHash) {
		for (const TypeDisplay& display : s_TypeDisplays) {
			if (display.hash == typeHash) {
				return display.name;
			}
		}

		return "Asset";
	}

	ImU32 TypeAccent(const uint64_t typeHash) {
		for (const TypeDisplay& display : s_TypeDisplays) {
			if (display.hash == typeHash) {
				return display.accent;
			}
		}

		return IM_COL32(150, 150, 150, 255);
	}
}

namespace Snowflake {
	ContentBrowserWatcher::ContentBrowserWatcher() {
		m_Thread = std::thread(&ContentBrowserWatcher::ScanThreadMain, this);
	}

	ContentBrowserWatcher::~ContentBrowserWatcher() {
		{
			std::lock_guard lock(m_RequestMutex);
			m_Stop = true;
		}

		m_Wake.notify_all();
		m_Thread.join();

		m_Watcher.reset();
	}

	void ContentBrowserWatcher::handleFileAction(efsw::WatchID, const std::string& dir, const std::string& filename, efsw::Action, const std::string& oldFilename) {
		MarkDirty(std::filesystem::path(dir) / filename);

		if (!oldFilename.empty()) {
			MarkDirty(std::filesystem::path(dir) / oldFilename);
		}
	}

	void ContentBrowserWatcher::handleMissedFileActions(efsw::WatchID, const std::string& dir) {
		CORI_WARN_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ContentBrowser }, "The file watcher dropped events under '{}', falling back to a full rescan.", dir);
		RequestFullScan();
	}

	void ContentBrowserWatcher::MarkDirty(const std::filesystem::path& path) {
		{
			std::lock_guard lock(m_RequestMutex);
			m_DirtyPaths.emplace_back(path.native());
		}

		m_Wake.notify_one();
	}

	void ContentBrowserWatcher::RequestFullScan() {
		{
			std::lock_guard lock(m_RequestMutex);
			m_ForceFullScan = true;
		}

		m_Wake.notify_one();
	}

	void ContentBrowserWatcher::RequestRebuild() {
		{
			std::lock_guard lock(m_RequestMutex);
			m_ForceRebuild = true;
		}

		m_Wake.notify_one();
	}

	void ContentBrowserWatcher::SelectFolder(std::string virtualDir) {
		{
			std::lock_guard lock(m_RequestMutex);
			m_RequestedSelection = std::move(virtualDir);
			m_SelectionChanged = true;
		}

		m_Wake.notify_one();
	}

	std::shared_ptr<const ContentBrowserWatcher::View> ContentBrowserWatcher::GetView() const {
		return m_View.load(std::memory_order_acquire);
	}

	void ContentBrowserWatcher::ScanThreadMain() {
		Cori::SetThreadName("ContentBrowser");

		auto nextFullScan = std::chrono::steady_clock::now();

		while (true) {
			bool fullScan = false;
			bool rebuild = false;
			bool republish = false;
			bool watcherChanged = false;

			{
				std::unique_lock lock(m_RequestMutex);

				auto deadline = nextFullScan;
				if (!m_Queue.empty()) {
					deadline = std::min(deadline, std::chrono::steady_clock::now() + s_ValidationPollInterval);
				}

				m_Wake.wait_until(lock, deadline, [this] {
					return m_Stop || m_ForceFullScan || m_ForceRebuild || m_SelectionChanged || !m_DirtyPaths.empty();
				});

				if (m_Stop) {
					return;
				}

				fullScan = std::chrono::steady_clock::now() >= nextFullScan || m_ForceFullScan;
				rebuild = m_ForceRebuild;
				m_ForceFullScan = false;
				m_ForceRebuild = false;

				m_PassDirtyPaths.clear();
				m_PassDirtyPaths.swap(m_DirtyPaths);

				if (m_SelectionChanged) {
					m_ActiveSelection = m_RequestedSelection;
					m_SelectionChanged = false;
					republish = true;
				}
			}

			m_Stats.visited = 0;
			m_Stats.listed = 0;
			m_Stats.foldersAdded = 0;
			m_Stats.foldersRemoved = 0;
			m_Stats.entriesAdded = 0;
			m_Stats.entriesRemoved = 0;
			m_Stats.validated = 0;
			m_Stats.timedOut = 0;
			m_Stats.retried = 0;

			if (watcherChanged && !m_UseWatcher) {
				m_Watcher.reset();
				fullScan = true;
			}

			if (rebuild) {
				m_Folders.clear();
				m_Roots.clear();
				m_Queue.clear();
				m_Unresolved.clear();
				m_EntryCount = 0;
				m_Watcher.reset();
				fullScan = true;
			}

			if (fullScan) {
				FullPass();
				float seconds = m_Watcher != nullptr ? m_WatchedScanInterval : m_ScanInterval;
				nextFullScan = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(std::max(seconds, 0.05f)));
			}
			else if (!m_PassDirtyPaths.empty()) {
				TargetedPass();
			}

			bool retried = RetryUnresolved();
			bool confirmed = ProcessQueue();

			m_Stats.watching = m_Watcher != nullptr;

			if (fullScan || !m_PassDirtyPaths.empty() || republish || retried || confirmed || m_View.load(std::memory_order_acquire) == nullptr) {
				PublishView();
			}
		}
	}

	void ContentBrowserWatcher::FullPass() {
		CORI_PROFILE_FUNCTION();

		auto start = std::chrono::steady_clock::now();

		Cori::Core::AssetManager2::ForEachAssetDir([this](const Cori::Core::AssetDir& dir) {
			for (const auto& id : m_Roots | std::views::values) {
				if (id == dir.id) {
					return;
				}
			}

			if (dir.dir.empty()) {
				return;
			}

			Folder& root = CreateFolder(nullptr, dir.label, dir.dir, dir.label + "://");
			m_Roots.emplace_back(&root, dir.id);
		});

		if (m_UseWatcher && m_Watcher == nullptr && !m_Roots.empty()) {
			m_Watcher = std::make_unique<efsw::FileWatcher>();

			bool watching = false;
			for (const auto& folder : m_Roots | std::views::keys) {
				if (m_Watcher->addWatch(folder->path.string(), this, true) >= 0) {
					watching = true;
					continue;
				}

				CORI_WARN_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ContentBrowser }, "Could not watch '{}', it will only be picked up by the periodic rescan.", folder->path.string());
			}

			if (watching) {
				m_Watcher->watch();
			}
			else {
				m_Watcher.reset();
			}
		}

		for (const auto& folder : m_Roots | std::views::keys) {
			ScanDirectory(*folder, 0, true);
		}

		m_Stats.folders = m_Folders.size();
		m_Stats.entries = m_EntryCount;
		m_Stats.lastPassMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		m_Stats.lastPassTargeted = false;
		m_Stats.passes++;
	}

	void ContentBrowserWatcher::TargetedPass() {
		CORI_PROFILE_FUNCTION();

		auto start = std::chrono::steady_clock::now();

		m_Targets.clear();

		for (const std::string& path : m_PassDirtyPaths) {
			std::string virtualDir;
			if (!ToVirtualDir(path, virtualDir)) {
				continue;
			}

			Folder* folder = ResolveFolder(virtualDir);
			if (folder == nullptr) {
				continue;
			}

			for (const Folder* target : { static_cast<const Folder*>(folder), static_cast<const Folder*>(folder->parent) }) {
				if (target != nullptr && std::ranges::find(m_Targets, target->virtualDir) == m_Targets.end()) {
					m_Targets.emplace_back(target->virtualDir);
				}
			}
		}

		for (const std::string& dir : m_Targets) {
			bool exact = false;
			Folder* folder = ResolveFolder(dir, &exact);
			if (!exact) {
				continue;
			}

			ScanDirectory(*folder, 0, false);
		}

		m_Stats.folders = m_Folders.size();
		m_Stats.entries = m_EntryCount;
		m_Stats.lastPassMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		m_Stats.lastPassTargeted = true;
		m_Stats.passes++;
	}

	ContentBrowserWatcher::Folder& ContentBrowserWatcher::CreateFolder(Folder* parent, std::string name, std::filesystem::path path, std::string virtualDir) {
		std::string key = virtualDir;
		Folder& folder = m_Folders.try_emplace(std::move(key)).first->second;

		folder.name = std::move(name);
		folder.path = std::move(path);
		folder.virtualDir = std::move(virtualDir);
		folder.parent = parent;

		if (parent != nullptr) {
			parent->children.emplace_back(&folder);
		}

		return folder;
	}

	ContentBrowserWatcher::Folder* ContentBrowserWatcher::FindChild(const Folder& parent, const std::string_view name) {
		ComposeVirtual(m_KeyScratch, parent, name);

		const auto it = m_Folders.find(std::string_view(m_KeyScratch));
		return it == m_Folders.end() ? nullptr : &it->second;
	}

	void ContentBrowserWatcher::ScanDirectory(Folder& folder, const uint32_t depth, const bool recurseAll) {
		m_Stats.visited++;

		GetScratch(depth).newFolders.clear();

		bool changed = true;

		if (m_TimestampCheck) {
			std::error_code error;
			const auto stamp = last_write_time(folder.path, error);
			if (error) {
				return;
			}

			changed = folder.stamp != stamp || folder.racy;
			folder.stamp = stamp;
			folder.racy = std::filesystem::file_time_type::clock::now() - stamp < s_RacyWindow;
		}

		if (changed && ReadListing(folder, depth)) {
			ReconcileChildren(folder, depth);
			ReconcileEntries(folder, depth);
		}

		Scratch& scratch = GetScratch(depth);

		if (!recurseAll) {
			for (Folder* child : scratch.newFolders) {
				ScanDirectory(*child, depth + 1, true);
			}
			return;
		}

		scratch.folders = folder.children;

		for (Folder* child : scratch.folders) {
			ScanDirectory(*child, depth + 1, true);
		}
	}

	bool ContentBrowserWatcher::ReadListing(const Folder& folder, const uint32_t depth) {
		CORI_PROFILE_FUNCTION();

		Scratch& scratch = GetScratch(depth);
		scratch.dirs.Reset();
		scratch.files.Reset();

		std::error_code error;
		std::filesystem::directory_iterator it(folder.path, error);
		if (error) {
			CORI_WARN_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ContentBrowser }, "Could not open '{}'. Error: {}", folder.path.string(), error.message());
			return false;
		}

		for (const std::filesystem::directory_iterator last; !error && it != last; it.increment(error)) {
			const std::filesystem::directory_entry& item = *it;

			std::error_code queryError;

			if (item.is_directory(queryError)) {
				scratch.dirs.Add(item.path().filename().native());
				continue;
			}

			if (!item.is_regular_file(queryError) || item.path().extension().native() != s_JsonExtension) {
				continue;
			}

			scratch.files.Add(item.path().filename().native());
		}

		m_Stats.listed++;
		return true;
	}

	void ContentBrowserWatcher::ReconcileChildren(Folder& folder, const uint32_t depth) {
		CORI_PROFILE_FUNCTION();

		Scratch& scratch = GetScratch(depth);

		scratch.present.clear();
		for (uint64_t i = 0; i < scratch.dirs.count; i++) {
			scratch.present.emplace(scratch.dirs[i]);
		}

		scratch.folders.clear();
		for (Folder* child : folder.children) {
			if (!scratch.present.contains(child->name)) {
				scratch.folders.emplace_back(child);
			}
		}

		for (Folder* doomed : scratch.folders) {
			DestroyRecursive(*doomed, depth + 1);
		}

		for (uint64_t i = 0; i < scratch.dirs.count; i++) {
			const std::string& name = scratch.dirs[i];

			if (FindChild(folder, name) != nullptr) {
				continue;
			}

			ComposeVirtual(m_KeyScratch, folder, name);

			Folder& child = CreateFolder(&folder, name, folder.path / name, m_KeyScratch);
			GetScratch(depth).newFolders.emplace_back(&child);
			m_Stats.foldersAdded++;
		}
	}

	void ContentBrowserWatcher::ReconcileEntries(Folder& folder, const uint32_t depth) {
		CORI_PROFILE_FUNCTION();

		Scratch& scratch = GetScratch(depth);

		scratch.present.clear();
		for (uint64_t i = 0; i < scratch.files.count; i++) {
			scratch.present.emplace(scratch.files[i]);
		}

		scratch.fileNames.clear();
		for (const auto& fileName : folder.entries | std::views::keys) {
			if (!scratch.present.contains(fileName)) {
				scratch.fileNames.emplace_back(fileName);
			}
		}

		for (const std::string& fileName : scratch.fileNames) {
			folder.entries.erase(fileName);
			m_EntryCount--;
			m_Stats.entriesRemoved++;
		}

		for (uint64_t i = 0; i < scratch.files.count; i++) {
			auto& fileName = scratch.files[i];

			if (folder.entries.contains(fileName)) {
				continue;
			}

			ComposeVirtual(m_PathScratch, folder, fileName);

			Entry entry;
			entry.id = Cori::Utility::HashString64(m_PathScratch);
			entry.label = fileName.substr(0, fileName.size() - s_JsonExtension.size());
			entry.virtualPath = m_PathScratch;

			folder.entries.emplace(fileName, std::move(entry));
			m_EntryCount++;
			m_Stats.entriesAdded++;

			m_Queue.emplace_back(Pending{ .folder = &folder, .fileName = fileName });
		}
	}

	void ContentBrowserWatcher::DestroyRecursive(Folder& folder, const uint32_t depth) {
		Scratch& scratch = GetScratch(depth);
		scratch.folders = folder.children;

		for (Folder* child : scratch.folders) {
			DestroyRecursive(*child, depth + 1);
		}

		std::erase_if(m_Queue, [target = &folder](const Pending& item) { return item.folder == target; });
		std::erase_if(m_Unresolved, [target = &folder](const Unresolved& item) { return item.folder == target; });

		if (folder.parent != nullptr) {
			std::erase(folder.parent->children, &folder);
		}

		m_EntryCount -= folder.entries.size();
		m_Stats.foldersRemoved++;

		m_Folders.erase(m_Folders.find(std::string_view(folder.virtualDir)));
	}

	bool ContentBrowserWatcher::RetryUnresolved() {
		if (m_Unresolved.empty()) {
			m_Stats.unresolved = 0;
			return false;
		}

		CORI_PROFILE_FUNCTION();

		auto now = std::filesystem::file_time_type::clock::now();

		std::erase_if(m_Unresolved, [this, now](const Unresolved& item) {
			const auto it = item.folder->entries.find(item.fileName);
			if (it == item.folder->entries.end()) {
				return true;
			}

			Entry& entry = it->second;
			if (entry.validated || !entry.timedOut) {
				return true;
			}

			std::error_code error;
			auto stamp = last_write_time(item.folder->path / item.fileName, error);
			if (error) {
				return true;
			}

			if (stamp == entry.stamp && now - stamp >= s_RacyWindow) {
				return false;
			}

			entry.stamp = stamp;
			entry.timedOut = false;
			m_Queue.emplace_back(Pending{ .folder = item.folder, .fileName = item.fileName });
			m_Stats.retried++;
			return true;
		});

		m_Stats.unresolved = m_Unresolved.size();
		return m_Stats.retried != 0;
	}

	bool ContentBrowserWatcher::ProcessQueue() {
		if (m_Queue.empty()) {
			m_Stats.pending = 0;
			return false;
		}

		CORI_PROFILE_FUNCTION();

		auto now = std::chrono::steady_clock::now();
		auto timeout = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(m_ValidationTimeout));

		uint64_t budget = m_Queue.size();

		while (budget-- > 0 && !m_Queue.empty()) {
			Pending item = std::move(m_Queue.front());
			m_Queue.pop_front();

			const auto it = item.folder->entries.find(item.fileName);
			if (it == item.folder->entries.end()) {
				continue;
			}

			Entry& entry = it->second;

			if (const auto vectorKey = Cori::Core::AssetManager2::GetAssetVectorKey(entry.id)) {
				entry.vectorKey = vectorKey.value();
				entry.validated = true;
				entry.timedOut = false;
				m_Stats.validated++;
				continue;
			}

			if (item.queuedAt == std::chrono::steady_clock::time_point{}) {
				item.queuedAt = now;
				m_Queue.emplace_back(std::move(item));
				continue;
			}

			if (now - item.queuedAt < timeout) {
				m_Queue.emplace_back(std::move(item));
				continue;
			}

			CORI_WARN_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ContentBrowser }, "'{}' is not a registered asset after {} seconds, parking it. The file is most likely not valid json, or its metadata failed to parse.", entry.virtualPath, m_ValidationTimeout);

			std::error_code error;
			entry.stamp = last_write_time(item.folder->path / item.fileName, error);
			entry.timedOut = true;
			m_Unresolved.emplace_back(Unresolved{ .folder = item.folder, .fileName = std::move(item.fileName) });
			m_Stats.timedOut++;
		}

		m_Stats.pending = m_Queue.size();
		m_Stats.unresolved = m_Unresolved.size();

		return m_Stats.validated != 0 || m_Stats.timedOut != 0;
	}

	ContentBrowserWatcher::Folder* ContentBrowserWatcher::ResolveFolder(const std::string_view virtualDir, bool* exact) {
		if (exact != nullptr) {
			*exact = false;
		}

		uint64_t scheme = virtualDir.find("://");
		if (scheme == std::string_view::npos) {
			return nullptr;
		}

		const std::string_view prefix = virtualDir.substr(0, scheme + 3);

		Folder* current = nullptr;
		for (const auto& root : m_Roots | std::views::keys) {
			if (root->virtualDir == prefix) {
				current = root;
				break;
			}
		}

		if (current == nullptr) {
			return nullptr;
		}

		std::string_view rest = virtualDir.substr(scheme + 3);

		while (!rest.empty()) {
			uint64_t slash = rest.find('/');
			const std::string_view component = rest.substr(0, slash);
			if (component.empty()) {
				break;
			}

			Folder* child = FindChild(*current, component);
			if (child == nullptr) {
				return current;
			}

			current = child;

			if (slash == std::string_view::npos) {
				break;
			}

			rest.remove_prefix(slash + 1);
		}

		if (exact != nullptr) {
			*exact = true;
		}

		return current;
	}

	bool ContentBrowserWatcher::ToVirtualDir(const std::filesystem::path& path, std::string& out) {
		const std::string& full = path.native();

		for (const auto& root : m_Roots | std::views::keys) {
			std::string_view base = root->path.native();
			while (base.size() > 1 && base.back() == '/') {
				base.remove_suffix(1);
			}

			if (!std::string_view(full).starts_with(base)) {
				continue;
			}

			uint64_t offset = base.size();
			if (offset < full.size()) {
				if (full[offset] != '/') {
					continue;
				}
				offset++;
			}

			out.assign(root->virtualDir);
			out.append(full, offset, std::string::npos);
			return true;
		}

		return false;
	}

	ContentBrowserWatcher::Scratch& ContentBrowserWatcher::GetScratch(const uint32_t depth) {
		while (m_Scratch.size() <= depth) {
			m_Scratch.emplace_back();
		}

		return m_Scratch[depth];
	}

	void ContentBrowserWatcher::ComposeVirtual(std::string& out, const Folder& parent, const std::string_view name) {
		out.assign(parent.virtualDir);
		if (parent.parent != nullptr) {
			out.push_back('/');
		}
		out.append(name);
	}

	void ContentBrowserWatcher::PublishView() {
		CORI_PROFILE_FUNCTION();

		auto view = std::make_shared<View>();

		for (const auto& root : m_Roots | std::views::keys) {
			AppendFolderRows(*view, *root, 0);
		}

		bool exact = false;
		const Folder* selected = m_ActiveSelection.empty() ? nullptr : ResolveFolder(m_ActiveSelection, &exact);

		if (exact && selected != nullptr) {
			view->selectedDir = m_ActiveSelection;
			view->entries.reserve(selected->entries.size());

			for (const auto& entry : selected->entries | std::views::values) {
				view->entries.emplace_back(View::EntryRow{
					.label = entry.label,
					.virtualPath = entry.virtualPath,
					.id = entry.id,
					.vectorKey = entry.vectorKey,
					.validated = entry.validated,
					.timedOut = entry.timedOut
				});
			}

			std::ranges::sort(view->entries, {}, &View::EntryRow::label);
		}

		view->stats = m_Stats;

		m_View.store(std::move(view), std::memory_order_release);
	}

	void ContentBrowserWatcher::AppendFolderRows(View& view, const Folder& folder, const uint32_t depth) {
		view.folders.emplace_back(View::FolderRow{
			.name = folder.name,
			.virtualDir = folder.virtualDir,
			.depth = depth,
			.childCount = static_cast<uint32_t>(folder.children.size())
		});

		std::vector<const Folder*> children(folder.children.begin(), folder.children.end());
		std::ranges::sort(children, {}, &Folder::name);

		for (const Folder* child : children) {
			AppendFolderRows(view, *child, depth + 1);
		}
	}

	ContentBrowser::ContentBrowser() : m_Thumbnails(ThumbnailCache::Get()), m_TileSize(static_cast<int32_t>(Cori::Utility::ScaleUIUnit(s_DefaultTileSize))), m_TreeWidth(Cori::Utility::ScaleUIUnit(s_DefaultTreeWidth)) {}

	void ContentBrowser::Draw(bool* open, const char* name) {
		CORI_PROFILE_FUNCTION();

		if (m_Thumbnails) {
			m_Thumbnails->Tick();
		}

		if (open != nullptr && !*open) {
			return;
		}

		ImGui::SetNextWindowSize({ Cori::Utility::ScaleUIUnit(s_DefaultWindowWidth), Cori::Utility::ScaleUIUnit(s_DefaultWindowHeight) }, ImGuiCond_FirstUseEver);

		if (!ImGui::Begin(name, open, ImGuiWindowFlags_NoCollapse)) {
			ImGui::End();
			return;
		}


		const std::shared_ptr<const ContentBrowserWatcher::View> view = m_Watcher.GetView();
		if (view.get() != m_LastSeen) {
			if (m_LastSelectedDir != m_SelectedDir) {
				m_ThumbnailHandles.clear();
				m_Thumbnails->ReleaseAll();
				m_LastSelectedDir = m_SelectedDir;
			}
			m_LastSeen = view.get();
		}

		if (view == nullptr) {
			ImGui::TextDisabled("Scanning...");
			ImGui::End();
			return;
		}

		DrawToolbar(*view);

		if (view->folders.empty()) {
			ImGui::TextDisabled("No asset directories are registered.");
			ImGui::End();
			return;
		}

		if (!m_TreeSeeded) {
			for (const ContentBrowserWatcher::View::FolderRow& row : view->folders) {
				if (row.depth == 0) {
					m_Expanded.insert(row.virtualDir);
				}
			}

			if (m_SelectedDir != view->folders.front().virtualDir) {
				m_SelectedDir = view->folders.front().virtualDir;
				m_Watcher.SelectFolder(m_SelectedDir);
			}

			m_TreeSeeded = true;
		}

		auto available = ImGui::GetContentRegionAvail();
		m_TreeWidth = std::clamp(m_TreeWidth, Cori::Utility::ScaleUIUnit(s_MinTreeWidth), std::max(Cori::Utility::ScaleUIUnit(s_MinTreeWidth), available.x - Cori::Utility::ScaleUIUnit(s_MinGridWidth)));

		float bodyHeight = std::max(available.y, 1.0f);

		DrawTree(*view, bodyHeight);

		ImGui::SameLine(0.0f, 0.0f);

		ImGui::InvisibleButton("##splitter", { Cori::Utility::ScaleUIUnit(s_SplitterWidth), bodyHeight });
		if (ImGui::IsItemActive()) {
			m_TreeWidth += ImGui::GetIO().MouseDelta.x;
		}
		if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}

		ImGui::SameLine(0.0f, 0.0f);

		DrawGrid(*view, bodyHeight);

		ImGui::End();
	}

	void ContentBrowser::OnUpdate(Cori::Core::GameTimer& gameTimer) {
		if (m_Thumbnails) {
			m_Thumbnails->OnUpdate(gameTimer);
		}
	}

	void ContentBrowser::OnTickUpdate(Cori::Core::GameTimer& gameTimer) {
		if (m_Thumbnails) {
			m_Thumbnails->OnTickUpdate(gameTimer);
		}
	}

	void ContentBrowser::DrawToolbar(const ContentBrowserWatcher::View& view) {
		ImGui::Text("%zu folders, %zu assets, %zu pending, %zu unresolved  |  %s", view.stats.folders, view.stats.entries, view.stats.pending, view.stats.unresolved, view.stats.watching ? "watching" : "polling");

		ImGui::SameLine();
		if (ImGui::Button("rescan")) {
			m_Watcher.RequestFullScan();
		}

		ImGui::SameLine();
		if (ImGui::Button("rebuild")) {
			m_Watcher.RequestRebuild();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(Cori::Utility::ScaleUIUnit(s_TileSizeSliderWidth));
		ImGui::SliderInt("##tilesize", &m_TileSize, static_cast<int32_t>(Cori::Utility::ScaleUIUnit(s_MinTileSize)), static_cast<int32_t>(Cori::Utility::ScaleUIUnit(s_MaxTileSize)), "%d px");

		ImGui::Text("last pass %.3f ms (%s), visited %zu, listed %zu", view.stats.lastPassMs, view.stats.lastPassTargeted ? "targeted" : "full", view.stats.visited, view.stats.listed);
		ImGui::Text("last pass changes: folders +%zu -%zu, assets +%zu -%zu, validated %zu, parked %zu, retried %zu", view.stats.foldersAdded, view.stats.foldersRemoved, view.stats.entriesAdded, view.stats.entriesRemoved, view.stats.validated, view.stats.timedOut, view.stats.retried);

		ImGui::Separator();
	}

	void ContentBrowser::DrawTree(const ContentBrowserWatcher::View& view, const float bodyHeight) {
		ImGui::BeginChild("##tree", ImVec2(m_TreeWidth, bodyHeight), ImGuiChildFlags_Borders);

		const ImU32 selectedColor = ImGui::GetColorU32(ImGuiCol_Header);
		const ImU32 heldColor = ImGui::GetColorU32(ImGuiCol_HeaderActive);
		const ImU32 hoveredColor = Cori::Utility::Fade(selectedColor, 0.35f);
		const float highlightRounding = ImGui::GetStyle().SelectableRounding;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->ChannelsSplit(2);
		drawList->ChannelsSetCurrent(1);

		ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32_BLACK_TRANS);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32_BLACK_TRANS);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32_BLACK_TRANS);

		uint32_t skipDepth = UINT32_MAX;
		int32_t counter = -1;
		uint32_t childrenFolderDepth = UINT32_MAX;
		uint32_t focusedFolderChildrenCount = UINT32_MAX;
		m_FocusedFolderIndex = UINT32_MAX;
		m_FocusedFolderChildren.clear();

		for (const auto& [name, virtualDir, depth, childCount] : view.folders) {
			counter++;
			if (depth > skipDepth) {
				continue;
			}

			if (depth == childrenFolderDepth && focusedFolderChildrenCount > m_FocusedFolderChildren.size()) {
				m_FocusedFolderChildren.emplace_back(counter);
				if (focusedFolderChildrenCount == m_FocusedFolderChildren.size()) {
					childrenFolderDepth = UINT32_MAX;
				}
			}

			if (depth > skipDepth - 1) {
				continue;
			}

			skipDepth = UINT32_MAX;

			bool leaf = childCount == 0;
			bool expanded = !leaf && m_Expanded.contains(virtualDir);

			float indent = static_cast<float>(depth) * ImGui::GetTreeNodeToLabelSpacing();
			if (indent > 0.0f) {
				ImGui::Indent(indent);
			}

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (leaf) {
				flags |= ImGuiTreeNodeFlags_Leaf;
			}
			if (virtualDir == m_SelectedDir) {
				m_FocusedFolderIndex = counter;
				childrenFolderDepth = depth + 1;
				focusedFolderChildrenCount = childCount;
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::PushID(virtualDir.c_str());
			ImGui::SetNextItemOpen(expanded);

			bool open = ImGui::TreeNodeEx(name.c_str(), flags);

			const bool selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;
			const bool hovered = ImGui::IsItemHovered();
			if (selected || hovered) {
				const ImU32 highlight = selected ? (ImGui::IsItemActive() ? heldColor : selectedColor) : hoveredColor;

				drawList->ChannelsSetCurrent(0);
				drawList->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), highlight, highlightRounding);
				drawList->ChannelsSetCurrent(1);
			}

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				if (m_SelectedDir != virtualDir) {
					m_SelectedDir = virtualDir;
					m_Watcher.SelectFolder(m_SelectedDir);
				}
			}

			ImGui::PopID();

			if (indent > 0.0f) {
				ImGui::Unindent(indent);
			}

			if (!leaf) {
				if (open != expanded) {
					if (open) {
						m_Expanded.insert(virtualDir);
					}
					else {
						m_Expanded.erase(virtualDir);
					}
				}

				if (!open) {
					skipDepth = depth + 1;
				}
			}
		}

		ImGui::PopStyleColor(3);
		drawList->ChannelsMerge();

		ImGui::EndChild();
	}

	float ContentBrowser::CardHeight() const {
		return m_TileSize + ImGui::GetTextLineHeight() * s_LabelLines + Cori::Utility::ScaleUIUnit(s_CardPadding) * 3.0f;
	}

	uint32_t ContentBrowser::DrawGrid(const ContentBrowserWatcher::View& view, const float bodyHeight) {
		ImGui::BeginGroup();

		float pathHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
		float gridHeight = std::max(bodyHeight - pathHeight - ImGui::GetStyle().ItemSpacing.y, 1.0f);

		ImGui::BeginChild("##path", ImVec2(0.0f, pathHeight), ImGuiChildFlags_Borders);

		if (view.selectedDir.empty()) {
			ImGui::TextDisabled("Nothing selected.");
			ImGui::EndChild();

			ImGui::BeginChild("##grid", ImVec2(0.0f, gridHeight), ImGuiChildFlags_Borders);
			ImGui::EndChild();

			ImGui::EndGroup();
			return 0;
		}

		ImGui::TextUnformatted(view.selectedDir.c_str());
		ImGui::EndChild();

		ImGui::BeginChild("##grid", ImVec2(0.0f, gridHeight), ImGuiChildFlags_Borders);

		ImVec2 surfaceOrigin = ImGui::GetWindowPos();
		ImVec2 surfaceSize = ImGui::GetWindowSize();

		ImGuiStyle& style = ImGui::GetStyle();

		float cardWidth = m_TileSize + Cori::Utility::ScaleUIUnit(s_CardPadding) * 2.0f;
		float itemWidth = cardWidth + style.ItemSpacing.x;
		float itemHeight = CardHeight() + style.ItemSpacing.y;

		uint32_t childrenFolderCount = 0;

		if (m_FocusedFolderIndex != UINT32_MAX) {
			childrenFolderCount = view.folders[m_FocusedFolderIndex].childCount;
		}

		uint32_t total = childrenFolderCount + view.entries.size();
		uint32_t columns = static_cast<uint32_t>(std::max(1.0f, std::floor(ImGui::GetContentRegionAvail().x / itemWidth)));
		uint32_t rows = (total + columns - 1) / columns;
		constexpr float fbScale = 1.0f;

		float baseX = ImGui::GetCursorPosX();
		float baseY = ImGui::GetCursorPosY();

		float scroll = ImGui::GetScrollY();

		uint32_t firstRow = static_cast<uint32_t>(std::max(0.0f, std::floor(scroll / itemHeight) - 1.0f));
		uint32_t lastRow = std::min(rows, static_cast<uint32_t>(std::ceil((scroll + surfaceSize.y) / itemHeight)) + 1);

		for (uint32_t row = firstRow; row < lastRow; row++) {
			for (uint32_t column = 0; column < columns; column++) {
				uint32_t item = row * columns + column;
				if (item >= total) {
					break;
				}

				ImGui::SetCursorPos(ImVec2(baseX + static_cast<float>(column) * itemWidth, baseY + static_cast<float>(row) * itemHeight));

				ImVec2 origin = ImGui::GetCursorScreenPos();

				if (item < childrenFolderCount) {
					DrawFolder(view.folders[m_FocusedFolderChildren[item]], view.folders[m_FocusedFolderIndex], origin, static_cast<float>(m_TileSize), fbScale);
					continue;
				}

				DrawAssetTile(view.entries[item - childrenFolderCount], origin, static_cast<float>(m_TileSize), fbScale, surfaceOrigin);
			}
		}

		ImGui::SetCursorPos(ImVec2(baseX, baseY + static_cast<float>(rows) * itemHeight));
		ImGui::Dummy(ImVec2(1.0f, 1.0f));

		ImGui::EndChild();
		ImGui::EndGroup();

		return total;
	}

	void ContentBrowser::DrawCard(const ImVec2 origin, const float thumbnail, const bool selected, const bool hovered, const ImU32 selectedColor, const ImU32 hoveredColor) const {
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImVec2 max(origin.x + thumbnail + Cori::Utility::ScaleUIUnit(s_CardPadding) * 2.0f, origin.y + CardHeight());

		ImGuiStyle& style = ImGui::GetStyle();

		ImU32 edging = hovered ? hoveredColor : 0x00000000;

		if (selected) {
			drawList->AddRect(origin, max, selectedColor, style.WindowRounding, 2.0f);
		} else {
			drawList->AddRect(origin, max, edging, style.WindowRounding, 2.0f);
		}
	}

	void ContentBrowser::DrawLabel(const ImVec2 origin, const float thumbnail, const char* text) {
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		const float cardPadding = Cori::Utility::ScaleUIUnit(s_CardPadding);

		float wrapWidth = thumbnail;
		float top = origin.y + thumbnail + cardPadding * 2.0f;

		ImVec2 size = ImGui::CalcTextSize(text, nullptr, false, wrapWidth);
		ImVec2 position(origin.x + cardPadding + std::max(0.0f, (wrapWidth - size.x) * 0.5f), top);

		drawList->PushClipRect(ImVec2(origin.x, top), ImVec2(origin.x + thumbnail + cardPadding * 2.0f, top + ImGui::GetTextLineHeight() * s_LabelLines), true);
		drawList->AddText(nullptr, 0.0f, position, ImGui::GetColorU32(ImGuiCol_Text), text, nullptr, wrapWidth);
		drawList->PopClipRect();
	}

	void ContentBrowser::DrawFolder(const ContentBrowserWatcher::View::FolderRow& folder, const ContentBrowserWatcher::View::FolderRow& parent, const ImVec2 origin, const float thumbnailSize, float fbScale) {
		ImGui::PushID(folder.virtualDir.c_str());

		const float cardPadding = Cori::Utility::ScaleUIUnit(s_CardPadding);

		ImGui::InvisibleButton("##tile", ImVec2(thumbnailSize + cardPadding * 2.0f, CardHeight()));

		bool hovered = ImGui::IsItemHovered();
		bool activated = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hovered;

		DrawCard(origin, thumbnailSize, false, hovered, ImGui::GetColorU32(ImGuiCol_CheckMark), Cori::Utility::Fade(ImGui::GetColorU32(ImGuiCol_CheckMark), 0.6f));

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		const float width = thumbnailSize * 0.82f;
		const float height = width * 0.92f;

		const float left = origin.x + cardPadding + (thumbnailSize - width) * 0.5f;
		const float top = origin.y + cardPadding + (thumbnailSize - height) * 0.5f;
		const float right = left + width;
		const float bottom = top + height;

		const float rounding = width * 0.075f;
		const float tabWidth = width * 0.35f;
		const float tabHeight = height * 0.12f;
		const float tabSlant = width * 0.15f;
		const float tabShoulder = width * 0.05f;
		const float tabFillet = width * 0.05f;

		const float slantLength = std::sqrt(tabSlant * tabSlant + tabHeight * tabHeight);
		const float slantX = tabSlant / slantLength;
		const float slantY = tabHeight / slantLength;

		const ImVec2 tabCorner(left + tabWidth, top);
		const ImVec2 bodyCorner(left + tabWidth + tabSlant, top + tabHeight);

		const float sheetTop = top + height * 0.24f;
		const float sheetInset = width * 0.10f;
		const float frontTop = top + height * 0.335f;

		ImU32 folderColor = ImGui::GetColorU32(ImGuiCol_CheckMark);

		if (hovered) {
			folderColor = Cori::Utility::Shade(folderColor, 1.12f);
		}

		const ImU32 backColor = Cori::Utility::Shade(folderColor, 0.38f);
		const ImU32 sheetColor = Cori::Utility::Hex32ToImU32(0xf2f0eeFF);

		drawList->PathClear();
		drawList->PathArcTo(ImVec2(left + rounding, top + rounding), rounding, IM_PI, IM_PI * 1.5f);
		drawList->PathLineTo(ImVec2(tabCorner.x - tabShoulder, tabCorner.y));
		drawList->PathBezierQuadraticCurveTo(tabCorner, ImVec2(tabCorner.x + slantX * tabShoulder, tabCorner.y + slantY * tabShoulder));
		drawList->PathLineTo(ImVec2(bodyCorner.x - slantX * tabFillet, bodyCorner.y - slantY * tabFillet));
		drawList->PathBezierQuadraticCurveTo(bodyCorner, ImVec2(bodyCorner.x + tabFillet, bodyCorner.y));
		drawList->PathArcTo(ImVec2(right - rounding, top + tabHeight + rounding), rounding, IM_PI * 1.5f, IM_PI * 2.0f);
		drawList->PathArcTo(ImVec2(right - rounding, bottom - rounding), rounding, 0.0f, IM_PI * 0.5f);
		drawList->PathArcTo(ImVec2(left + rounding, bottom - rounding), rounding, IM_PI * 0.5f, IM_PI);
		drawList->PathFillConcave(backColor);

		drawList->AddRectFilled(ImVec2(left + sheetInset, sheetTop), ImVec2(right - sheetInset, frontTop + rounding), sheetColor, width * 0.078f, ImDrawFlags_RoundCornersTop);

		drawList->AddRectFilled(ImVec2(left, frontTop), ImVec2(right, bottom), folderColor, rounding);

		DrawLabel(origin, thumbnailSize, folder.name.c_str());

		ImGui::PopID();

		if (activated) {
			if (m_SelectedDir != folder.virtualDir) {
				m_SelectedDir = folder.virtualDir;
				m_Watcher.SelectFolder(m_SelectedDir);
			}
			m_ThumbnailHandles.clear();
			m_Thumbnails->ReleaseAll();
			if (!m_Expanded.contains(folder.virtualDir) && folder.childCount > 0) {
				m_Expanded.insert(folder.virtualDir);
			}

			if (!m_Expanded.contains(parent.virtualDir) && parent.childCount > 0) {
				m_Expanded.insert(parent.virtualDir);
			}
		}
	}

	void ContentBrowser::DrawAssetTile(const ContentBrowserWatcher::View::EntryRow& entry, const ImVec2 origin, const float thumbnailSize, float fbScale, const ImVec2 surfaceOrigin) {
		ImGui::PushID(entry.virtualPath.c_str());

		const float cardPadding = Cori::Utility::ScaleUIUnit(s_CardPadding);


		ImGui::InvisibleButton("##tile", ImVec2(thumbnailSize + cardPadding * 2.0f, CardHeight()));

		bool hovered = ImGui::IsItemHovered();

		if (ImGui::IsItemClicked()) {
			m_SelectedAsset = entry.id;
		}

		bool drewPreview = false;
		ImU32 accent;

		uint64_t hash = UINT64_MAX;
		const char* typeName = "Undefined";
		const char* status = "Valid";

		if (entry.validated) {
			hash = Cori::Core::AssetManager2::GetAssetTypeHash(entry.vectorKey);
			accent = TypeAccent(hash);
			typeName = TypeName(hash);
		} else {
			if (entry.timedOut) {
				accent = Cori::Utility::Hex32ToImU32(0x9e9e9eFF);
				status = "Unresolved";
			} else {
				accent = Cori::Utility::Hex32ToImU32(0xff8f00FF);
				status = "Pending";
			}
		}

		DrawCard(origin, thumbnailSize, entry.id == m_SelectedAsset && entry.validated, hovered, accent, Cori::Utility::Fade(accent, 0.6f));

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImVec2 thumbMin(origin.x + cardPadding, origin.y + cardPadding);
		ImVec2 thumbMax(thumbMin.x + thumbnailSize, thumbMin.y + thumbnailSize);

		const bool previewable = entry.validated && (hash == s_MeshTypeHash || hash == s_MaterialTypeHash || hash == s_TextureTypeHash);

		if (previewable && m_Thumbnails) {
			auto it = m_ThumbnailHandles.find(entry.id);
			if (it == m_ThumbnailHandles.end()) {
				ThumbnailHandle requested = ThumbnailCache::GetInvalidHandle();

				if (hash == s_MeshTypeHash) {
					requested = m_Thumbnails->RequestMesh(
						Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>(entry.virtualPath.c_str()),
						static_cast<uint32_t>(std::lround(std::max(thumbnailSize * fbScale, 1.0f))));
				}
				else if (hash == s_MaterialTypeHash) {
					requested = m_Thumbnails->RequestMaterial(
						Cori::Core::AssetManager2::Load<Cori::Graphics::Material>(entry.virtualPath.c_str()),
						static_cast<uint32_t>(std::lround(std::max(thumbnailSize * fbScale, 1.0f))));
				}
				else {
					requested = m_Thumbnails->RequestTexture(
						Cori::Core::AssetManager2::Load<Cori::Graphics::Texture2>(entry.virtualPath.c_str()));
				}

				if (requested != ThumbnailCache::GetInvalidHandle()) {
					it = m_ThumbnailHandles.try_emplace(entry.id, requested).first;
				}
			}

			if (it != m_ThumbnailHandles.end()) {
				m_Thumbnails->Resize(it->second, static_cast<uint32_t>(std::lround(std::max(thumbnailSize * fbScale, 1.0f))));

				if (const auto placement = m_Thumbnails->TryGetPlacement(it->second)) {
					drawList->AddImage(placement->texture, thumbMin, thumbMax, placement->uv0, placement->uv1);
					drewPreview = true;
				}
			}
		}

		if (!drewPreview) {
			ImVec2 center((thumbMin.x + thumbMax.x) * 0.5f, (thumbMin.y + thumbMax.y) * 0.5f);
			float radius = thumbnailSize * 0.22f;

			drawList->AddRectFilled(ImVec2(center.x - radius, center.y - radius), ImVec2(center.x + radius, center.y + radius), Cori::Utility::Fade(accent, 0.35f), 4.0f);
			drawList->AddRect(ImVec2(center.x - radius, center.y - radius), ImVec2(center.x + radius, center.y + radius), accent, 4.0f, 0, 1.5f);
		}

		drawList->AddRectFilled(ImVec2(thumbMin.x, thumbMax.y - 3.0f), thumbMax, accent);

		DrawLabel(origin, thumbnailSize, entry.label.c_str());

		if (hovered) {
			ImGui::SetTooltip("%s\n%s\n%s", entry.virtualPath.c_str(), typeName, status);
		}

		ImGui::PopID();
	}
}
