#pragma once

namespace Cori {

	// MAKE SURE THAT THE CTORARGS ARE THE SAME IN CreateShared\Unique and in RegisterInSharedFactory\UniqueFactory

	// probuably will get rid of this types of macros, they're more annoying than helpful, intellisense is freaking out because of them

	// make unique factory as well
	template <typename BaseType, typename KeyType, typename... CtorArgs>
	class Factory {
	public:
		using SharedCreator = std::function<std::shared_ptr<BaseType>(CtorArgs...)>;
		using UniqueCreator = std::function<std::unique_ptr<BaseType>(CtorArgs...)>;

		static Factory& Instance() {
			static Factory factory;
			return factory;
		}

		bool RegisterShared(const KeyType& key, SharedCreator creator) {

			if (m_SharedCreators.count(key)) {
				return false;
			}

			m_SharedCreators.insert({ key, creator });
			return true;
		}

		bool RegisterUnique(const KeyType& key, UniqueCreator creator) {
			if (m_UniqueCreators.count(key)) {
				return false;
			}

			m_UniqueCreators.insert({ key, creator });
			return true;
		}

		static std::shared_ptr<BaseType> CreateShared(const KeyType& key, CtorArgs... ctorArgs) {
			return Instance().CreateSharedImpl(key, std::forward<CtorArgs>(ctorArgs)...);
		}

		static std::unique_ptr<BaseType> CreateUnique(const KeyType& key, CtorArgs... ctorArgs) {
			return Instance().CreateUniqueImpl(key, std::forward<CtorArgs>(ctorArgs)...);
		}

	private:
		std::shared_ptr<BaseType> CreateSharedImpl(const KeyType& key, CtorArgs... ctorArgs) {

			auto it = m_SharedCreators.find(key);
			if (it == m_SharedCreators.end()) {
				CORI_CORE_ERROR("Factory: No shared creator registered for Key: '{0}' (numeric: {1}) (BaseType: '{2}')", typeid(key).name(), static_cast<int>(key), Logger::ColoredText(typeid(BaseType).name(), fmt::color::violet));
				return nullptr;
			}
			return it->second(std::forward<CtorArgs>(ctorArgs)...);
		}

		std::unique_ptr<BaseType> CreateUniqueImpl(const KeyType& key, CtorArgs... ctorArgs) {
			auto it = m_UniqueCreators.find(key);
			if (it == m_UniqueCreators.end()) {
				CORI_CORE_ERROR("Factory: No shared unique registered for Key: '{0}' (numeric: {1}) (BaseType: '{2}')", typeid(key).name(), static_cast<int>(key), Logger::ColoredText(typeid(BaseType).name(), fmt::color::violet));
				return nullptr;
			}
			return it->second(std::forward<CtorArgs>(ctorArgs)...);
		}
		Factory() = default;
		~Factory() = default;
		Factory(const Factory&) = delete;
		Factory& operator=(const Factory&) = delete;
		Factory(Factory&&) = delete;
		Factory& operator=(Factory&&) = delete;

		std::map<KeyType, SharedCreator> m_SharedCreators;
		std::map<KeyType, UniqueCreator> m_UniqueCreators;
	};


	// CRTP base class for self-registration
	template<typename BaseType, typename DerivedType, typename KeyType, KeyType KeyValue, typename... CtorArgs>
	class RegisterInSharedFactory {
	public:
		static std::shared_ptr<BaseType> Create(CtorArgs&&... ctorArgs) {
			if (DerivedType::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				return std::make_shared<DerivedType>(std::forward<CtorArgs>(ctorArgs)...);
			}
			else {
				CORI_CORE_ERROR("RegisterInSharedFactory: PreCreateHook failed for factory with type: {0}, nullptr returned", Logger::ColoredText(typeid(BaseType).name(), fmt::color::violet));
				return nullptr;
			}
		}

	protected:

		inline static bool _RegisterShared = Factory<BaseType, KeyType, CtorArgs...>::Instance().RegisterShared(KeyValue, Create);

		RegisterInSharedFactory() = default;
		~RegisterInSharedFactory() = default;

		RegisterInSharedFactory(const RegisterInSharedFactory&) = delete;
		RegisterInSharedFactory& operator=(const RegisterInSharedFactory&) = delete;
		RegisterInSharedFactory(RegisterInSharedFactory&&) = delete;
		RegisterInSharedFactory& operator=(RegisterInSharedFactory&&) = delete;

	};

	template<typename BaseType, typename DerivedType, typename KeyType, KeyType KeyValue, typename... CtorArgs>
	class RegisterInUniqueFactory {
	public:
		static std::unique_ptr<BaseType> Create(CtorArgs&&... ctorArgs) {
			if (DerivedType::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				return std::make_unique<DerivedType>(std::forward<CtorArgs>(ctorArgs)...);
			}
			else {
				CORI_CORE_ERROR("RegisterInUniqueFactory: PreCreateHook failed for factory with type: {0}, nullptr returned", Logger::ColoredText(typeid(BaseType).name(), fmt::color::violet));
				return nullptr;
			}
		}

	protected:

		inline static bool _RegisterUnique = Factory<BaseType, KeyType, CtorArgs...>::Instance().RegisterUnique(KeyValue, Create);

		RegisterInUniqueFactory() = default;
		~RegisterInUniqueFactory() = default;

		RegisterInUniqueFactory(const RegisterInUniqueFactory&) = delete;
		RegisterInUniqueFactory& operator=(const RegisterInUniqueFactory&) = delete;
		RegisterInUniqueFactory(RegisterInUniqueFactory&&) = delete;
		RegisterInUniqueFactory& operator=(RegisterInUniqueFactory&&) = delete;

	};

#define CORI_DECLARE_SHARED_FACTORY_REGISTERED(ClassName, CtorArgs) \
public: \
static bool PreCreateHook CtorArgs; \
	ClassName CtorArgs; \
private: \
	struct StaticInitHelper { \
	    StaticInitHelper() { \
		    (void)_RegisterShared; \
	    } \
	}; \
	inline static StaticInitHelper s_InitHelper;

#define CORI_DEFINE_SHARED_FACTORY_REGISTERED(ClassName, HookBody, CtorArgs, ...) \
ClassName::ClassName CtorArgs \
	__VA_ARGS__ \
	bool ClassName::PreCreateHook CtorArgs { \
		HookBody \
		return true; \
	} \
	static_assert(true, "Semicolon expected after CORI_DEFINE_SHARED_FACTORY_REGISTERED")

#define CORI_DECLARE_UNIQUE_FACTORY_REGISTERED(ClassName, CtorArgs) \
public: \
static bool PreCreateHook CtorArgs; \
	ClassName CtorArgs; \
private: \
	struct StaticInitHelper { \
	    StaticInitHelper() { \
		    (void)_RegisterUnique; \
	    } \
	}; \
	inline static StaticInitHelper s_InitHelper;

#define CORI_DEFINE_UNIQUE_FACTORY_REGISTERED(ClassName, HookBody, CtorArgs, ...) \
ClassName::ClassName CtorArgs \
	__VA_ARGS__ \
	bool ClassName::PreCreateHook CtorArgs { \
		HookBody \
		return true; \
	} \
	static_assert(true, "Semicolon expected after CORI_DEFINE_UNIQUE_FACTORY_REGISTERED")

}