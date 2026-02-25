#pragma once
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/ErrorCodes.hpp"
#include "Utility/StringHash.hpp"
#include "nlohmann/json.hpp"
#include "FileSystem/PathManager.hpp"

namespace Cori {
	namespace Core {
		using AssetID = Utility::StringHash64;

		template<typename T>
		concept IsValidAsset = requires(const Handle<T>& a) {
			{ T::Manager::template IsHandleValid(a) } -> std::same_as<bool>;
			{ T::Manager::template GetAssetID(a) } -> std::same_as<AssetID>;
			{ T::Manager::template GetPlaceholder() } -> std::same_as<Handle<T>>;
			T::Manager::template AddRef(a);
			T::Manager::template RemoveRef(a);
		};

		template<IsValidAsset T>
		struct AssetRef {
			AssetRef() = default;

			explicit AssetRef(Handle<T> handle) {
				if (T::Manager::template IsHandleValid(handle)) {
					T::Manager::template AddRef(handle);
				}
			}

			~AssetRef() {
				if (T::Manager::template IsHandleValid(m_Handle)) {
					T::Manager::template RemoveRef(m_Handle);
				}
			}

			AssetRef(const AssetRef& other) : m_Handle(other.m_Handle) {
				if (T::Manager::template IsHandleValid(m_Handle)) {
					T::Manager::template AddRef(m_Handle);
				}
			}

			AssetRef(AssetRef&& other) noexcept : m_Handle(other.m_Handle) {
				other.m_Handle = {};
			}

			AssetRef& operator=(const AssetRef& other) noexcept {
				m_Handle = other.m_Handle;
				if (T::Manager::template IsHandleValid(m_Handle)) {
					T::Manager::template AddRef(m_Handle);
				}

				return *this;
			}

			AssetRef& operator=(AssetRef&& other) noexcept {
				m_Handle = other.m_Handle;
				other.m_Handle = {};

				return *this;
			}

			Handle<T> GetHandle() {
				return m_Handle;
			}

			AssetID GetAssetID() {
				if (T::Manager::template IsHandleValid(m_Handle)) {
					return T::Manager::template GetAssetID(m_Handle);
				}

				return 0;
			}

		private:
			Handle<T> m_Handle;
		};

		class TestManager;

		struct TestAsset {
			using Manager = TestManager;

			uint64_t data;
		};

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


			static Handle<TestAsset> GetPlaceholder() {
				return Get().placeholder;
			}

			Handle<TestAsset> placeholder;
			std::unordered_map<Handle<TestAsset>, AssetID> reverseLookupMap;
			std::vector<uint32_t> refCounts;
			FlatSlotMap<TestAsset> flat;
		};

		struct PrimaryAssetBase {

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
			uint64_t rawHandle{ 0xFFFFFFFF00000000 };
		};


		class AssetManager2 {
		public:
			static void Init();

			static void Shutdown();

			static AssetManager2& Get();

			template<IsValidAsset T>
			static AssetRef<T> Load(AssetID id) {

			}

			template<IsValidAsset T> requires std::derived_from<T, PrimaryAssetBase>
			static AssetRef<T> LoadPrimary(AssetID id) {

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

				if (json.contains("AssetTypename") && json.contains("AssetType")) {
					uint32_t typeFlag = json["AssetType"].get<uint32_t>();
					if (typeFlag != 0 && typeFlag != 1) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Asset file '{}' specifies invalid value '{}' for AssetType, must be either 0 (for primary asset) or 1 (for secondary asset). Asset will be skipped.", assetFilePath.string(), typeFlag);
						return;
					}

					Utility::StringHash64 typeHash = Utility::HashString64(json["AssetTypename"].get<std::string>());
					AssetID pathHash = Utility::HashString64(assetFilePath.string());
					//TODO: in debug build, check for hash collisions

					auto& entry = Get().m_AssetDatabase[pathHash];
					entry.path = std::filesystem::relative(assetFilePath, Get().m_AppRootPath);
					entry.type = static_cast<AssetType>(typeFlag);
					entry.assetTypenameHash = typeHash;

					if (json.contains("AssetDeletionPolicy") ) {
						uint32_t assetDeletionPolicyFlag = json["AssetDeletionPolicy"].get<uint32_t>();
						if (assetDeletionPolicyFlag != 0 && assetDeletionPolicyFlag != 1) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Asset file '{}' specifies 'AssetDeletionPolicy', but holds an invalid value '{}' for AssetDeletionPolicy, must be either 0 (ref counted) or 1 (keep alive). Ref counted will be used.", assetFilePath.string(), typeFlag);
						} else {
							entry.deletionPolicy = static_cast<AssetDeletionPolicy>(assetDeletionPolicyFlag);
						}
					}
				}
			}

			std::unordered_map<AssetID, AssetRecord> m_AssetDatabase;

			std::filesystem::path m_AppRootPath;

			static std::unique_ptr<AssetManager2> s_Instance;
		};

		template<typename T>
		void to_json(nlohmann::json& j, const AssetRef<T>& ref) {
			AssetID id = ref.GetAssetID();
			std::string path = AssetManager2::GetPath(id).value().get().filename().string();
			j = path;
		}

		template<typename T>
		void from_json(const nlohmann::json& j, AssetRef<T>& ref) {
			std::string path = j.get<std::string>();
			AssetID id = Utility::HashString64(path);
			ref = AssetManager2::Load<T>(id);
		}
	}
}
