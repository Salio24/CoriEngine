#pragma once
#include <array>
#include <span>
#include "Utility/StringHash.hpp"

namespace Cori {
	namespace Core {
		inline constexpr uint32_t s_MaxAssetDependencies{ 8 };

		struct AssetDependency {
			Utility::StringHash64 typeHash{ 0 };
			uint32_t index{ UINT32_MAX };
			uint32_t version{ 0 };

			[[nodiscard]] bool operator==(const AssetDependency& other) const = default;
		};

		struct AssetDependencySet {
			std::array<AssetDependency, s_MaxAssetDependencies> deps{};
			uint32_t count{ 0 };

			[[nodiscard]] std::span<const AssetDependency> View() const {
				return std::span<const AssetDependency>(deps.data(), count);
			}

			[[nodiscard]] bool operator==(const AssetDependencySet& other) const {
				if (count != other.count) {
					return false;
				}

				for (uint32_t i = 0; i < count; i++) {
					if (!(deps[i] == other.deps[i])) {
						return false;
					}
				}

				return true;
			}
		};

		static_assert(std::is_trivially_copyable_v<AssetDependencySet>, "AssetDependencySet is published through a seqlock and must be trivially copyable.");
	}
}
