#pragma once
#include "Core/AssetManager/AssetManager2.hpp"
#include "Core/AssetManager/AssetDependency.hpp"

namespace Cori {
	namespace Core {
		namespace Internal {
			template<typename M>
			struct AssetRefAssetType {
				using Type = void;
			};

			template<IsValidAsset A>
			struct AssetRefAssetType<AssetRef<A>> {
				using Type = A;
			};

			template<typename M>
			concept IsAssetRefMember = !std::is_void_v<typename AssetRefAssetType<M>::Type>;

			template<typename M>
			concept IsDependencyCarrier = std::is_class_v<M> && std::is_aggregate_v<M> && !IsAssetRefMember<M>;
		}

		template<typename T>
		consteval uint32_t CountAssetDependencies() {
			static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

			uint32_t count = 0;

			template for (constexpr auto member : members) {
				using MemberType = [:std::meta::type_of(member):];
				using M = std::remove_cvref_t<MemberType>;

				if constexpr (Internal::IsAssetRefMember<M>) {
					count++;
				}
				else if constexpr (Internal::IsDependencyCarrier<M>) {
					count += CountAssetDependencies<M>();
				}
			}

			return count;
		}

		template<typename T>
		void CollectAssetDependencies(const T& value, AssetDependencySet& out) {
			static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));

			template for (constexpr auto member : members) {
				using MemberType = [:std::meta::type_of(member):];
				using M = std::remove_cvref_t<MemberType>;

				if constexpr (Internal::IsAssetRefMember<M>) {
					using A = typename Internal::AssetRefAssetType<M>::Type;

					const ConstHandle<A> handle = value.[:member:].GetHandle();
					if (handle.IsSet() && out.count < s_MaxAssetDependencies) {
						out.deps[out.count] = AssetDependency{
							.typeHash = AssetTraits<A>::TypeHash,
							.index = handle.GetIndex(),
							.version = handle.GetVersion()
						};

						out.count++;
					}
				}
				else if constexpr (Internal::IsDependencyCarrier<M>) {
					CollectAssetDependencies(value.[:member:], out);
				}
			}
		}

		template<typename T>
		[[nodiscard]] AssetDependencySet MakeAssetDependencySet(const T& value) {
			static_assert(CountAssetDependencies<T>() <= s_MaxAssetDependencies, "Asset type declares more AssetRef members than s_MaxAssetDependencies, raise the limit.");

			AssetDependencySet set{};
			CollectAssetDependencies(value, set);
			return set;
		}
	}
}
