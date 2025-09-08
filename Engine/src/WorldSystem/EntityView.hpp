#pragma once
#include "Entity.hpp"

namespace Cori {
	namespace World {
		template <typename... T>
		struct Exclude {};

		template <typename View>
		class EntityView {
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

				bool operator!=(const Iterator& other) const { return m_EnttIterator != other.m_EnttIterator; }

			private:
				entt::registry* m_Registry;
				UnderlyingIterator m_EnttIterator;
			};

			EntityView(View view, entt::registry& registry)
				: m_View(view), m_Registry(&registry) {}

			template <typename T>
			decltype(auto) Get(Entity entity) {
				return m_View.template get<T>(entity.GetRawEntity());
			}

			auto begin() { return Iterator(m_Registry, m_View.begin()); }
			auto end() { return Iterator(m_Registry, m_View.end()); }

		private:
			View m_View;
			entt::registry* m_Registry;
		};
	}
}
