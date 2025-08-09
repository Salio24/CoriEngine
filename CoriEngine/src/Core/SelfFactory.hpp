#pragma once

namespace Cori {

	//TODO: adopt std::expected

	template <typename DerivedType>
	class SharedSelfFactory {
	protected:
		SharedSelfFactory() = default;
		~SharedSelfFactory() = default;

	public:
		SharedSelfFactory(const SharedSelfFactory&) = delete;
		SharedSelfFactory& operator=(const SharedSelfFactory&) = delete;
		SharedSelfFactory(SharedSelfFactory&&) = delete;
		SharedSelfFactory& operator=(SharedSelfFactory&&) = delete;

		template <typename... CtorArgs>
		static std::shared_ptr<DerivedType> Create(CtorArgs&&... ctorArgs) {
			if (DerivedType::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				class DerivedTypeProxy : public DerivedType {
				public:
					explicit DerivedTypeProxy(CtorArgs&&... proxyCtorArgs)
						: DerivedType(std::forward<CtorArgs>(proxyCtorArgs)...) {
					}
				};

				return std::make_shared<DerivedTypeProxy>(std::forward<CtorArgs>(ctorArgs)...);
			}
			CORI_CORE_ERROR("SharedSelfFactory: PreCreateHook failed for shared factory with type: {0}, nullptr returned", typeid(DerivedType).name());
			return nullptr;
		}
	};

	template <typename DerivedType>
	class UniqueSelfFactory {
	protected:
		UniqueSelfFactory() = default;
		~UniqueSelfFactory() = default;

	public:
		UniqueSelfFactory(const UniqueSelfFactory&) = delete;
		UniqueSelfFactory& operator=(const UniqueSelfFactory&) = delete;
		UniqueSelfFactory(UniqueSelfFactory&&) = delete;
		UniqueSelfFactory& operator=(UniqueSelfFactory&&) = delete;

		template <typename... CtorArgs>
		static std::unique_ptr<DerivedType> Create(CtorArgs&&... ctorArgs) {
			if (DerivedType::PreCreateHook(std::forward<CtorArgs>(ctorArgs)...)) {
				class DerivedTypeProxy : public DerivedType {
				public:
					explicit DerivedTypeProxy(CtorArgs&&... proxyCtorArgs)
						: DerivedType(std::forward<CtorArgs>(proxyCtorArgs)...) {
					}
				};

				return std::make_unique<DerivedTypeProxy>(std::forward<CtorArgs>(ctorArgs)...);
			}
			CORI_CORE_ERROR("UniqueSelfFactory: PreCreateHook failed for unique factory with type: {0}, nullptr returned", typeid(DerivedType).name());
			return nullptr;
		}
	};
}