#pragma once
#include <oneapi/tbb/concurrent_vector.h>
#include <oneapi/tbb/concurrent_unordered_map.h>
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManagerEnums.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/AssetManager/AssetDependency.hpp"
#include "Core/ErrorCodes.hpp"
#include "Utility/StringHash.hpp"
#include "nlohmann/json.hpp"
#include "FileSystem/PathManager.hpp"
#include "Utility/CleanTypeName.hpp"
#include "Utility/GlazeUtils.hpp"
#include "Core/Time.hpp"
#include "Utility/BitHelpers.hpp"
#include "Core/Threading/OneAtATime.hpp"

namespace Cori {
	namespace Core {
		namespace Internal {
			struct EmptyRefTag { EmptyRefTag() = default; };
			inline constexpr EmptyRefTag EmptyRef{};
		}

		using AssetID = Utility::StringHash64;
		using AssetDirID = uint16_t;

		struct PrimaryAssetBase {

		};

		struct SecondaryAssetBase {

		};

		template<typename T>
		concept IsAssetBase = std::derived_from<T, PrimaryAssetBase> || std::derived_from<T, SecondaryAssetBase>;

		template<typename T>
		concept IsRegisteredAsset = requires {
			typename T::Manager;
		};

		template<typename T>
		concept AssetHasNoPlaceholder = requires { requires T::NOPLACEHOLDER; };

		template<typename T>
		inline constexpr bool AssetHotReloadEnabled = []{
			if constexpr (requires { { T::EnableHotReload } -> std::convertible_to<bool>; }) {
				return static_cast<bool>(T::EnableHotReload);
			}
			else if constexpr (requires { { T::Manager::EnableHotReload } -> std::convertible_to<bool>; }) {
				return static_cast<bool>(T::Manager::EnableHotReload);
			}
			else {
				return false;
			}
		}();

		template<typename T>
		inline constexpr bool AssetAutoHotReloadEnabled = AssetHotReloadEnabled<T> && []{
			if constexpr (requires { { T::EnableAutoHotReload } -> std::convertible_to<bool>; }) {
				return static_cast<bool>(T::EnableAutoHotReload);
			}
			else if constexpr (requires { { T::Manager::EnableAutoHotReload } -> std::convertible_to<bool>; }) {
				return static_cast<bool>(T::Manager::EnableAutoHotReload);
			}
			else {
				return false;
			}
		}();

		template<typename T>
		concept SpokeSuppliesPlaceholder = requires {
			{ T::Manager::template GetPlaceholder<T>() } -> std::same_as<Handle<T>>;
		};

		template<typename T>
		concept SpokeDeclaresHotReloadPolicy = requires {
			{ T::Manager::EnableHotReload } -> std::convertible_to<bool>;
			{ T::Manager::EnableAutoHotReload } -> std::convertible_to<bool>;
		};

		template<typename T>
		concept SpokeHasAllocateExtras = requires(const Handle<T> handle) {
			{ T::Manager::AllocateExtras(handle) } -> std::same_as<void>;
		};

		template<typename T>
		concept SpokeHasFreeExtras = requires(const Handle<T> handle) {
			{ T::Manager::FreeExtras(handle) } -> std::same_as<void>;
		};

		template<typename T>
		concept SpokePublishesIdentity = requires(const Handle<T> handle) {
			{ T::Manager::GetIdentityVersion(handle) } -> std::same_as<uint32_t>;
			{ T::Manager::TryReadDependencies(handle) } -> std::same_as<std::expected<std::pair<AssetDependencySet, uint32_t>, ErrorCode>>;
		};

