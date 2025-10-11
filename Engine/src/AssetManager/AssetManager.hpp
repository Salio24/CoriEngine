#pragma once
namespace Cori {
	namespace Core {
		class Application;
	}

	namespace Internal {
		/**
		 * @brief Checks T can be considered a descriptor.
		 */
		template<typename T>
		concept IsDescriptor = requires(const T& a, const T& b) {
			{ a.GetRuntimeID() } -> std::same_as<uint32_t>;
			{ a.m_Name } -> std::convertible_to<std::string>;
			{ a == b } -> std::convertible_to<bool>;
			typename T::AssetType;
			typename T::Hasher;
		};

		/**
		 * @brief Checks if AssetType of Descriptor can be loaded by the AssetManager.
		 */
		template<typename Descriptor>
		concept CanBeLoaded = IsDescriptor<Descriptor> && requires(const Descriptor& d) {
			{ Descriptor::AssetType::Create(d) } -> std::same_as<std::shared_ptr<typename Descriptor::AssetType>>;
		};

		/**
		 * @brief Checks if Asset is an asset type that AssetManager can work with.
		 */
		template<typename Asset>
		concept IsAsset = CanBeLoaded<typename Asset::Descriptor>;
	}

	//PS: This is ass, need to move to using handles + uuid/metadata + hub-and-spoke model. And this will allow async loading.

	/**
	 * @brief Used when you want to manually control the asset lifetime, loading, preloading, unloading.
	 * @details Mainly used for assets that are not bound to any particular object, but you want to keep them alive.
	 * \n For example: Fonts, AnimationPacks, Sounds, etc. Because all asset lifetimes in Cori are managed by a shared pointers,
	 * if not for the AssetManager these asset would be unloaded as soon as some object is done with them and refcount dropped to 0,
	 * AssetManger is a convenient place to keep these objects loaded and alive.
	 * \n For loading and later reviving the asset AssetManger uses descriptors that describe how to load a particular asset.
	 */
	class AssetManager {
		struct Cache {
			std::unordered_map<std::type_index, std::any> m_Caches;
			std::mutex m_CacheMutex;
			std::unordered_map<std::type_index, std::any> m_Placeholders;
		};

