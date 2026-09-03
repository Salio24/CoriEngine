#include "AssetManager2.hpp"

#include "Core/Application.hpp"

namespace Cori {
	namespace Core {
		std::unique_ptr<AssetManager2> AssetManager2::s_Instance{ nullptr };

		void AssetManager2::Init() {
			new AssetManager2();
		}

		void AssetManager2::Shutdown() {
			s_Instance.reset();
		}

		AssetManager2& AssetManager2::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling AssetManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void AssetManager2::OnUpdate(GameTimer& timer) {
			static float timer_ = 0.0;
			timer_ += timer.GetDeltaTime();

			if (timer_ > 1.0f) {
				timer_ = 0.0f;
				Application::SubmitWorkerTask([] { ScanAndReload(); });

				for (AssetDirID i = 0; i < Get().m_PublishedDirCount; i++) {
					Application::SubmitWorkerTask([i] {
						ScanDirectory(i);
					});
				}
			}
		}
		std::optional<uint64_t> AssetManager2::GetAssetTypeHash(const AssetID id) {
			if (!IsRegistered(id)) {
				return std::nullopt;
			}

			uint32_t key = Get().m_AssetDatabase[id].vectorKey;
			return Get().m_TypeNameHashes[key];
		}

		uint64_t AssetManager2::GetAssetTypeHash(const uint32_t key) {
			CORI_CORE_ASSERT(Get().m_PublishedCount.load(std::memory_order_acquire) >= key, "Invalid vector key.");

			return Get().m_TypeNameHashes[key];
		}

		uint32_t AssetManager2::GetIdentityVersion(const uint32_t vectorKey) {
			const uint64_t typeHash = GetAssetTypeHash(vectorKey);
			const auto it = Get().m_AssetOps.find(typeHash);
			if (it == Get().m_AssetOps.end()) {
				return 0;
			}

			const AssetTypeOps* ops = it->second.load(std::memory_order_acquire);
			if (!ops || !ops->GetIdentityVersion) {
				return 0;
			}

			return ops->GetIdentityVersion(vectorKey);
		}

		std::expected<std::pair<AssetDependencySet, uint32_t>, ErrorCode> AssetManager2::TryReadDependencies(const uint32_t vectorKey) {
			const uint64_t typeHash = GetAssetTypeHash(vectorKey);
			const auto it = Get().m_AssetOps.find(typeHash);
			if (it == Get().m_AssetOps.end()) {
				return std::unexpected(ErrorCode::eObjectDoesNotExist);
			}

			const AssetTypeOps* ops = it->second.load(std::memory_order_acquire);
			if (!ops || !ops->TryReadDependencies) {
				return std::unexpected(ErrorCode::eNotReady);
			}

			return ops->TryReadDependencies(vectorKey);
		}

		uint32_t AssetManager2::GetDependencyIdentityVersion(const AssetDependency& dependency) {
			const auto it = Get().m_AssetOps.find(dependency.typeHash);
			if (it == Get().m_AssetOps.end()) {
				return 0;
			}

			const AssetTypeOps* ops = it->second.load(std::memory_order_acquire);
			if (!ops || !ops->GetIdentityVersionByHandle) {
				return 0;
			}

			return ops->GetIdentityVersionByHandle(dependency.index, dependency.version);
		}

		AssetStatus AssetManager2::GetDependencyStatus(const AssetDependency& dependency) {
			const auto it = Get().m_AssetOps.find(dependency.typeHash);
			if (it == Get().m_AssetOps.end()) {
				return AssetStatus::eUnspecified;
			}

			const AssetTypeOps* ops = it->second.load(std::memory_order_acquire);
			if (!ops || !ops->GetAssetStatusByHandle) {
				return AssetStatus::eUnspecified;
			}

			return ops->GetAssetStatusByHandle(dependency.index, dependency.version);
		}

		std::optional<uint32_t> AssetManager2::GetAssetVectorKey(const AssetID id) {
			if (!IsRegistered(id)) {
				return std::nullopt;
			}

			return Get().m_AssetDatabase[id].vectorKey;;
		}