		//TODO: add a 'Serialize' requirement for the asset types that can be saved back to disk (e.g. ShaderEffect, Material etc, all the assets that are just configs)
		template<typename T>
		concept HasValidSpoke = requires(const Handle<T> handle, const AssetID id, const uint32_t vectorKey, const AssetStatus status,  const uint32_t gen, std::filesystem::path path, std::string name) {
			{ T::Manager::template AllocateHandle<T>() } -> std::same_as<Handle<T>>;
			{ T::Manager::BindAsset(handle, id, vectorKey) } -> std::same_as<void>;
			{ T::Manager::BumpGeneration(handle) } -> std::same_as<uint32_t>;
			{ T::Manager::IsHandleValid(handle) } -> std::same_as<bool>;
			{ T::Manager::GetAssetID(handle) } -> std::same_as<AssetID>;
			{ T::Manager::TryAddRef(handle) } -> std::same_as<bool>;
			{ T::Manager::AddRef(handle) } -> std::same_as<void>;
			{ T::Manager::RemoveRef(handle) } -> std::same_as<void>;
			{ T::Manager::SetAssetStatus(handle, status) } -> std::same_as<void>;
			{ T::Manager::GetAssetStatus(handle) } -> std::same_as<AssetStatus>;
			{ T::Manager::Load(handle, id, gen, vectorKey, std::move(path)) } -> std::same_as<void>;
			{ T::Manager::Load(handle, id, gen, vectorKey, std::move(path), std::move(name)) } -> std::same_as<void>;
		} && SpokeDeclaresHotReloadPolicy<T> && (SpokeSuppliesPlaceholder<T> || AssetHasNoPlaceholder<T>);


		template<typename T>
		concept IsValidAsset = IsAssetBase<T> && IsRegisteredAsset<T> && HasValidSpoke<T>;

		template <IsValidAsset T>
		struct AssetTraits {
			static constexpr Utility::StringHash64 TypeHash = Utility::HashString64(std::meta::identifier_of(^^T));
		};

		template<typename T>
		concept CanHotReload = AssetHotReloadEnabled<T> && HasValidSpoke<T>;

		template<IsValidAsset T>
		struct AssetRef {
			//make private later
		private:
			AssetRef() = default;
		public:
			constexpr explicit AssetRef(Internal::EmptyRefTag) {}   // deliberately empty; for parse targets

			explicit AssetRef(Handle<T> handle) {
				CORI_CORE_ASSERT(T::Manager::IsHandleValid(handle), "Trying to construct AssetRef<{}> with an invalid handle, asserting.", CORI_CLEAN_TYPE_NAME(T));
				m_Handle = handle;
				T::Manager::AddRef(m_Handle);
			}

			~AssetRef() {
				if (m_Handle.IsSet()) {
					T::Manager::RemoveRef(m_Handle);
				}
			}

			AssetRef(const AssetRef& other) : m_Handle(other.m_Handle) {
				if (m_Handle.IsSet()) {
					T::Manager::AddRef(m_Handle);
				}
			}

			AssetRef(AssetRef&& other) noexcept : m_Handle(other.m_Handle) {
				other.m_Handle = {};
			}

			AssetRef& operator=(const AssetRef& other) noexcept {
				if (&other == this) {
					return *this;
				}

				if (other.m_Handle.IsSet()) {
					T::Manager::AddRef(other.m_Handle);
				}

				auto oldHandle = m_Handle;
				m_Handle = other.m_Handle;

				if (oldHandle.IsSet()) {
					T::Manager::RemoveRef(oldHandle);
				}

				return *this;
			}

			AssetRef& operator=(AssetRef&& other) noexcept {
				if (&other == this) {
					return *this;
				}

				auto oldHandle = m_Handle;
				m_Handle = other.m_Handle;
				other.m_Handle = {};

				if (oldHandle.IsSet()) {
					T::Manager::RemoveRef(oldHandle);
				}

				return *this;
			}

			[[nodiscard]] Handle<T> GetHandle() {
				return m_Handle;
			}

			[[nodiscard]] ConstHandle<T> GetHandle() const {
				return m_Handle;
			}

			[[nodiscard]] bool IsInitialized() const {
				return m_Handle.IsSet();
			}

			[[nodiscard]] std::optional<AssetID> GetAssetID() const {
				if (m_Handle.IsSet()) {
					AssetID result = T::Manager::GetAssetID(m_Handle);
					return result != UINT64_MAX ? std::optional(result) : std::nullopt;
				}

				CORI_CORE_ERROR("GetAssetID called on a moved AssetRef<{}>, returning 0.", CORI_CLEAN_TYPE_NAME(T));

				return std::nullopt;
			}
		protected:
			friend class AssetManager2;

			static AssetRef<T> AdoptExisting(const Handle<T> handle) {
				CORI_CORE_ASSERT(T::Manager::IsHandleValid(handle), "AssetRef<{}>::AdoptExisting called with an invalid handle, asserting.", CORI_CLEAN_TYPE_NAME(T));
				AssetRef<T> inst;
				inst.m_Handle = handle;
				return inst;
			}

