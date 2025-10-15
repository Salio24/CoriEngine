#pragma once
#include "Entity.hpp"

namespace Cori {
	namespace World {
		/**
		 * @brief Helper to exclude certain components from a static view.
		 * @tparam T Component types to exclude.
		 */
		template <typename... T>
		struct Exclude {};

		/**
		 * @brief A wrapper for an EnTT compile-time view that provides an iterator to access Entity instances directly.
		 * @note Don't create directly, use Scene::StaticView.
		 */
		template <typename View>
		class StaticEntityView {
		public:
			using UnderlyingIterator = typename View::iterator;

			class Iterator {
			public:
				using iterator_category = std::forward_iterator_tag;
				using value_type = Entity;
				using difference_type = std::ptrdiff_t;
				using pointer = const Entity*;
				using reference = Entity;

				Iterator(entt::registry* registry, UnderlyingIterator it)
					: m_Registry(registry), m_EnttIterator(it) {}

				reference operator*() const {
					return Entity({*m_Registry, *m_EnttIterator});
				}

				Iterator& operator++() {
					++m_EnttIterator;
					return *this;
				}

				bool operator!=(const Iterator& other) const {
					return m_EnttIterator != other.m_EnttIterator;
				}

			private:
				entt::registry* m_Registry;
				UnderlyingIterator m_EnttIterator;
			};

			StaticEntityView(View view, entt::registry& registry)
				: m_View(view), m_Registry(&registry) {}

			/**
			 * @brief Retries the components from an entity, both the components and the entity should be in a view, faster than GetComponents.
			 * @tparam T Components to retrieve.
			 * @param entity Entity from which to retrieve the component.
			 * @return References to the requested component(s).
			 */
			template<typename... T>
			[[nodiscard]] decltype(auto) Get(Entity entity) {
				return m_View.template get<T...>(entity.GetRawEntity());
			}

			/**
			 * @brief Checks how many entities in a view have a specific component.
			 * @tparam T Component type.
			 * @return Amount of entities with component T in a view.
			 * @note Not available if view has components with ```in_place_delete``` policy.
			 */
			template<typename T>
			[[nodiscard]] size_t Size() {
				return m_View.template size<T>();
			}

			/**
			 * @brief Checks the estimate amount of entities in the view.
			 * @return Estimate size hint.
			 * @note Only available if view has components with ```in_place_delete``` policy.
			 */
			[[nodiscard]] size_t SizeHint() const {
				return m_View.size_hint();
			}

			/**
			 * @brief Checks if view contains a specific entity.
			 * @param entity Entity to check the presence of.
			 * @return True if persent, false otherwise.
			 */
			[[nodiscard]] bool Contains(const Entity entity) const {
				return m_View.contains(entity.GetRawEntity());
			}

			auto begin() { return Iterator(m_Registry, m_View.begin()); }
			auto end() { return Iterator(m_Registry, m_View.end()); }

		private:
			View m_View;
			entt::registry* m_Registry;
		};

		/**
		 * @brief A wrapper for an EnTT runtime view that provides an iterator to access Entity instances directly.
		 * @details This view is configured at runtime, offering flexibility at a slight performance cost compared to a compile-time/static view.
		 * Use this when the exact set of components to iterate over is not known at compile-time.
		 */
		class DynamicEntityView {
		public:

			/**
			 * @brief Constructs a runtime view wrapper.
			 * @param registry A reference to the EnTT registry.
			 */
			explicit DynamicEntityView(entt::registry& registry)
				: m_Registry(&registry) {}

			/**
			 * @brief Adds one or more component types to the view's filter.
			 * @tparam T The component types to include.
			 */
			template <typename... T>
			DynamicEntityView& With() & {
				(m_View.iterate(m_Registry->storage<T>()), ...);
				return *this;
			}

			/**
			 * @brief Adds one or more component types to the view's filter.
			 * @tparam T The component types to include.
			 */
			template <typename... T>
			DynamicEntityView&& With() && {
				(m_View.iterate(m_Registry->storage<T>()), ...);
				return std::move(*this);
			}

			/**
			 * @brief Excludes one or more component types from the view's filter.
			 * @tparam T The component types to exclude.
			 */
			template <typename... T>
			DynamicEntityView& Without() & {
				(m_View.exclude(m_Registry->storage<T>()), ...);
				return *this;
			}

			/**
			 * @brief Excludes one or more component types from the view's filter.
			 * @tparam T The component types to exclude.
			 */
			template <typename... T>
			DynamicEntityView&& Without() && {
				(m_View.exclude(m_Registry->storage<T>()), ...);
				return std::move(*this);
			}

			/**
			 * @brief Checks the estimate amount of entities in the view.
			 * @return Estimate size hint.
			 */
			[[nodiscard]] size_t SizeHint() const {
				return m_View.size_hint();
			}

			/**
			 * @brief Checks if view contains a specific entity.
			 * @param entity Entity to check the presence of.
			 * @return True if persent, false otherwise.
			 */
			[[nodiscard]] bool Contains(const Entity entity) const {
				return m_View.contains(entity.GetRawEntity());
			}

			/**
			 * @brief Completely clear the view.
			 */
			void Clear() {
				m_View.clear();
			}

			using UnderlyingIterator = entt::runtime_view::iterator;
			class Iterator {
			public:
				using iterator_category = std::forward_iterator_tag;
				using value_type = Entity;
				using difference_type = std::ptrdiff_t;
				using pointer = const Entity*;
				using reference = Entity;

				Iterator(entt::registry* registry, UnderlyingIterator it)
					: m_Registry(registry), m_EnttIterator(it) {}

				reference operator*() const {
					return Entity({*m_Registry, *m_EnttIterator});
				}

				Iterator& operator++() {
					++m_EnttIterator;
					return *this;
				}

				bool operator!=(const Iterator& other) const {
					return m_EnttIterator != other.m_EnttIterator;
				}

			private:
				entt::registry* m_Registry;
				UnderlyingIterator m_EnttIterator;
			};

			auto begin() { return Iterator(m_Registry, m_View.begin()); }
			auto end() { return Iterator(m_Registry, m_View.end()); }

		private:
			entt::registry* m_Registry;
			entt::runtime_view m_View;
		};
	}
}