		std::string AssetManager2::GetAssetDisplayName(const AssetID id) {
			if (id == 0 || !IsRegistered(id)) {
				return {};
			}

			const uint32_t vectorKey = Get().m_AssetDatabase[id].vectorKey;
			const NonAtomicAssetMeta* meta = Get().m_NonAtomicData[vectorKey].load(std::memory_order_acquire);

			if (!meta) {
				return {};
			}

			if (meta->name) {
				return meta->name.value();
			}

			return meta->jsonPath.stem().string();
		}

		void AssetManager2::ChangePolicy(const AssetID id, const AssetDeletionPolicy newPolicy) {
			CORI_CORE_ASSERT(Get().m_AssetDatabase.contains(id), "Invalid AssetID.");
			auto vectorKey = Get().m_AssetDatabase[id].vectorKey;
			auto typeHash = Get().m_TypeNameHashes[vectorKey].load(std::memory_order_acquire);
			CORI_CORE_ASSERT(Get().m_AssetOps.contains(typeHash), "AssetOps is null for a loaded asset type with type hash {}", typeHash);
			auto assetOps = Get().m_AssetOps[typeHash].load(std::memory_order_acquire);
			assetOps->ChangePolicy(newPolicy, vectorKey);
		}

		AssetRecord& AssetManager2::GetAssetRecord(const AssetID id) {
			CORI_CORE_ASSERT(Get().m_AssetDatabase.contains(id), "Invalid AssetID.");
			return Get().m_AssetDatabase[id];
		}

		void AssetManager2::ScanDirectory(const AssetDirID dirID) {
			Threading::OneAtATime lk(Get().m_ScanRunning);
			if (!lk.TryLock()) {
				return;
			}

			CORI_PROFILE_FUNCTION();

			auto& assetDir = Get().m_AssetDirs[dirID];

			bool isNew = false;

			for (const auto& entry : std::filesystem::recursive_directory_iterator(assetDir.dir)) {
				if (entry.is_regular_file()) {
					if (entry.path().extension() == ".json") {
						isNew |= ProcessFile(entry.path(), dirID);
					}
				}
			}
		}

		void AssetManager2::ScanAndReload() {
			Threading::OneAtATime lk(Get().m_SaRRunning);
			if (!lk.TryLock()) {
				return;
			}

			CORI_PROFILE_FUNCTION();
			uint32_t counter = 0;
			for (const auto& rawHandle : std::ranges::subrange(Get().m_RawHandles.begin(), Get().m_RawHandles.begin() + Get().m_PublishedCount.load(std::memory_order_acquire))) {
				uint32_t currentKey = counter++;

				const auto& dirEntry = Get().m_AssetDirs[Get().m_ParentDirIDs[currentKey].load(std::memory_order_acquire)];
				auto jsonPath = dirEntry.dir / Get().m_NonAtomicData[currentKey].load(std::memory_order_acquire)->jsonPath;

				bool assetDataChanged = false;

				auto oldTime = Get().m_JsonTimestamp[currentKey];
				auto currentTime = last_write_time(jsonPath);
				if (oldTime < currentTime) {
					auto meta = ReloadMetadata(currentKey);
					if (!meta) {
						CORI_ERROR("err");
						continue;
					}

					bool identityUpToDate = CompareIdentityMetadata(meta.value(), currentKey);
					bool mutableUpToDate = CompareMutableMetadata(meta.value(), currentKey);

					if (!identityUpToDate || !mutableUpToDate) {
						// if we change metadata during Load method execution, very bad things can happen
						auto& mutex = GetMutex();
						std::lock_guard lk_(mutex);
						CORI_PROFILER_LOCK_MARK(mutex);

						if (!mutableUpToDate) {
							ApplyMutableMetadata(meta.value(), currentKey);
							mutableUpToDate = true;
						}

						if (!identityUpToDate && rawHandle.load(std::memory_order_acquire) == VersionedHandleBase::Null) {
							ApplyIdentityMetadata(meta.value(), currentKey);
							identityUpToDate = true;
						}
					}

					if (meta.value().assetDataHash != Get().m_AssetDataHashes[currentKey].load(std::memory_order_acquire)) {
						Get().m_AssetDataHashes[currentKey].store(meta.value().assetDataHash, std::memory_order_release);
						assetDataChanged = true;
					}

					if (identityUpToDate) {
						Get().m_JsonTimestamp[currentKey] = currentTime;
					}
				}

				if (rawHandle.load(std::memory_order_acquire) == VersionedHandleBase::Null) {
					continue;
				}

				auto typeHash = Get().m_TypeNameHashes[currentKey].load(std::memory_order_acquire);
				CORI_CORE_ASSERT(Get().m_AssetOps.contains(typeHash), "AssetOps is null for a loaded asset type with type hash {}", typeHash);
				auto assetOps = Get().m_AssetOps[typeHash].load(std::memory_order_acquire);
				if (!assetOps->hotReload) {
					continue;
				}

				if (assetDataChanged) {
					//mark manually reloadable
					if (assetOps->autoHotReload) {
						CORI_DEBUG("reloading cuz of json {}", currentKey);
						assetOps->Reload(currentKey);
						continue;
					}
				}

				bool reload = false;
				auto nonAtomicData = Get().m_NonAtomicData[currentKey].load(std::memory_order_acquire);
				if (nonAtomicData->assetFiles) {
					for (const auto& [assetPath, time] : std::views::zip(nonAtomicData->assetFiles.value(), Get().m_AssetFileStamps[currentKey])) {
						auto newTime = last_write_time(assetPath);
						if (newTime > time) {
							time = newTime;
							reload = true;
						}
					}
				}

				if (reload) {
					if (assetOps->autoHotReload) {
						CORI_DEBUG("reloading cuz of asset files {}", currentKey);
						assetOps->Reload(currentKey);
					}
				}
			}
		}