		private:
			Handle<T> m_Handle;
		};

		#if 0
		class TestManager;

		struct TestAsset : public PrimaryAssetBase {
			using Manager = TestManager;

			uint64_t data{};
			TestAsset() = default;
			TestAsset(uint64_t d) : PrimaryAssetBase(), data(d) {}
		};

		CORI_ADD_ASSET_TRAITS(TestAsset);

		class TestManager {
		public:
			TestManager() {
				flat.Reserve(128);
				refCounts.resize(128, 0);
				placeholder = flat.Emplace(128);
				reverseLookupMap[placeholder] = 128;
			}

			static TestManager& Get() {
				static TestManager inst;
				return inst;
			}

			static bool IsHandleValid(const Handle<TestAsset> handle) {
				return Get().flat.IsHandleValid(handle);
			}

			static void AddRef(const Handle<TestAsset> handle) {
				Get().refCounts[handle.GetIndex()]++;
			}

			static void RemoveRef(const Handle<TestAsset> handle) {
				Get().refCounts[handle.GetIndex()]--;
			}

			static Handle<TestAsset> Add(uint64_t data, AssetID assetID) {
				auto handle = Get().flat.Emplace(data);
				Get().reverseLookupMap[handle] = assetID;
				return handle;
			}

			static void Remove(const Handle<TestAsset> handle) {
				if (IsHandleValid(handle)) {
					Get().flat.Remove(handle);
					Get().refCounts[handle.GetIndex()] = 0;
					Get().reverseLookupMap.erase(handle);
				}
			}

			static AssetID GetAssetID(const Handle<TestAsset> handle) {
				return Get().reverseLookupMap.at(handle);
			}

			template<typename T> requires std::same_as<T, TestAsset>
			static Handle<T> GetPlaceholder() {
				return Get().placeholder;
			}

			template<typename T> requires std::same_as<T, TestAsset>
			static Handle<T> Load(const nlohmann::json& j) {
				//parse
				return Add(12312, 52343); // just for test
			}

			Handle<TestAsset> placeholder;
			std::unordered_map<Handle<TestAsset>, AssetID> reverseLookupMap;
			std::vector<uint32_t> refCounts;
			FlatSlotMap<TestAsset> flat;
		};
		#endif

		struct AssetRecord {
			//std::filesystem::path path;
			//std::filesystem::file_time_type pathTimestamp;
			//AssetType type{ AssetType::eUndefined };
			//std::string name{};

			uint32_t vectorKey{};

			//uint64_t assetTypenameHash{ 0 };
			//uint32_t rawHandleIndex{ UINT32_MAX };
			//uint32_t rawHandleVersion{ 0 };
		};

		struct AssetDir {
			std::string label{ "Empty AssetDir Label" };
			std::filesystem::path dir;
			std::atomic<std::filesystem::file_time_type> dirTimestamp;
			AssetDirID id;
			std::atomic<uint64_t> gen; // if i even want to allow adding asset dirs after the startup, this should become a seqlock
		};

		class AssetManager2 {
			template<IsValidAsset T>
			static void ReloadImpl(const uint32_t vectorKey) {
				AssetID id;
				Handle<T> handle;
				uint32_t gen;
				{
					auto& mutex = GetMutex();
					std::lock_guard lk(mutex);
					CORI_PROFILER_LOCK_MARK(mutex);
					handle = Handle<T>(Get().m_RawHandles[vectorKey].load(std::memory_order_acquire));
					if (!handle.IsSet() || !T::Manager::IsHandleValid(handle)) {
						return;
					}

					id = Get().m_ReverseLookup[vectorKey];
					gen = T::Manager::BumpGeneration(handle);
					T::Manager::SetAssetStatus(handle, AssetStatus::eLoading);
				}

				auto nonAtomicData = Get().m_NonAtomicData[vectorKey].load(std::memory_order_acquire);
				auto fsPath = Get().m_AssetDirs[Get().m_ParentDirIDs[vectorKey].load(std::memory_order_acquire)].dir / nonAtomicData->jsonPath;
				std::string name;
				if (nonAtomicData->name) {
					name = nonAtomicData->name.value();
				}
				#ifdef DEBUG_BUILD
				T::Manager::Load(handle, id, gen, vectorKey, std::move(fsPath), std::move(name));
				#else
				T::Manager::Load(handle, id, gen, vectorKey, std::move(fsPath));
				#endif
			}