	public:
		/**
		 * @brief Gets the asset from the asset manager cache, it works with any asset that has a Descriptor defined.
		 * @tparam Descriptor Will be deduced, no need to specify.
		 * @param descriptor Instance of the asset descriptor.
		 * @return A shared pointer to the loaded asset.
		 */
		template <Internal::CanBeLoaded Descriptor>
		static std::shared_ptr<typename Descriptor::AssetType> Get(const Descriptor& descriptor) {
			CORI_PROFILE_FUNCTION();
			std::lock_guard lock(s_Cache->m_CacheMutex);

			auto& cache = GetCache<typename Descriptor::AssetType>();
			if (const auto it = cache.find(descriptor.GetRuntimeID()); it != cache.end()) {
				return it->second;
			}

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::AssetManager::Self }, "Cache miss for type <{}>, name: '{}' (RuntimeID: {}). Loading...", CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType) , descriptor.m_Name, descriptor.GetRuntimeID());

			std::shared_ptr<typename Descriptor::AssetType> newAsset = Descriptor::AssetType::Create(descriptor);
			cache[descriptor.GetRuntimeID()] = newAsset;
			return newAsset;
		}


		/**
		 * @brief Preloads a group (or one) of assets into the cache.
		 * @tparam Descriptor Will be deduced, no need to specify.
		 * @param descriptors An list of asset descriptors to preload, all should have the same type, can't mix different asset descriptor types in one call.
		 * @details It is needed to avoid the stutter that will be caused when the asset is requested but not yet loaded.
		 */
		template <Internal::CanBeLoaded Descriptor>
		static void Preload(const std::initializer_list<Descriptor> descriptors) {

			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Preloading {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
			for (const auto& descriptor : descriptors) {
				Get(descriptor);
			}
			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Preloaded {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
		}

		/**
		 * @brief Unloads a group (or one) of assets from the cache.
		 * @tparam Descriptor Will be deduced, no need to specify.
		 * @param descriptors An list of asset descriptors to unloaded, all should have the same type, can't mix different asset descriptor types in one call.
		 * @note If an asset is still used somewhere (ref count > 1) it will be removed from cache, but freed only when the ref count drops to 0.
		 */
		template <Internal::IsDescriptor Descriptor>
		static void Unload(const std::initializer_list<Descriptor>& descriptors) {
			std::lock_guard lock(s_Cache->m_CacheMutex);

			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Unloading {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
			for (const auto& descriptor : descriptors) {
				auto& cache = GetCache<typename Descriptor::AssetType>();

				if (!cache.contains(descriptor.GetRuntimeID())) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::AssetManager::Self }, "Trying to unload <{}> that is not loaded, name '{}', (RuntimeID: {})", CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType), descriptor.m_Name, descriptor.GetRuntimeID());
					return;
				}

				auto ptr = cache.at(descriptor.GetRuntimeID());
				if (ptr.use_count() > 1) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::AssetManager::Self }, "Trying to unload <{}>, name '{}' that is still used somewhere, it will be freed, as soon as refcount drops to 0. (RuntimeID: {})", CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType), descriptor.m_Name, descriptor.GetRuntimeID());
				}

				cache.erase(descriptor.GetRuntimeID());
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::AssetManager::Self }, "Unloaded <{}>, name: '{}' (RuntimeID: {}).", CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType), descriptor.m_Name, descriptor.GetRuntimeID());
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Unloaded {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
		}

		/**
		 * @brief This allows you to register a placeholder for a specific asset that you can later retrieve.
		 * @tparam AssetType Type of asset to register placeholder for.
		 * @param placeholderInstance Asset instance that we will consider a placeholder.
		 */
		template<Internal::IsAsset AssetType>
		static void RegisterPlaceholder(const std::shared_ptr<AssetType>& placeholderInstance) {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Registering placeholder for type <{}>", CORI_CLEAN_TYPE_NAME(AssetType));
			if (!s_Cache->m_Placeholders.contains(std::type_index(typeid(AssetType)))) {
				s_Cache->m_Placeholders[std::type_index(typeid(AssetType))] = placeholderInstance;
				CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Registered placeholder for type <{}>", CORI_CLEAN_TYPE_NAME(AssetType));
				return;
			}

			CORI_CORE_WARN_TAGGED({ Logger::Tags::AssetManager::Self }, "Placeholder for type <{}> already exists.", CORI_CLEAN_TYPE_NAME(AssetType));
		}

		/**
		 * @brief Retrieves the instance of placeholder for a specified asset type.
		 * @tparam AssetType Asset type to retrieve a placeholder for.
		 * @return Asset placeholder instnace of success, nullptr if placeholder for this asset wasn't registered.
		 */
		template<Internal::IsAsset AssetType>
		static std::shared_ptr<AssetType> GetPlaceholder() {
			if (s_Cache->m_Placeholders.contains(std::type_index(typeid(AssetType)))) {
				return std::any_cast<std::shared_ptr<AssetType>>(s_Cache->m_Placeholders[std::type_index(typeid(AssetType))]);
			}

			CORI_CORE_ERROR_TAGGED({ Logger::Tags::AssetManager::Self }, "Placeholder for type <{}> doesnt exists, returned nullptr.", CORI_CLEAN_TYPE_NAME(AssetType));
			return nullptr;
		}

		/**
		 * @brief Checks if an asset type have placeholder registered.
		 * @tparam AssetType Asset type to check the existence of placeholder for.
		 * @return True if there is a placeholder registered, false otherwise.
		 */
		template<Internal::IsAsset AssetType>
		static bool HasPlaceholder() {
			return s_Cache->m_Placeholders.contains(std::type_index(typeid(AssetType)));
		}

		/**
		 * @brief Clears cache for o specified asset type.
		 * @tparam AssetType The type os asset that we want to clear cache for.
		 * @note If an asset is still used somewhere (ref count > 1) it will be removed from cache, but freed only when the ref count drops to 0.
		 */
		template <typename AssetType>
		static void ClearCache() {
			std::lock_guard lock(s_Cache->m_CacheMutex);

			if (s_Cache->m_Caches.contains(std::type_index(typeid(AssetType)))) {
				GetCache<AssetType>().clear();
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::AssetManager::Self }, "Cleared cache for type <{}>", CORI_CLEAN_TYPE_NAME(AssetType));
			}
		}
	private:
		friend Core::Application;
		static void Init();
		static void Shutdown();
		static void RegisterPlaceholders();

		template <typename AssetType>
		static std::unordered_map<uint32_t, std::shared_ptr<AssetType>>& GetCache() {
			const auto typeIndex = std::type_index(typeid(AssetType));
			if (!s_Cache->m_Caches.contains(typeIndex)) {
				s_Cache->m_Caches[typeIndex] = std::unordered_map<uint32_t, std::shared_ptr<AssetType>>();
			}
			return std::any_cast<std::unordered_map<uint32_t, std::shared_ptr<AssetType>>&>(s_Cache->m_Caches.at(typeIndex));
		}

		static Cache* s_Cache;
	};
}
