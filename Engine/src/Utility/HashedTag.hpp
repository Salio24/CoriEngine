#pragma once
#include "StringHash.hpp"

namespace Cori {
	namespace Utility {
		/**
		 * @brief HashedTag that uses a 64bit string hash (FNV-1a).
		 */
		struct HashedTag64 {
			/**
			 * @brief Gets the name the tag was declared with.
			 * @return Initial unhashed name.
			 */
			[[nodiscard]] const char* GetDebugName() const {
				return m_DebugName;
			}

			bool operator==(const HashedTag64& other) const {
				return m_Hash == other.m_Hash;
			}

			StringHash64 m_Hash{ 0 };
			const char* m_DebugName{};
		};
	}
}