			template <IsValidAsset T>
			static void ChangePolicyImpl(const AssetDeletionPolicy newPolicy, const uint32_t vectorKey) {
				auto& mutex = GetMutex();
				std::lock_guard lk(mutex);
				CORI_PROFILER_LOCK_MARK(mutex);
				auto old = GetDeletionPoliciesVector()[vectorKey].load(std::memory_order_acquire);
				if (old == newPolicy) {
					return;
				}

				Handle<T> handle = Handle<T>(Get().m_RawHandles[vectorKey].load(std::memory_order_acquire));
				if (handle.IsSet()) {
					GetDeletionPoliciesVector()[vectorKey].store(newPolicy, std::memory_order_release);
					if (old == AssetDeletionPolicy::eRefCounted) {
						T::Manager::TryAddRef(handle);
					}
					else {
						T::Manager::RemoveRef(handle);
					}
				}
			}

			template<IsValidAsset T>
			static uint32_t GetIdentityVersionImpl(const uint32_t vectorKey) {
				const Handle<T> handle = Handle<T>(Get().m_RawHandles[vectorKey].load(std::memory_order_acquire));
				if (!handle.IsSet() || !T::Manager::IsHandleValid(handle)) {
					return 0;
				}

				return T::Manager::GetIdentityVersion(handle);
			}

			template<IsValidAsset T>
			static std::expected<std::pair<AssetDependencySet, uint32_t>, ErrorCode> TryReadDependenciesImpl(const uint32_t vectorKey) {
				const Handle<T> handle = Handle<T>(Get().m_RawHandles[vectorKey].load(std::memory_order_acquire));
				if (!handle.IsSet() || !T::Manager::IsHandleValid(handle)) {
					return std::unexpected(ErrorCode::eInvalidHandle);
				}

				return T::Manager::TryReadDependencies(handle);
			}

			template<IsValidAsset T>
			static uint32_t GetIdentityVersionByHandleImpl(const uint32_t index, const uint32_t version) {
				const Handle<T> handle(index, version);
				if (!T::Manager::IsHandleValid(handle)) {
					return 0;
				}

				return T::Manager::GetIdentityVersion(handle);
			}

			template<IsValidAsset T>
			static AssetStatus GetAssetStatusByHandleImpl(const uint32_t index, const uint32_t version) {
				const Handle<T> handle(index, version);
				if (!T::Manager::IsHandleValid(handle)) {
					return AssetStatus::eUnspecified;
				}

				return T::Manager::GetAssetStatus(handle);
			}

			struct AssetTypeOps {
				void (*Reload)(uint32_t vectorKey);
				void (*ChangePolicy)(AssetDeletionPolicy newPolicy, uint32_t vectorKey);
				uint32_t (*GetIdentityVersion)(uint32_t vectorKey);
				std::expected<std::pair<AssetDependencySet, uint32_t>, ErrorCode> (*TryReadDependencies)(uint32_t vectorKey);
				uint32_t (*GetIdentityVersionByHandle)(uint32_t index, uint32_t version);
				AssetStatus (*GetAssetStatusByHandle)(uint32_t index, uint32_t version);
				bool hotReload;
				bool autoHotReload;
			};

			template<IsValidAsset T>
			static const AssetTypeOps& TypeOpsFor() {
				static bool oneshot = true;
				static const AssetTypeOps ops{
					.Reload = AssetHotReloadEnabled<T> ? &ReloadImpl<T> : nullptr,
					.ChangePolicy = &ChangePolicyImpl<T>,
					.GetIdentityVersion = [] {
						if constexpr (SpokePublishesIdentity<T>) { return &GetIdentityVersionImpl<T>; }
						else { return static_cast<uint32_t(*)(uint32_t)>(nullptr); }
					}(),
					.TryReadDependencies = [] {
						if constexpr (SpokePublishesIdentity<T>) { return &TryReadDependenciesImpl<T>; }
						else { return static_cast<std::expected<std::pair<AssetDependencySet, uint32_t>, ErrorCode>(*)(uint32_t)>(nullptr); }
					}(),
					.GetIdentityVersionByHandle = [] {
						if constexpr (SpokePublishesIdentity<T>) { return &GetIdentityVersionByHandleImpl<T>; }
						else { return static_cast<uint32_t(*)(uint32_t, uint32_t)>(nullptr); }
					}(),
					.GetAssetStatusByHandle = &GetAssetStatusByHandleImpl<T>,
					.hotReload = AssetHotReloadEnabled<T>,
					.autoHotReload = AssetAutoHotReloadEnabled<T>
				};
				if (oneshot) {
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "New asset type <{}> registered, hash '{}'", std::meta::identifier_of(^^T), Utility::HashString64(std::meta::identifier_of(^^T)));
					oneshot = false;
				}
				return ops;
			}

