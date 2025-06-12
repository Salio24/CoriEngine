#pragma once

namespace Cori {

	template <typename DerivedType>
	class SharedSeflFactory {
	protected:
		SharedSeflFactory() = default;
		~SharedSeflFactory() = default;

		SharedSeflFactory(const SharedSeflFactory&) = delete;
		SharedSeflFactory& operator=(const SharedSeflFactory&) = delete;
		SharedSeflFactory(SharedSeflFactory&&) = delete;
		SharedSeflFactory& operator=(SharedSeflFactory&&) = delete;

	public:
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
			else {
				CORI_CORE_ERROR("SharedSeflFactory: PreCreateHook failed for shared factory with type: {0}, nullptr returned", typeid(DerivedType).name());
				return nullptr;
			}
		}
	};

	template <typename DerivedType>
	class UniqueSeflFactory {
	protected:
		UniqueSeflFactory() = default;
		~UniqueSeflFactory() = default;

		UniqueSeflFactory(const UniqueSeflFactory&) = delete;
		UniqueSeflFactory& operator=(const UniqueSeflFactory&) = delete;
		UniqueSeflFactory(UniqueSeflFactory&&) = delete;
		UniqueSeflFactory& operator=(UniqueSeflFactory&&) = delete;

	public:
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
			else {
				CORI_CORE_ERROR("UniqueSeflFactory: PreCreateHook failed for unique factory with type: {0}, nullptr returned", typeid(DerivedType).name());
				return nullptr;
			}
		}
	};

	// TODO: rewrite this
	/*
	usage example

	hpp:

	class ExampleClass : public SharedFactoryTrackable<ExampleClass> {
		CORI_DECLARE_SHARED_FACTORY_TRACKABLE(
			ExampleClass,
			(argType1 arg1, argType2 arg2)
		);
	}
	
	cpp:
	CORI_DEFINE_SHARED_FACTORY_TRACKABLE(
		ExampleClass,
			{

				this is the PreCreateHook body, code here is executed just before the constructor is called 
				and thus before the object is instantiated

				returning false here will log an error, and return a nullptr instead of a shared_ptr of an instance
				and thus the constructor is not called at all, the instance creation is aborted
				you can use this to check if the parameters are valid, or if the object can be created at all

				even tho PreCreateHook returns true at the very end by default, 
				you can return true manually to skip the remaining code

				if PreCreateHook is not needed, just leave this scope empty

			},
			(argType1 arg1, argType2 arg2),
				: m_Foo(arg1), m_Test(arg2) {

				this is the compiler body

			}
		);

		be aware that arguments (this ones - (argType1 arg1, argType2 arg2) in this example) should be the same across both macros

		make sure that the definition macros and the class declaration is in the same namespace

	example usage of the created class:

	std::shared_ptr<ExampleClass> m_ExampleInstance = ExampleClass::Create(arg1, arg2);

	you can not use the class constructor directly, as it is protected, and you shouldn't

	use CORI_DECLARE_UNIQUE_FACTORY_TRACKABLE instead of CORI_DECLARE_SHARED_FACTORY_TRACKABLE
	and CORI_DEFINE_UNIQUE_FACTORY_TRACKABLE instead of CORI_DEFINE_SHARED_FACTORY_TRACKABLE
	if you want to use unique ptr instead of shared ptr for your class

	*/
}