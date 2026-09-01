#pragma once
#include <efsw/efsw.hpp>
#include "Core/AssetManager/AssetManager2.hpp"
#include "Utility/TransparentStringHash.hpp"
#include "Graphics/Vulkan/VulkanMeshManager.hpp"
#include "ThumbnailCache.hpp"

namespace Snowflake {
	class ContentBrowserWatcher final : private efsw::FileWatchListener {
	public:

		struct Stats {
			uint64_t folders{ 0 };
			uint64_t entries{ 0 };
			uint64_t pending{ 0 };
			uint64_t unresolved{ 0 };

			uint64_t visited{ 0 };
			uint64_t listed{ 0 };
			uint64_t foldersAdded{ 0 };
			uint64_t foldersRemoved{ 0 };
			uint64_t entriesAdded{ 0 };
			uint64_t entriesRemoved{ 0 };
			uint64_t validated{ 0 };
			uint64_t timedOut{ 0 };
			uint64_t retried{ 0 };

			double lastPassMs{ 0.0 };
			uint64_t passes{ 0 };
			bool lastPassTargeted{ false };
			bool watching{ false };
		};

		struct View {
			struct FolderRow {
				std::string name;
				std::string virtualDir;
				uint32_t depth{ 0 };
				uint32_t childCount{ 0 };
			};

			struct EntryRow {
				std::string label;
				std::string virtualPath;
				Cori::Core::AssetID id{ 0 };
				uint32_t vectorKey{ UINT32_MAX };
				bool validated{ false };
				bool timedOut{ false };
			};

			std::vector<FolderRow> folders;
			std::vector<EntryRow> entries;
			std::string selectedDir;
			Stats stats;
		};

		ContentBrowserWatcher();

		~ContentBrowserWatcher() override;

		ContentBrowserWatcher(const ContentBrowserWatcher&) = delete;
		ContentBrowserWatcher& operator=(const ContentBrowserWatcher&) = delete;

		void MarkDirty(const std::filesystem::path& path);

		void RequestFullScan();

		void RequestRebuild();

		void SelectFolder(std::string virtualDir);

		[[nodiscard]] std::shared_ptr<const View> GetView() const;

	private:
		struct Entry {
			Cori::Core::AssetID id{ 0 };
			uint32_t vectorKey{ UINT32_MAX };
			std::string label;
			std::string virtualPath;
			std::filesystem::file_time_type stamp{};
			bool validated{ false };
			bool timedOut{ false };
		};

		struct Folder {
			std::string name;
			std::filesystem::path path;
			std::string virtualDir;
			std::filesystem::file_time_type stamp{ std::filesystem::file_time_type::min() };
			bool racy{ false };
			Folder* parent{ nullptr };
			std::vector<Folder*> children;
			std::unordered_map<std::string, Entry, Cori::Utility::TransparentStringHash, std::equal_to<>> entries;
		};

		struct Pending {
			Folder* folder{ nullptr };
			std::string fileName;
			std::chrono::steady_clock::time_point queuedAt{};
		};

		struct Unresolved {
			Folder* folder{ nullptr };
			std::string fileName;
		};

		struct Listing {
			std::vector<std::string> names;
			uint64_t count{ 0 };

			void Reset() { count = 0; }

			void Add(const std::string_view name) {
				if (count == names.size()) {
					names.emplace_back(name);
				}
				else {
					names[count].assign(name);
				}
				count++;
			}

			[[nodiscard]] const std::string& operator[](const uint64_t index) const { return names[index]; }
		};

		struct Scratch {
			Listing dirs;
			Listing files;
			std::unordered_set<std::string_view> present;
			std::vector<Folder*> folders;
			std::vector<Folder*> newFolders;
			std::vector<std::string> fileNames;
		};

		void handleFileAction(efsw::WatchID watchID, const std::string& dir, const std::string& filename, efsw::Action action, const std::string& oldFilename) override;

		void handleMissedFileActions(efsw::WatchID watchID, const std::string& dir) override;

		void ScanThreadMain();

		void FullPass();

		void TargetedPass();

		void ScanDirectory(Folder& folder, const uint32_t depth, const bool recurseAll);

		bool ReadListing(const Folder& folder, const uint32_t depth);

		void ReconcileChildren(Folder& folder, const uint32_t depth);

		void ReconcileEntries(Folder& folder, const uint32_t depth);

		void DestroyRecursive(Folder& folder, const uint32_t depth);

		bool RetryUnresolved();

		bool ProcessQueue();

		void PublishView();

		static void AppendFolderRows(View& view, const Folder& folder, const uint32_t depth);

		Folder& CreateFolder(Folder* parent, std::string name, std::filesystem::path path, std::string virtualDir);

		[[nodiscard]] Folder* FindChild(const Folder& parent, const std::string_view name);