			struct JsonLayout {
				struct MetadataType {
					std::string assetTypename;
					AssetType assetType;
					std::optional<Utility::GlazeWithFallback<AssetDeletionPolicy, AssetDeletionPolicy::eRefCounted, "from Metadata declared in AssetManager::ProcessFile">> assetDeletionPolicy;
					std::optional<std::vector<std::string>> assetFiles;
					std::optional<std::string> name;
				} Metadata;
				glz::raw_json_view AssetData;
			};

			struct ProcessedMetadata {
				uint64_t typeNameHash;
				uint64_t assetDataHash;
				AssetType type;
				AssetDeletionPolicy policy;
				std::optional<std::vector<std::string>> assetFiles;
				std::optional<std::string> name;
			};

			struct NonAtomicAssetMeta {
				std::filesystem::path jsonPath;
				//std::optional<std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>>> assetFiles;
				std::optional<std::vector<std::filesystem::path>> assetFiles; // these should resolve against the json parent dir
				std::optional<std::string> name;
			};
		public:
			static void Init();

			static void Shutdown();

			static AssetManager2& Get();

			static bool IsRegistered(const char* path) {
				AssetID id = Utility::HashString64(path);
				return Get().m_AssetDatabase.contains(id);
			}

			static bool IsRegistered(const AssetID id) {
				return Get().m_AssetDatabase.contains(id);
			}

			static std::optional<uint64_t> GetAssetTypeHash(const AssetID id);

			static uint64_t GetAssetTypeHash(const uint32_t key);

			static uint32_t GetIdentityVersion(const uint32_t vectorKey);

			[[nodiscard]] static std::expected<std::pair<AssetDependencySet, uint32_t>, ErrorCode> TryReadDependencies(const uint32_t vectorKey);

			[[nodiscard]] static uint32_t GetDependencyIdentityVersion(const AssetDependency& dependency);

			[[nodiscard]] static AssetStatus GetDependencyStatus(const AssetDependency& dependency);

			static std::optional<uint32_t> GetAssetVectorKey(const AssetID id);

			[[nodiscard]] static std::string GetAssetDisplayName(const AssetID id);

			template<typename F>
			static void ForEachAssetDir(F&& f) {
				auto publishedCount = Get().m_PublishedDirCount.load(std::memory_order_acquire);
				for (auto& entry : std::ranges::subrange(Get().m_AssetDirs.begin(), Get().m_AssetDirs.begin() + publishedCount)) {
					f(entry);
				}
			}

			template<IsValidAsset T>
			static AssetRef<T> Load(const char* path) {
				return Load<T>(Utility::HashString64(path), path);
			}

			template<IsValidAsset T>
			static AssetRef<T> Load(const AssetID id) {
				return Load<T>(id, "Path missing, AssetID only variant of Load was called.");
			}


			template<IsValidAsset T>
			static AssetRef<T> Load(const AssetID id, const char* path) {
				Handle<T> handle;
				uint32_t gen;
				uint32_t vectorKey;
				std::filesystem::path fsPath;
				std::string name;
				
				{
					auto& mutex = GetMutex();
					std::lock_guard lk(mutex);
					CORI_PROFILER_LOCK_MARK(mutex);

					if (!Get().m_AssetDatabase.contains(id)) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Load failed, no asset with path '{}' found in the data base. Placeholder for '{}' returned.", path, std::meta::identifier_of(^^T));
						return AssetRef<T>(T::Manager::template GetPlaceholder<T>());
					}

					auto& record = Get().m_AssetDatabase[id];
					vectorKey = record.vectorKey;
					uint64_t typeHash = Get().m_TypeNameHashes[vectorKey].load(std::memory_order_acquire);
					if (typeHash != Utility::HashString64(std::meta::identifier_of(^^T))) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Load failed, typename hash mismatch, type provided '{}', it's hash '{}', but expected hash is '{}'. Asset with path '{}'. Placeholder will be returned.", std::meta::identifier_of(^^T), Utility::HashString64(std::meta::identifier_of(^^T)), typeHash, path);
						return AssetRef<T>(T::Manager::template GetPlaceholder<T>());
					}

