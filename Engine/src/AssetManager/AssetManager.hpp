#pragma once
namespace Cori {
	namespace Internal {
		template <typename T>
		concept IsDescriptor = requires(const T& a, const T& b) {
			{ a.GetRuntimeID() } -> std::same_as<uint32_t>;
			{ a.m_Name } -> std::convertible_to<std::string>;
			{ a == b } -> std::convertible_to<bool>;
			typename T::AssetType;
			typename T::Hasher;
		};

		template <typename Descriptor>
		concept CanBeDefaultLoaded = IsDescriptor<Descriptor> && requires(const Descriptor& d) {
			{ Descriptor::AssetType::Create(d) } -> std::same_as<std::shared_ptr<typename Descriptor::AssetType>>;
		};
	}

	class AssetManager {
		struct Cache {
			std::unordered_map<std::type_index, std::any> m_Caches;
		};

	public:
		static void Init();
		static void Shutdown();

		template <Internal::CanBeDefaultLoaded Descriptor>
		static std::shared_ptr<typename Descriptor::AssetType> Get(const Descriptor& descriptor) {
			CORI_PROFILE_FUNCTION();

			auto& cache = GetCache<typename Descriptor::AssetType>();
			if (const auto it = cache.find(descriptor.GetRuntimeID()); it != cache.end()) {
				return it->second;
			}

			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::AssetManager::Self }, "Cache miss for type <{}>, name: '{}' (RuntimeID: {}). Loading...", CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType) , descriptor.m_Name, descriptor.GetRuntimeID());

			std::shared_ptr<typename Descriptor::AssetType> newAsset = Descriptor::AssetType::Create(descriptor);
			cache[descriptor.GetRuntimeID()] = newAsset;
			return newAsset;
		}

		template <Internal::CanBeDefaultLoaded Descriptor>
		static void Preload(const std::initializer_list<Descriptor> descriptors) {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Preloading {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
			for (const auto& descriptor : descriptors) {
				Get(descriptor);
			}
			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Preloaded {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
		}

		template <Internal::IsDescriptor Descriptor>
		static void Unload(const std::initializer_list<Descriptor>& descriptors) {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Unloading {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
			for (const auto& descriptor : descriptors) {
				auto& cache = GetCache<typename Descriptor::AssetType>();

				if (!cache.contains(descriptor.GetRuntimeID())) {
					CORI_CORE_WARN_TAGGED({ Logger::Tags::AssetManager::Self }, "Trying to unload <{}> that is not loaded, name '{}', (RuntimeID: {})", CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType), descriptor.m_Name, descriptor.GetRuntimeID());
					return;
				}
				cache.erase(descriptor.GetRuntimeID());
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::AssetManager::Self }, "Unloaded <{}>, name: '{}' (RuntimeID: {}).", CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType), descriptor.m_Name, descriptor.GetRuntimeID());
			}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::AssetManager::Self }, "Unloaded {} <{}(s/es)>", descriptors.size(), CORI_CLEAN_TYPE_NAME(typename Descriptor::AssetType));
		}

		template <typename AssetType>
		static void ClearCache() {
			if (s_Cache->m_Caches.contains(std::type_index(typeid(AssetType)))) {
				GetCache<AssetType>().clear();
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::AssetManager::Self }, "Cleared cache for type <{}>", CORI_CLEAN_TYPE_NAME(AssetType));
			}
		}

	private:
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
