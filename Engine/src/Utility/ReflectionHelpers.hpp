#pragma once

namespace Cori {
	namespace Utility {
		/**
		 * @brief Pulls the value of an annotation of type A off a member, or a fallback when it carries none.
		 */
		template <typename A>
		consteval A AnnotationOr(const std::meta::info member, A fallback) {
			for (const std::meta::info annotation : std::meta::annotations_of(member)) {
				if (std::meta::remove_cv(std::meta::type_of(annotation)) == ^^A) {
					return std::meta::extract<A>(annotation);
				}
			}
			return fallback;
		}

		/**
		 * @brief True when a member carries an annotation of type A, used for the tag annotations that hold no data.
		 */
		template <typename A>
		consteval bool HasAnnotation(const std::meta::info member) {
			for (const std::meta::info annotation : std::meta::annotations_of(member)) {
				if (std::meta::remove_cv(std::meta::type_of(annotation)) == ^^A) {
					return true;
				}
			}
			return false;
		}
	}
}