					Get().m_AssetOps[typeHash].store(&TypeOpsFor<T>(), std::memory_order_release);

					handle = Handle<T>(Get().m_RawHandles[vectorKey].load(std::memory_order_acquire));
					if (handle.IsSet()) {
						if (T::Manager::TryAddRef(handle)) {
							return AssetRef<T>::AdoptExisting(handle);
						}
					}

					handle = T::Manager::template AllocateHandle<T>();
					gen = T::Manager::BumpGeneration(handle);
					T::Manager::BindAsset(handle, id, vectorKey);

					if (GetDeletionPoliciesVector()[vectorKey].load(std::memory_order_acquire) == AssetDeletionPolicy::eKeepAlive) {
						T::Manager::AddRef(handle);
					}

					Get().m_RawHandles[vectorKey].store(handle.ToRaw(), std::memory_order_release);

					T::Manager::SetAssetStatus(handle, AssetStatus::eLoading);

					auto nonAtomicData = Get().m_NonAtomicData[vectorKey].load(std::memory_order_acquire);
					fsPath = Get().m_AssetDirs[Get().m_ParentDirIDs[vectorKey].load(std::memory_order_acquire)].dir / nonAtomicData->jsonPath;
					if (nonAtomicData->name) {
						name = nonAtomicData->name.value();
					}
				}

				if constexpr (std::derived_from<T, PrimaryAssetBase>) {
					static_assert("not yet");
				}
				else if constexpr (std::derived_from<T, SecondaryAssetBase>) {
					#ifdef DEBUG_BUILD
					T::Manager::Load(handle, id, gen, vectorKey, std::move(fsPath), std::move(name));
					#else
					T::Manager::Load(handle, id, gen, vectorKey, std::move(fsPath));
					#endif
				}

