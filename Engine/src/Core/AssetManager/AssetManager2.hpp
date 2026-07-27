#pragma once
#include <oneapi/tbb/concurrent_vector.h>
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
#include "Utility/BitHelpers.hpp"

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
		concept IsAssetBase = std::derived_from<T, PrimaryAssetBase> || std::derived_from<T, SecondaryAssetBase>;

		template<typename T>
		concept IsRegisteredAsset = requires {
			typename T::Manager;
			requires std::same_as<Utility::StringHash64, std::remove_cvref_t<decltype(AssetTraits<T>::TypeHash)>>;
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

		template<typename T>
		concept CanHotReload = AssetHotReloadEnabled<T> && HasValidSpoke<T>;

		template<IsValidAsset T>
		struct AssetRef {
			//make private later
		//private:
			AssetRef() = default;
		//public:

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

			[[nodiscard]] AssetID GetAssetID() const {
				if (m_Handle.IsSet()) {
					return T::Manager::GetAssetID(m_Handle);
				}

				CORI_CORE_ERROR("GetAssetID called on a moved AssetRef<{}>, returning 0.", CORI_CLEAN_TYPE_NAME(T));

				return 0;
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
			std::filesystem::path path;
			std::filesystem::file_time_type pathTimestamp;
			AssetType type{ AssetType::eUndefined };
			std::string name{};

			uint32_t vectorKey{};

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
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Load failed, no asset with path '{}' found in the data base. Placeholder for '{}' returned.", path, CORI_CLEAN_TYPE_NAME(T));
						return AssetRef<T>(T::Manager::template GetPlaceholder<T>());
					}

					auto& record = Get().m_AssetDatabase[id];
					if (record.assetTypenameHash != AssetTraits<T>::TypeHash) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Load failed, typename hash mismatch, type provided '{}', it's hash '{}', but expected hash is '{}'. Asset with path '{}'. Placeholder will be returned.", CORI_CLEAN_TYPE_NAME(T), AssetTraits<T>::TypeHash, record.assetTypenameHash, path);
						return AssetRef<T>(T::Manager::template GetPlaceholder<T>());
					}

					handle = Handle<T>(record.rawHandleIndex, record.rawHandleVersion);
					if (handle.IsSet()) {
						if (T::Manager::TryAddRef(handle)) {
							return AssetRef<T>::AdoptExisting(handle);
						}
					}

					handle = T::Manager::template AllocateHandle<T>();
					gen = T::Manager::BumpGeneration(handle);
					vectorKey = record.vectorKey;
					T::Manager::BindAsset(handle, id, vectorKey);

					if (GetDeletionPoliciesVector()[vectorKey].load(std::memory_order_acquire) == AssetDeletionPolicy::eKeepAlive) {
						T::Manager::AddRef(handle);
					}

					record.rawHandleIndex = handle.GetIndex();
					record.rawHandleVersion = handle.GetVersion();

					T::Manager::SetAssetStatus(handle, AssetStatus::eLoading);

					fsPath = GetAssetDir() / record.path;
					name = record.name;
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

			static AssetRecord& GetAssetRecord(const AssetID id) {
				CORI_CORE_ASSERT(Get().m_AssetDatabase.contains(id), "Invalid AssetID.");
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

			static CORI_PROFILE_LOCKABLE_TYPE(std::mutex)& GetMutex() {
				return Get().m_Mutex;
			}

			//static tbb::concurrent_vector<std::atomic<AssetStatus>>& GetAssetStatusesVector() {
			//	return Get().m_AssetStatuses;
			//}

			static tbb::concurrent_vector<std::atomic<AssetDeletionPolicy>>& GetDeletionPoliciesVector() {
				return Get().m_DeletionPolicies;
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
				//auto& entry = Get().m_AssetDatabase[pathHash];
				auto [it, isNew] = Get().m_AssetDatabase.try_emplace(pathHash);
				auto& entry = it->second;

				if (isNew) {
					uint32_t vectorKey = Get().m_NextVectorKey++;
					const uint64_t newSizePowerOfTwo = Utility::GetNextPowerOfTwo(vectorKey + 1);

					//if (newSizePowerOfTwo >= Get().m_AssetStatuses.size()) {
					//	Get().m_AssetStatuses.grow_to_at_least(newSizePowerOfTwo);
					//}

					if (newSizePowerOfTwo >= Get().m_DeletionPolicies.size()) {
						Get().m_DeletionPolicies.grow_to_at_least(newSizePowerOfTwo);
					}

					//Get().m_AssetStatuses[vectorKey].store(AssetStatus::eUnloaded, std::memory_order_release);
					Get().m_DeletionPolicies[vectorKey].store(l.Metadata.assetDeletionPolicy.value_or(AssetDeletionPolicy::eRefCounted), std::memory_order_release);

					entry.vectorKey = vectorKey;
				}

				entry.path = std::move(relativeAssetPath);
				entry.pathTimestamp = std::filesystem::last_write_time(Get().m_AppRootPath / entry.path);
				entry.type = l.Metadata.assetType;
				entry.assetTypenameHash = Utility::HashString64(l.Metadata.assetTypename);
			}

			std::unordered_map<AssetID, AssetRecord> m_AssetDatabase;
			//tbb::concurrent_vector<std::atomic<AssetStatus>> m_AssetStatuses;
			tbb::concurrent_vector<std::atomic<AssetDeletionPolicy>> m_DeletionPolicies;
			uint32_t m_NextVectorKey{ 0 };

			std::filesystem::path m_AppRootPath;

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

	template <class T>
	struct to<JSON, Cori::Core::AssetRef<T>> {
		template <auto Opts>
		static void op(const Cori::Core::AssetRef<T>& from, is_context auto&& ctx, auto&& b, auto&& ix) noexcept {
			CORI_CORE_ASSERT(false, "to json was called for AssetRef")
			Cori::Core::AssetID id = from.GetAssetID();
			std::string path = Cori::Core::AssetManager2::GetPath(id).value().get().filename().string();
			serialize<JSON>::op<Opts>(path, ctx, b, ix);
		}
	};
}