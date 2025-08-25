#pragma once

namespace Cori {
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
			if (!Instance().m_SharedCreators.contains(key)) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Factory::Self, Logger::Tags::Core::Factory::Shared }, "No creator registered for BaseType '{}' with KeyType '{}'", CORI_CLEAN_TYPE_NAME(BaseType), CORI_CLEAN_TYPE_NAME(key));
				return nullptr;
			}
			return Instance().m_SharedCreators.at(key)(std::forward<CtorArgs>(ctorArgs)...);
		}

		static std::unique_ptr<BaseType> CreateUnique(const KeyType& key, CtorArgs... ctorArgs) {
			if (!Instance().m_UniqueCreators.contains(key)) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Factory::Self, Logger::Tags::Core::Factory::Unique }, "No creator registered for BaseType '{}' with KeyType '{}'", CORI_CLEAN_TYPE_NAME(BaseType), CORI_CLEAN_TYPE_NAME(key));
				return nullptr;
			}
			return Instance().m_UniqueCreators.at(key)(std::forward<CtorArgs>(ctorArgs)...);
		}

	private:

		Factory() = default;
		~Factory() = default;
		Factory(const Factory&) = delete;
		Factory& operator=(const Factory&) = delete;
		Factory(Factory&&) = delete;
		Factory& operator=(Factory&&) = delete;

		std::unordered_map<KeyType, SharedCreator> m_SharedCreators;
		std::unordered_map<KeyType, UniqueCreator> m_UniqueCreators;
	};


	// CRTP base class for self-registration
	template <typename BaseType, typename DerivedType, typename KeyType, KeyType KeyValue, typename... CtorArgs>
	class RegisterInFactory {
		static std::shared_ptr<BaseType> CreateShared(CtorArgs&&... ctorArgs) {
			if (DerivedType::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				return std::make_shared<DerivedType>(std::forward<CtorArgs>(ctorArgs)...);
			}
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Factory::Self, Logger::Tags::Core::Factory::Register, Logger::Tags::Core::Factory::Shared }, "PreCreateHook failed for type: {}, nullptr returned", CORI_CLEAN_TYPE_NAME(BaseType));
			return nullptr;
		}

		static std::unique_ptr<BaseType> CreateUnique(CtorArgs&&... ctorArgs) {
			if (DerivedType::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				return std::make_unique<DerivedType>(std::forward<CtorArgs>(ctorArgs)...);
			}
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Factory::Self, Logger::Tags::Core::Factory::Register, Logger::Tags::Core::Factory::Unique }, "PreCreateHook failed for type: {}, nullptr returned", CORI_CLEAN_TYPE_NAME(BaseType));
			return nullptr;
		}

	protected:
		inline static bool RegisterShared = Factory<BaseType, KeyType, CtorArgs...>::Instance().RegisterShared(KeyValue, CreateShared);

		inline static bool RegisterUnique = Factory<BaseType, KeyType, CtorArgs...>::Instance().RegisterUnique(KeyValue, CreateUnique);
	};

#define CORI_REGISTERED_FACTORY_INIT \
private: \
	struct StaticInitHelper { \
	    StaticInitHelper() { \
		    (void)RegisterShared; \
		    (void)RegisterUnique; \
	    } \
	}; \
	inline static StaticInitHelper s_InitHelper

}