		std::optional<AssetDirID> AssetManager2::AddAssetDir(std::string label, const std::filesystem::path& dir) {
			if (!exists(dir)) {
				return std::nullopt;
			}

			for (auto& assetDir : Get().m_AssetDirs) {
				if (assetDir.label == label) {
					return std::nullopt;
				}
			}

			AssetDirID key = Get().m_PublishedDirCount.load(std::memory_order_relaxed);
			const uint64_t newSizePowerOfTwo = Utility::GetNextPowerOfTwo(key + 1);
			if (newSizePowerOfTwo >= Get().m_AssetDirs.size()) {
				Get().m_AssetDirs.grow_to_at_least(newSizePowerOfTwo);
			}

			auto& assetDir = Get().m_AssetDirs[key];
			assetDir.label = std::move(label);
			assetDir.dir = dir;
			assetDir.dirTimestamp.store(last_write_time(dir), std::memory_order_relaxed);
			assetDir.gen.fetch_add(1, std::memory_order_relaxed);
			assetDir.id = key;

			Get().m_PublishedDirCount.fetch_add(1, std::memory_order_release);

			return key;
		}

		AssetManager2::~AssetManager2() {
			for (auto* ptr : m_AllSlots) {
				delete ptr;
			}
		}

		std::optional<AssetManager2::ProcessedMetadata> AssetManager2::ReloadMetadata(const uint32_t vectorKey) {
			JsonLayout l;
			std::string buffer;
			auto path = Get().m_AssetDirs[Get().m_ParentDirIDs[vectorKey]].dir / Get().m_NonAtomicData[vectorKey].load(std::memory_order_acquire)->jsonPath;
			auto readError = glz::file_to_buffer(buffer, path.c_str());
			if (readError != glz::error_code::none) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Failed to open asset file '{}', skipping it.", path.string());
				return std::nullopt;
			}

			auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(l, buffer);
			if (parseError) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Asset file '{}' metadata load failed, error: {}", path.string(), glz::format_error(parseError, buffer));
				return std::nullopt;
			}

