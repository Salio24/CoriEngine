#pragma once
#include "Core/DataStructures/FlatSlotMap.hpp"
#include "AssetManagerEnums.hpp"
#include "AssetManager/AssetLoadStatus.hpp"
#include "Core/ErrorCodes.hpp"
#include "Utility/StringHash.hpp"
#include "nlohmann/json.hpp"
#include "FileSystem/PathManager.hpp"
#include "Utility/CleanTypeName.hpp"
#include "Utility/GlazeUtils.hpp"
#include "Core/Time.hpp"

#define CORI_ADD_ASSET_TRAITS(T, ...) \
template <> struct AssetTraits<__VA_ARGS__ __VA_OPT__(::) T> { \
	static constexpr Utility::StringHash64 TypeHash = Utility::HashString64(#T); \
}

class ScopedTimer {
public:
	using clock = std::chrono::steady_clock;

	explicit ScopedTimer(std::string_view name = "ScopedTimer")
		: name_(name), start_(clock::now()) {}

	~ScopedTimer() {
		const auto end = clock::now();
		const auto duration = std::chrono::duration<double, std::milli>(end - start_);
		std::cout << name_ << " took " << std::fixed << std::setprecision(5)
				  << duration.count() << " ms\n";
	}

	ScopedTimer(const ScopedTimer&) = delete;
	ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
	std::string_view name_;
	clock::time_point start_;
};

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
			{ T::Manager::template Load<T>(b) } -> std::same_as<Handle<T>>; // add Unload method requirement, add 'Serialize' method requirement for assets that can be saved to disk (e.g. ShaderEffect, Material etc, all the assets that are just configs)
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
				CORI_CORE_ASSERT(T::Manager::IsHandleValid(handle), "Trying to construct AssetRef<{}> with an invalid handle, asserting.", CORI_CLEAN_TYPE_NAME(T));
				m_Handle = handle;
				T::Manager::AddRef(m_Handle);
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
				if (&other == this) {
					return *this;
				}

				if (T::Manager::IsHandleValid(m_Handle)) {
					T::Manager::RemoveRef(m_Handle);
				}

				m_Handle = other.m_Handle;
				other.m_Handle = {};

				return *this;
			}

			[[nodiscard]] Handle<T> GetHandle() {
				return m_Handle;
			}

			[[nodiscard]] ConstHandle<T> GetHandle() const {
				return m_Handle;
			}

			[[nodiscard]] bool IsInitialized() const {
				return m_Handle.GetIndex() != UINT32_MAX && m_Handle.GetVersion() != 0;
			}

			[[nodiscard]] AssetID GetAssetID() {
				if (T::Manager::IsHandleValid(m_Handle)) {
					return T::Manager::GetAssetID(m_Handle);
				}

				CORI_CORE_ERROR("GetAssetID called on a AssetRef<{}>, that holds an invalid handle, returning 0.", CORI_CLEAN_TYPE_NAME(T));

				return 0;
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
			std::filesystem::path path;
			std::filesystem::file_time_type pathTimestamp;
			AssetStatus status{ AssetStatus::eUnloaded };
			AssetType type{ AssetType::eUndefined };
			AssetDeletionPolicy deletionPolicy{ AssetDeletionPolicy::eRefCounted };
			std::string name{};

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

			static std::expected<void, ErrorCode> AddAssetRecord(const AssetID id, const AssetRecord& record) {
				auto [_, result] = Get().m_AssetDatabase.try_emplace(id, record);
				if (!result) {
					return std::unexpected(ErrorCode::eObjectAlreadyExists);
				}

				return {};
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

			static void OnUpdate(GameTimer& timer) {
				static float timer_ = 0.0;
				timer_ += timer.GetDeltaTime();

				if (timer_ > 1.0f) {
					timer_ = 0.0f;
					TestCase();
				}
			}

			static void TestCase() {
				ScopedTimer test("awea");
				uint64_t counter = 0;
				uint64_t counter2 = 0;

				auto copy = Get().m_AssetDatabase;

				for (auto& entry : copy | std::views::values) {
					auto newStamp = std::filesystem::last_write_time(Get().m_AppRootPath / entry.path);
					if (entry.pathTimestamp < newStamp) {
						CORI_DEBUG("{}", entry.path.string());
						entry.pathTimestamp = newStamp;
						counter2++;
					}

					counter++;
				}

				CORI_DEBUG("checked '{}', changed '{}'", counter, counter2);
			}


			~AssetManager2() = default;

		private:
			AssetManager2() {
				s_Instance = std::unique_ptr<AssetManager2>(this);

				m_AppRootPath = FileSystem::PathManager::GetAliasedPath("APP_ROOT");
				ScanDirectory(FileSystem::PathManager::GetAliasedPath("ASSET_DIR"));
			}

			static void ProcessFile(const std::filesystem::path& assetFilePath) {
				if (assetFilePath.filename() == "Samplers.json") {
					return;
				}

				struct Layout {
					struct Metadata {
						std::string assetTypename;
						AssetType assetType;
						std::optional<Utility::GlazeWithFallback<AssetDeletionPolicy, AssetDeletionPolicy::eRefCounted, "from Metadata declared in AssetManager::ProcessFile">> assetDeletionPolicy;
					} Metadata;
					glz::raw_json_view AssetData;
				};

				Layout l;
				std::string buffer;
				auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
				if (readError != glz::error_code::none) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Failed to open asset file '{}', skipping it.", assetFilePath.string());
				}

				auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(l, buffer);
				if (parseError) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Asset file '{}' metadata load failed, error: {}", assetFilePath.string(), glz::format_error(parseError, buffer));
					return;
				}

				auto relativeAssetPath = std::filesystem::relative(assetFilePath, Get().m_AppRootPath);
				AssetID pathHash = Utility::HashString64(relativeAssetPath.string());
				auto& entry = Get().m_AssetDatabase[pathHash];

				entry.path = relativeAssetPath;
				entry.pathTimestamp = std::filesystem::last_write_time(Get().m_AppRootPath / entry.path);
				entry.type = l.Metadata.assetType;
				entry.assetTypenameHash = Utility::HashString64(l.Metadata.assetTypename);
				entry.deletionPolicy = l.Metadata.assetDeletionPolicy.value_or(AssetDeletionPolicy::eRefCounted);
			}

			std::unordered_map<AssetID, AssetRecord> m_AssetDatabase;

			std::filesystem::path m_AppRootPath;

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

	template <class T>
	struct to<JSON, Cori::Core::AssetRef<T>> {
		template <auto Opts>
		static void op(const Cori::Core::AssetRef<T>& from, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
			Cori::Core::AssetID id = from.GetAssetID();
			std::string path = Cori::Core::AssetManager2::GetPath(id).value().get().filename().string();
			serialize<JSON>::op<Opts>(path, ctx, b, ix);
		}
	};
}