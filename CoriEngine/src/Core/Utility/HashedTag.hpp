#pragma once
#include "StringHash.hpp"

namespace Cori {
	namespace Utility {
		struct HashedTag64 {
			const char* GetDebugName() const {
				return m_DebugName;
			}

			bool operator==(const HashedTag64& other) const {
				return m_Hash == other.m_Hash;
			}


			StringHash64 m_Hash{ 0 };
			const char* m_DebugName;
		};
	}
}

#ifdef DEBUG_BUILD
	#ifdef CORI_CHECK_TAG_COLLISION
	namespace Cori {
		namespace Utils {
			namespace Internal {
				inline void CheckGlobalTag64Collision(Utility::StringHash64 tag, const char* name) {
					static std::unordered_map<Utility::StringHash64, const char*> globalTag64CollisionMap;
					if (globalTag64CollisionMap.contains(tag)) {
						std::cout << "Tag hash64 collision: '" << name << "'and: '" << globalTag64CollisionMap.at(tag) << "' , both hash to: 0x" << std::hex << tag << std::dec << "\n";
					} else {
						globalTag64CollisionMap.emplace(tag, name);
						std::cout << "Tag registered, name: '" << name << "' , hash: 0x" << std::hex << tag << std::dec << "\n";
					}
				}
			}
		}
	}

	#define CONCAT_IMPL(a, b) a##b
	#define CONCAT(a, b) CONCAT_IMPL(a, b)

	#define CORI_DECLARE_TAG(tag) inline constexpr Cori::Utils::HashedTag64 tag{#tag##_hs64, #tag} \
		static const bool CONCAT(RegisterTagForCheck, __LINE__) = [](){ \
			Cori::Utils::Internal::CheckGlobalTag64Collision(#tag##_hs64, #tag); \
			return true; \
		}();

	#else
	#define CORI_DECLARE_TAG(tag) inline constexpr Cori::Utility::HashedTag64 tag{#tag##_hs64, #tag};
	#endif
#else
	#define CORI_DECLARE_TAG(tag) inline constexpr Cori::Utility::HashedTag64 tag{#tag##_hs64, #tag};
#endif