		[[nodiscard]] Folder* ResolveFolder(const std::string_view virtualDir, bool* exact = nullptr);

		[[nodiscard]] bool ToVirtualDir(const std::filesystem::path& path, std::string& out);

		[[nodiscard]] Scratch& GetScratch(const uint32_t depth);

		static void ComposeVirtual(std::string& out, const Folder& parent, const std::string_view name);

		std::unordered_map<std::string, Folder, Cori::Utility::TransparentStringHash, std::equal_to<>> m_Folders;

		std::vector<std::pair<Folder*, Cori::Core::AssetDirID>> m_Roots;
		uint64_t m_EntryCount{ 0 };

		std::deque<Pending> m_Queue;
		std::vector<Unresolved> m_Unresolved;

		std::deque<Scratch> m_Scratch;

		Stats m_Stats;

		std::string m_PathScratch;
		std::string m_KeyScratch;

		std::string m_ActiveSelection;

		std::vector<std::string> m_Targets;
		std::vector<std::string> m_PassDirtyPaths;

		std::unique_ptr<efsw::FileWatcher> m_Watcher;

		std::atomic<std::shared_ptr<const View>> m_View{ nullptr };

		std::thread m_Thread;

		std::mutex m_RequestMutex;
		std::condition_variable m_Wake;

		std::vector<std::string> m_DirtyPaths;
		std::string m_RequestedSelection;

		bool m_Stop{ false };
		bool m_ForceFullScan{ true };
		bool m_ForceRebuild{ false };
		bool m_SelectionChanged{ false };

		static constexpr std::chrono::seconds s_RacyWindow{ 1 };
		static constexpr std::chrono::milliseconds s_ValidationPollInterval{ 250 };
		static constexpr bool m_TimestampCheck{ true };
		static constexpr bool m_UseWatcher{ true };
		static constexpr float m_ValidationTimeout{ 10.0f };
		static constexpr float m_ScanInterval{ 1.0f };
		static constexpr float m_WatchedScanInterval{ 15.0f };

	};

	class ContentBrowser {
	public:
		static constexpr const char* s_DefaultName{ "Content Browser" };

		ContentBrowser();

		void Draw(bool* open, const char* name = s_DefaultName);

		void OnUpdate(Cori::Core::GameTimer& gameTimer);
		void OnTickUpdate(Cori::Core::GameTimer& gameTimer);

	private:
		static constexpr float s_MinTreeWidth{ 120.0f };
		static constexpr float s_DefaultTreeWidth{ 220.0f };
		static constexpr float s_MinGridWidth{ 200.0f };
		static constexpr float s_SplitterWidth{ 4.0f };
		static constexpr float s_TileSizeSliderWidth{ 128.0f };
		static constexpr float s_DefaultWindowWidth{ 920.0f };
		static constexpr float s_DefaultWindowHeight{ 420.0f };

		static constexpr float s_MinTileSize{ 48.0f };
		static constexpr float s_MaxTileSize{ 256.0f };
		static constexpr float s_DefaultTileSize{ 96.0f };
		static constexpr float s_CardPadding{ 6.0f };
		static constexpr float s_LabelLines{ 2.0f };

		void DrawToolbar(const ContentBrowserWatcher::View& view);

		void DrawTree(const ContentBrowserWatcher::View& view, float bodyHeight);

		float CardHeight() const;

		uint32_t DrawGrid(const ContentBrowserWatcher::View& view, float bodyHeight);
		void DrawCard(ImVec2 origin, float thumbnail, bool selected, bool hovered, const ImU32 selectedColor, const ImU32 hoveredColor) const;
		static void DrawLabel(ImVec2 origin, float thumbnail, const char* text);
		void DrawFolder(const ContentBrowserWatcher::View::FolderRow& folder, const ContentBrowserWatcher::View::FolderRow& parent, ImVec2 origin, float thumbnailSize, float fbScale);
		void DrawAssetTile(const ContentBrowserWatcher::View::EntryRow& entry, ImVec2 origin, float thumbnailSize, float fbScale, ImVec2 surfaceOrigin);

		std::shared_ptr<ThumbnailCache> m_Thumbnails;
		std::unordered_map<Cori::Core::AssetID, ThumbnailHandle> m_ThumbnailHandles;

		std::unordered_set<std::string> m_Expanded;
		std::string m_SelectedDir;
		Cori::Core::AssetID m_SelectedAsset{ 0 };

		int32_t m_TileSize{};
		float m_TreeWidth{};
		bool m_ShowPending{ true };
		bool m_TreeSeeded{ false };

		uint32_t m_FocusedFolderIndex{ UINT32_MAX };
		std::vector<uint32_t> m_FocusedFolderChildren;

		std::string m_LastSelectedDir;
		const ContentBrowserWatcher::View* m_LastSeen{};
		ContentBrowserWatcher m_Watcher;
	};
}