				return AssetRef<T>::AdoptExisting(handle);
			}

			static uint64_t GetRawHandle(const uint32_t vectorKey) {
				return Get().m_RawHandles[vectorKey].load(std::memory_order_acquire);
			}

			static void SetRawHandle(const uint32_t vectorKey, const uint64_t newHandle) {
				Get().m_RawHandles[vectorKey].store(newHandle, std::memory_order_release);
			}

			static void ChangePolicy(const AssetID id, const AssetDeletionPolicy newPolicy);

			static AssetRecord& GetAssetRecord(const AssetID id);

			//FIXME: use file watcher instead of brute-forcing it, costs 2.4s!!!! on a torture test (1945 folders 12649 asset files)
			static void ScanDirectory(const AssetDirID dirID);

			static void OnUpdate(GameTimer& timer);

			//FIXME: use file watcher instead of brute-forcing it, costs 250ms on a torture test (1945 folders 12649 asset files)
			static void ScanAndReload();

			static CORI_PROFILE_LOCKABLE_TYPE(std::mutex)& GetMutex() {
				return Get().m_Mutex;
			}

			static tbb::concurrent_vector<std::atomic<AssetDeletionPolicy>>& GetDeletionPoliciesVector() {
				return Get().m_DeletionPolicies;
			}

			static std::optional<AssetDirID> AddAssetDir(std::string label, const std::filesystem::path& dir);

			~AssetManager2();

		private:
			AssetManager2() {
				s_Instance = std::unique_ptr<AssetManager2>(this);

				auto key = AddAssetDir("assets", FileSystem::PathManager::GetAliasedPath("ASSET_DIR"));
				auto key2 = AddAssetDir("enginedata", FileSystem::PathManager::GetAliasedPath("ENGINE_DATA"));
				ScanDirectory(key.value());
				ScanDirectory(key2.value());
			}

			static std::optional<ProcessedMetadata> ReloadMetadata(const uint32_t vectorKey);

			static std::filesystem::path GetAssetFileBasePath(const uint32_t vectorKey, const NonAtomicAssetMeta* nonAtomicMeta) {
				const auto& dirEntry = Get().m_AssetDirs[Get().m_ParentDirIDs[vectorKey].load(std::memory_order_acquire)];
				return dirEntry.dir / nonAtomicMeta->jsonPath.parent_path();
			}

			static bool CompareIdentityMetadata(const ProcessedMetadata& meta, const uint32_t vectorKey);

			static bool CompareMutableMetadata(const ProcessedMetadata& meta, const uint32_t vectorKey);

			static void ApplyIdentityMetadata(const ProcessedMetadata& meta, const uint32_t vectorKey) {
				Get().m_TypeNameHashes[vectorKey].store(meta.typeNameHash, std::memory_order_release);
				Get().m_AssetTypes[vectorKey].store(meta.type, std::memory_order_release);
			}

			static void ApplyMutableMetadata(ProcessedMetadata& meta, const uint32_t vectorKey);

			static bool ProcessFile(const std::filesystem::path& assetFilePath, const AssetDirID dirID);

			//std::unordered_map<AssetID, AssetRecord> m_AssetDatabase;

			tbb::concurrent_unordered_map<AssetID, AssetRecord> m_AssetDatabase;
			tbb::concurrent_unordered_map<uint64_t, std::atomic<const AssetTypeOps*>> m_AssetOps;
			tbb::concurrent_vector<AssetDir> m_AssetDirs;

			std::atomic<bool> m_ScanRunning;
			std::atomic<bool> m_SaRRunning;


			tbb::concurrent_vector<std::atomic<NonAtomicAssetMeta*>> m_NonAtomicData;
			tbb::concurrent_vector<NonAtomicAssetMeta*> m_AllSlots;

			tbb::concurrent_vector<std::vector<std::filesystem::file_time_type>> m_AssetFileStamps; //purely internal, should not be read outside ScanDirectory or ScanAndReload methods

			tbb::concurrent_vector<AssetID> m_ReverseLookup;
			tbb::concurrent_vector<std::atomic<AssetDeletionPolicy>> m_DeletionPolicies;
			tbb::concurrent_vector<std::atomic<AssetDirID>> m_ParentDirIDs;
			tbb::concurrent_vector<std::atomic<uint64_t>> m_RawHandles;
			tbb::concurrent_vector<std::atomic<uint64_t>> m_AssetDataHashes;
			tbb::concurrent_vector<std::filesystem::file_time_type> m_JsonTimestamp;
			tbb::concurrent_vector<std::atomic<AssetType>> m_AssetTypes;
			tbb::concurrent_vector<std::atomic<uint64_t>> m_TypeNameHashes;


			std::atomic<AssetDirID> m_PublishedDirCount{ 0 };
			uint32_t m_NextVectorKey{ 0 };
			std::atomic<uint32_t> m_PublishedCount{ 0 };

			CORI_PROFILE_LOCKABLE_N(std::mutex, m_Mutex, "AssetManager2 registry lock");

			static std::unique_ptr<AssetManager2> s_Instance;
		};
	}
}

//FIXME: check for AssetID != 0r
namespace glz {
	template <class T>
	struct from<JSON, Cori::Core::AssetRef<T>> {
		template <auto Opts>
		static void op(Cori::Core::AssetRef<T>& to, is_context auto&& ctx, auto&& it, auto&& end) {
			std::string result;
			parse<JSON>::op<Opts>(result, ctx, it, end);
			if (!static_cast<bool>(ctx.error)) {
				to = Cori::Core::AssetManager2::Load<T>(result.c_str());
			}
		}
	};

	//template <class T>
	//struct to<JSON, Cori::Core::AssetRef<T>> {
	//	template <auto Opts>
	//	static void op(const Cori::Core::AssetRef<T>& from, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
	//		CORI_CORE_ASSERT(false, "to json was called for AssetRef")
	//		Cori::Core::AssetID id = from.GetAssetID();
	//		std::string path = Cori::Core::AssetManager2::GetPath(id).value().get().filename().string();
	//		serialize<JSON>::op<Opts>(path, ctx, b, ix);
	//	}
	//};
}