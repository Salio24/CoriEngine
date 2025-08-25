#pragma once

namespace Cori {

	template<typename...>
	class SharedSelfFactory;

	template <typename Type>
	class SharedSelfFactory<Type> {
	protected:
		SharedSelfFactory() = default;
		~SharedSelfFactory() = default;

	public:
		SharedSelfFactory(const SharedSelfFactory&) = delete;
		SharedSelfFactory& operator=(const SharedSelfFactory&) = delete;
		SharedSelfFactory(SharedSelfFactory&&) = delete;
		SharedSelfFactory& operator=(SharedSelfFactory&&) = delete;

		template <typename... CtorArgs>
		static std::shared_ptr<Type> Create(CtorArgs&&... ctorArgs) {
			if (Type::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				class TypeProxy : public Type {
				public:
					explicit TypeProxy(CtorArgs&&... proxyCtorArgs)
						: Type(std::forward<CtorArgs>(proxyCtorArgs)...) {
					}
				};

				return std::make_shared<TypeProxy>(std::forward<CtorArgs>(ctorArgs)...);
			}
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Factory::SelfFactory, Logger::Tags::Core::Factory::Shared }, "PreCreateHook failed for shared factory with type: {}, nullptr returned", CORI_CLEAN_TYPE_NAME(Type));
			return nullptr;
		}
	};

	template <typename Type, typename ErrorType>
	class SharedSelfFactory<Type, ErrorType> {
	protected:
		SharedSelfFactory() = default;
		~SharedSelfFactory() = default;

	public:
		SharedSelfFactory(const SharedSelfFactory&) = delete;
		SharedSelfFactory& operator=(const SharedSelfFactory&) = delete;
		SharedSelfFactory(SharedSelfFactory&&) = delete;
		SharedSelfFactory& operator=(SharedSelfFactory&&) = delete;


		template <typename... CtorArgs>
		static std::expected<std::shared_ptr<Type>, ErrorType> Create(CtorArgs&&... ctorArgs) {
			return std::move(Type::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)).transform([...ctorArgs = std::forward<CtorArgs>(ctorArgs)]() mutable {
				class TypeProxy : public Type {
				public:
					explicit TypeProxy(CtorArgs&&... proxyCtorArgs)
						: Type(std::forward<CtorArgs>(proxyCtorArgs)...) {}
				};

				return std::make_shared<TypeProxy>(std::forward<CtorArgs>(ctorArgs)...);
			});
		}
	};

	template<typename...>
	class UniqueSelfFactory;

	template <typename Type>
	class UniqueSelfFactory<Type> {
	protected:
		UniqueSelfFactory() = default;
		~UniqueSelfFactory() = default;

	public:
		UniqueSelfFactory(const UniqueSelfFactory&) = delete;
		UniqueSelfFactory& operator=(const UniqueSelfFactory&) = delete;
		UniqueSelfFactory(UniqueSelfFactory&&) = delete;
		UniqueSelfFactory& operator=(UniqueSelfFactory&&) = delete;

		template <typename... CtorArgs>
		static std::unique_ptr<Type> Create(CtorArgs&&... ctorArgs) {
			if (Type::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				class TypeProxy : public Type {
				public:
					explicit TypeProxy(CtorArgs&&... proxyCtorArgs)
						: Type(std::forward<CtorArgs>(proxyCtorArgs)...) {
					}
				};

				return std::make_unique<TypeProxy>(std::forward<CtorArgs>(ctorArgs)...);
			}
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Factory::SelfFactory, Logger::Tags::Core::Factory::Unique }, "UniqueSelfFactory: PreCreateHook failed for unique factory with type: {}, nullptr returned", CORI_CLEAN_TYPE_NAME(Type));
			return nullptr;
		}
	};

	template <typename Type, typename ErrorType>
	class UniqueSelfFactory<Type, ErrorType> {
	protected:
		UniqueSelfFactory() = default;
		~UniqueSelfFactory() = default;

	public:
		UniqueSelfFactory(const UniqueSelfFactory&) = delete;
		UniqueSelfFactory& operator=(const UniqueSelfFactory&) = delete;
		UniqueSelfFactory(UniqueSelfFactory&&) = delete;
		UniqueSelfFactory& operator=(UniqueSelfFactory&&) = delete;

		template <typename... CtorArgs>
		static std::expected<std::unique_ptr<Type>, ErrorType> Create(CtorArgs&&... ctorArgs) {
			return std::move(Type::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)).transform([...ctorArgs = std::forward<CtorArgs>(ctorArgs)]() mutable {
				class TypeProxy : public Type {
				public:
					explicit TypeProxy(CtorArgs&&... proxyCtorArgs)
						: Type(std::forward<CtorArgs>(proxyCtorArgs)...) {}
				};

				return std::make_unique<TypeProxy>(std::forward<CtorArgs>(ctorArgs)...);
			});
		}
	};
}