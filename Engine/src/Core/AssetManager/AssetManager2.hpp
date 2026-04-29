#pragma once
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/ErrorCodes.hpp"
#include "Utility/StringHash.hpp"
#include "nlohmann/json.hpp"
#include "FileSystem/PathManager.hpp"

#define CORI_ADD_ASSET_TRAITS(T, ...) \
template <> struct AssetTraits<__VA_ARGS__ __VA_OPT__(::) T> { \
	static constexpr Utility::StringHash64 TypeHash = Utility::HashString64(#T); \
}

namespace Cori {
	namespace Core {

		using AssetID = Utility::StringHash64;

		template <typename T>
		struct AssetTraits {
			static_assert(sizeof(T) == 0, "Asset type not registered!");
		};

		struct PrimaryAssetBase {

		};

		struct SecondaryAssetBase {

		};

		template<typename T>
		concept CanHotReload = requires(const Handle<T> a, const AssetID b) {
			{ T::Manager::Reload(a, b) } -> std::same_as<void>;
		};

		template<typename T>
		concept IsValidAsset = requires(const Handle<T> a, const AssetID b) {
			{ T::Manager::IsHandleValid(a) } -> std::same_as<bool>;
			{ T::Manager::GetAssetID(a) } -> std::same_as<AssetID>;
			{ T::Manager::template GetPlaceholder<T>() } -> std::same_as<Handle<T>>;
			{ T::Manager::template Load<T>(b) } -> std::same_as<Handle<T>>; // add Unload method requrement
			requires std::same_as<Utility::StringHash64, std::remove_cvref_t<decltype(AssetTraits<T>::TypeHash)>>;
			requires std::same_as<bool, std::remove_cvref_t<decltype(T::Manager::EnableHotReload)>>;
			T::Manager::AddRef(a);
			T::Manager::RemoveRef(a);
		}
		&& (std::derived_from<T, PrimaryAssetBase> || std::derived_from<T, SecondaryAssetBase>)
		&& (!T::Manager::EnableHotReload || CanHotReload<T>);

		template<IsValidAsset T>
		struct AssetRef {
			AssetRef() = default;

			explicit AssetRef(Handle<T> handle) {
				if (T::Manager::IsHandleValid(handle)) {
					T::Manager::AddRef(handle);
					m_Handle = handle;
				}
			}

			~AssetRef() {
				if (T::Manager::IsHandleValid(m_Handle)) {
					T::Manager::RemoveRef(m_Handle);
				}
			}

			AssetRef(const AssetRef& other) : m_Handle(other.m_Handle) {
				if (T::Manager::IsHandleValid(m_Handle)) {
					T::Manager::AddRef(m_Handle);
				}
			}

			AssetRef(AssetRef&& other) noexcept : m_Handle(other.m_Handle) {
				other.m_Handle = {};
			}

			AssetRef& operator=(const AssetRef& other) noexcept {
				m_Handle = other.m_Handle;
				if (T::Manager::IsHandleValid(m_Handle)) {
					T::Manager::AddRef(m_Handle);
				}

				return *this;
			}

			AssetRef& operator=(AssetRef&& other) noexcept {
				if (T::Manager::IsHandleValid(m_Handle)) {
					T::Manager::RemoveRef(m_Handle);
				}

				m_Handle = other.m_Handle;
				other.m_Handle = {};

				return *this;
			}

			Handle<T> GetHandle() {
				return m_Handle;
			}

			AssetID GetAssetID() {
				if (T::Manager::IsHandleValid(m_Handle)) {
					return T::Manager::GetAssetID(m_Handle);
				}

				CORI_CORE_ERROR("GetAssetID called on a AssetRef that holds an invalid handle of object of type <{}>, returning 0.", CORI_CLEAN_TYPE_NAME(T));

				return 0;
			}

		private:
			Handle<T> m_Handle;
		};

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

		enum class AssetType : uint8_t {
			ePrimary = 0,
			eSecondary = 1,
			eUndefined
		};

		enum class AssetDeletionPolicy : uint8_t {
			eRefCounted = 0,
			eKeepAlive = 1
		};

		struct AssetRecord {
			std::filesystem::path path;
			AssetStatus status{ AssetStatus::eUnloaded };
			AssetType type{ AssetType::eUndefined };
			AssetDeletionPolicy deletionPolicy{ AssetDeletionPolicy::eRefCounted };

			uint64_t assetTypenameHash{ 0 };
			uint32_t rawHandleIndex{ UINT32_MAX };
			uint32_t rawHandleVersion{ 0 };
		};


		class AssetManager2 {
		public:
			static void Init();

			static void Shutdown();

			static AssetManager2& Get();

			template<IsValidAsset T>
			static AssetRef<T> Load(const char* path) {
				AssetID id = Utility::HashString64(path);
				if (!Get().m_AssetDatabase.contains(id)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Load failed, no asset with path '{}' found in the data base. Placeholder for '{}' returned.", path, CORI_CLEAN_TYPE_NAME(T));
					return AssetRef<T>(T::Manager::template GetPlaceholder<T>());
				}

				auto& record = Get().m_AssetDatabase[id];

				if (record.assetTypenameHash != AssetTraits<T>::TypeHash) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Load failed, typename hash mismatch, type provided '{}', it's hash '{}', but expected hash is '{}'. Asset with path '{}'. Placeholder will be returned.", CORI_CLEAN_TYPE_NAME(T), AssetTraits<T>::TypeHash, record.assetTypenameHash, path);
					return AssetRef<T>(T::Manager::template GetPlaceholder<T>());
				}

				if (record.status == AssetStatus::eLoaded || record.status == AssetStatus::eLoading || record.status == AssetStatus::eLoadQueued || record.status == AssetStatus::eLoadFailed) {
					return AssetRef<T>(Handle<T>{ record.rawHandleIndex, record.rawHandleVersion });
				}

				//record.status = AssetStatus::eLoading;

				if constexpr (std::derived_from<T, PrimaryAssetBase>) {
				}
				else if constexpr (std::derived_from<T, SecondaryAssetBase>) {
					return AssetRef<T>(T::Manager::template Load<T>(id));
				}

			}

			static AssetRecord& GetAssetRecord(const AssetID id) {
				return Get().m_AssetDatabase[id];
			}

			static void UnloadAsset(AssetID id) {

			}

			static void ScanDirectory(const std::filesystem::path& assetDirPath) {
				if (!exists(assetDirPath)) {
					return;
				}

				for (const auto& entry : std::filesystem::recursive_directory_iterator(assetDirPath)) {
					if (entry.is_regular_file()) {
						if (entry.path().extension() == ".json") {
							ProcessFile(entry.path());
						}
					}
				}

			}

			static std::expected<std::reference_wrapper<std::filesystem::path>, ErrorCode> GetPath(const AssetID assetID) {
				if (Get().m_AssetDatabase.contains(assetID)) {
					return Get().m_AssetDatabase[assetID].path;
				}

				return std::unexpected(ErrorCode::eObjectDoesNotExist);
			}

			static const std::filesystem::path& GetAssetDir() {
				return Get().m_AppRootPath;
			}


			~AssetManager2() = default;

		private:
			AssetManager2() {
				s_Instance = std::unique_ptr<AssetManager2>(this);
				TestManager::Get();

				m_AppRootPath = FileSystem::PathManager::GetAliasedPath("APP_ROOT");
				ScanDirectory(FileSystem::PathManager::GetAliasedPath("ASSET_DIR"));
			}

			static void ProcessFile(const std::filesystem::path& assetFilePath) {
				std::ifstream file(assetFilePath);

				if (!file.good()) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Failed to open asset file '{}', skipping it.", assetFilePath.string());
				}

				nlohmann::json json = nlohmann::json::parse(file);

				if (json.contains("Metadata") && json.contains("AssetData")) {
					nlohmann::json& meta = json["Metadata"];
					if (meta.contains("AssetTypename") && meta.contains("AssetType")) {

						uint32_t typeFlag = meta["AssetType"].get<uint32_t>();
						if (typeFlag != 0 && typeFlag != 1) {
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Asset file '{}' specifies invalid value '{}' for AssetType, must be either 0 (for primary asset) or 1 (for secondary asset). Asset will be skipped.", assetFilePath.string(), typeFlag);
							return;
						}

						Utility::StringHash64 typeHash = Utility::HashString64(meta["AssetTypename"].get<std::string>());
						auto relativeAssetPath = std::filesystem::relative(assetFilePath, Get().m_AppRootPath);
						AssetID pathHash = Utility::HashString64(relativeAssetPath.string());
						//TODO: in debug build, check for hash collisions

						auto& entry = Get().m_AssetDatabase[pathHash];
						entry.path = relativeAssetPath;
						entry.type = static_cast<AssetType>(typeFlag);
						entry.assetTypenameHash = typeHash;

						if (meta.contains("AssetDeletionPolicy") ) {
							uint32_t assetDeletionPolicyFlag = meta["AssetDeletionPolicy"].get<uint32_t>();
							if (assetDeletionPolicyFlag != 0 && assetDeletionPolicyFlag != 1) {
								CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Asset file '{}' specifies 'AssetDeletionPolicy', but holds an invalid value '{}' for AssetDeletionPolicy, must be either 0 (ref counted) or 1 (keep alive). Ref counted will be used.", assetFilePath.string(), typeFlag);
							} else {
								entry.deletionPolicy = static_cast<AssetDeletionPolicy>(assetDeletionPolicyFlag);
							}
						}
					}
				}
			}

			std::unordered_map<AssetID, AssetRecord> m_AssetDatabase;

			std::filesystem::path m_AppRootPath;

			static std::unique_ptr<AssetManager2> s_Instance;
		};

		//FIXME: check for AssetID != 0
		template<typename T>
		void to_json(nlohmann::json& j, const AssetRef<T>& ref) {
			AssetID id = ref.GetAssetID();
			std::string path = AssetManager2::GetPath(id).value().get().filename().string();
			j = path;
		}

		template<typename T>
		void from_json(const nlohmann::json& j, AssetRef<T>& ref) {
			std::string path = j.get<std::string>();
			ref = AssetManager2::Load<T>(path.c_str());
		}
	}
}