			return ProcessedMetadata{
				.typeNameHash = Utility::HashString64(l.Metadata.assetTypename),
				.assetDataHash = Utility::HashString64(l.AssetData.str),
				.type = l.Metadata.assetType,
				.policy = l.Metadata.assetDeletionPolicy.value_or(AssetDeletionPolicy::eRefCounted),
				.assetFiles = std::move(l.Metadata.assetFiles),
				.name = std::move(l.Metadata.name)
			};
		}

		bool AssetManager2::CompareIdentityMetadata(const ProcessedMetadata& meta, const uint32_t vectorKey) {
			if (meta.typeNameHash != Get().m_TypeNameHashes[vectorKey].load(std::memory_order_acquire)) {
				return false;
			}
			if (meta.type != Get().m_AssetTypes[vectorKey].load(std::memory_order_acquire)) {
				return false;
			}

			return true;
		}

		bool AssetManager2::CompareMutableMetadata(const ProcessedMetadata& meta, const uint32_t vectorKey) {
			if (meta.policy != Get().m_DeletionPolicies[vectorKey].load(std::memory_order_acquire)) {
				return false;
			}
			auto nonAtomicMeta = Get().m_NonAtomicData[vectorKey].load(std::memory_order_acquire);
			if (!meta.name) {
				if (nonAtomicMeta->name) {
					return false;
				}
			} else {
				if (!nonAtomicMeta->name) {
					return false;
				}
				if (nonAtomicMeta->name.value() != meta.name) {
					return false;
				}
			}

			if (!meta.assetFiles || meta.assetFiles->empty()) {
				if (nonAtomicMeta->assetFiles) {
					return false;
				}
			} else {
				if (!nonAtomicMeta->assetFiles) {
					return false;
				}
				if (nonAtomicMeta->assetFiles.value().size() != meta.assetFiles.value().size()) {
					return false;
				}
				const auto basePath = GetAssetFileBasePath(vectorKey, nonAtomicMeta);
				for (const auto& [oldPath, newPath] : std::views::zip(nonAtomicMeta->assetFiles.value(), meta.assetFiles.value())) {
					if (oldPath != basePath / newPath) {
						return false;
					}
				}
			}

			return true;
		}

		void AssetManager2::ApplyMutableMetadata(ProcessedMetadata& meta, const uint32_t vectorKey) {
			auto typeHash = Get().m_TypeNameHashes[vectorKey].load(std::memory_order_acquire);
			CORI_CORE_ASSERT(Get().m_AssetOps.contains(typeHash), "AssetOps is null for a loaded asset type with type hash {}", typeHash);
			auto assetOps = Get().m_AssetOps[typeHash].load(std::memory_order_acquire);
			assetOps->ChangePolicy(meta.policy, vectorKey);

			auto oldNonAtomicMeta = Get().m_NonAtomicData[vectorKey].load(std::memory_order_acquire);

			auto* newNonAtomicMeta = new NonAtomicAssetMeta();
			newNonAtomicMeta->jsonPath = oldNonAtomicMeta->jsonPath;
			newNonAtomicMeta->name = std::move(meta.name);
			//FIXME: propagate the name change to the asset spoke

			auto& stampsVec = Get().m_AssetFileStamps[vectorKey];
			stampsVec.clear();
			if (meta.assetFiles && !meta.assetFiles->empty()) {
				const auto basePath = GetAssetFileBasePath(vectorKey, newNonAtomicMeta);
				newNonAtomicMeta->assetFiles = std::vector<std::filesystem::path>{};
				for (const auto& assetPath : meta.assetFiles.value()) {
					std::filesystem::path resolvedPath = basePath / assetPath;
					auto time = last_write_time(resolvedPath);
					newNonAtomicMeta->assetFiles->emplace_back(std::move(resolvedPath));
					stampsVec.emplace_back(time);
				}
			}

			Get().m_NonAtomicData[vectorKey].store(newNonAtomicMeta, std::memory_order_release);
			Get().m_AllSlots.emplace_back(newNonAtomicMeta);
		}

		bool AssetManager2::ProcessFile(const std::filesystem::path& assetFilePath, const AssetDirID dirID) {
			if (assetFilePath.filename() == "Samplers.json") {
				return false;
			}

			const auto& dirEntry = Get().m_AssetDirs[dirID];

			std::string hashString;
			auto relativeAssetPath = std::filesystem::relative(assetFilePath, dirEntry.dir);
			hashString.reserve(dirEntry.label.size() + 3 + relativeAssetPath.native().size());
			hashString.append(dirEntry.label);
			hashString.append("://");
			hashString.append(relativeAssetPath.generic_string());

			AssetID pathHash = Utility::HashString64(hashString);

			if (Get().m_AssetDatabase.contains(pathHash)) {
				return false;
			}

			JsonLayout l;
			std::string buffer;
			auto readError = glz::file_to_buffer(buffer, assetFilePath.c_str());
			if (readError != glz::error_code::none) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Failed to open asset file '{}', skipping it.", assetFilePath.string());
				return false;
			}

			auto parseError = glz::read<Utility::ReflectEnumsOpts{}>(l, buffer);
			if (parseError) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::AssetManager }, "Asset file '{}' metadata load failed, error: {}", assetFilePath.string(), glz::format_error(parseError, buffer));
				return false;
			}

			AssetRecord entry;

			uint32_t vectorKey = Get().m_NextVectorKey++;
			entry.vectorKey = vectorKey;

			const uint64_t newSizePowerOfTwo = Utility::GetNextPowerOfTwo(vectorKey + 1);

			if (newSizePowerOfTwo >= Get().m_DeletionPolicies.size()) {
				Get().m_DeletionPolicies.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_TypeNameHashes.size()) {
				Get().m_TypeNameHashes.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_RawHandles.size()) {
				Get().m_RawHandles.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_NonAtomicData.size()) {
				Get().m_NonAtomicData.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_JsonTimestamp.size()) {
				Get().m_JsonTimestamp.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_AssetDataHashes.size()) {
				Get().m_AssetDataHashes.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_AssetTypes.size()) {
				Get().m_AssetTypes.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_ParentDirIDs.size()) {
				Get().m_ParentDirIDs.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_ReverseLookup.size()) {
				Get().m_ReverseLookup.grow_to_at_least(newSizePowerOfTwo);
			}

			if (newSizePowerOfTwo >= Get().m_AssetFileStamps.size()) {
				Get().m_AssetFileStamps.grow_to_at_least(newSizePowerOfTwo);
			}

			Get().m_DeletionPolicies[vectorKey].store(l.Metadata.assetDeletionPolicy.value_or(AssetDeletionPolicy::eRefCounted), std::memory_order_release);

			Get().m_JsonTimestamp[vectorKey] = std::filesystem::last_write_time(assetFilePath);
			Get().m_AssetDataHashes[vectorKey].store(Utility::HashString64(l.AssetData.str), std::memory_order_release);
			Get().m_ReverseLookup[vectorKey] = pathHash;

			Get().m_TypeNameHashes[vectorKey].store(Utility::HashString64(l.Metadata.assetTypename), std::memory_order_release);
			Get().m_AssetTypes[vectorKey].store(l.Metadata.assetType, std::memory_order_release);

			Get().m_ParentDirIDs[vectorKey].store(dirID, std::memory_order_release);

			auto* nonAtomicMeta = new NonAtomicAssetMeta();
			nonAtomicMeta->name = std::move(l.Metadata.name);
			nonAtomicMeta->jsonPath = std::move(relativeAssetPath);
			if (l.Metadata.assetFiles) {
				auto& val = l.Metadata.assetFiles.value();
				auto& stampsVec = Get().m_AssetFileStamps[vectorKey];
				stampsVec.clear();
				if (!val.empty()) {
					nonAtomicMeta->assetFiles = std::vector<std::filesystem::path>{};
					for (auto& assetPath : val) {
						std::filesystem::path path = dirEntry.dir / nonAtomicMeta->jsonPath.parent_path() /assetPath;
						auto time = last_write_time(path);
						nonAtomicMeta->assetFiles->emplace_back(std::move(path));
						stampsVec.emplace_back(time);
					}
				}
			}

			Get().m_NonAtomicData[vectorKey].store(nonAtomicMeta, std::memory_order_release);
			Get().m_AllSlots.emplace_back(nonAtomicMeta);

			Get().m_RawHandles[vectorKey].store(VersionedHandleBase::Null, std::memory_order_release);

			Get().m_AssetDatabase.emplace(pathHash, entry);

			Get().m_PublishedCount.store(Get().m_NextVectorKey, std::memory_order_release);
			return true;
		}

	}
}
