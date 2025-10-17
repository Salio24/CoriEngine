#ifndef UUID_H
#define UUID_H
#include <uuid.h>

namespace Cori {
	namespace Core {
		/**
		 * @brief A 128bit UUID, can be serialized to the string and deserialized from it.
		 */
		class UUID {
		public:
			/**
			 * @brief Generates a random 128bit UUID.
			 */
			UUID();

			/**
			 * @brief Loads UUID from a serialized string, the one created by GetSerializationString() method.
			 * @param uuidStr Serialized string.
			 */
			explicit UUID(const std::string& uuidStr) : m_ID(LoadFromString(uuidStr)) {}

			explicit operator uuids::uuid() const noexcept { return m_ID; }

			/**
			 * @brief Returns the UUID as a formated string liable for serialization.
			 * @return Formated string.
			 */
			[[nodiscard]] std::string GetSerializationString() const {
				return uuids::to_string(m_ID);
			}

			[[nodiscard]] std::pair<uint64_t, uint64_t> GetRaw() const {
				const auto span = m_ID.as_bytes();
				const uint64_t left = *reinterpret_cast<const uint64_t*>(&span[0]);
				const uint64_t right = *reinterpret_cast<const uint64_t*>(&span[8]);
				return std::make_pair(left, right);
			}

			/**
			 * @brief Do really i need to explain this?
			 */
			bool operator==(const UUID& other) const {
				return m_ID ==  other.m_ID;
			}

		private:
			const uuids::uuid m_ID;

			static uuids::uuid LoadFromString(const std::string& uuidStr);
		};
	}
}

template<>
struct std::hash<Cori::Core::UUID> {
	std::size_t operator()(const Cori::Core::UUID& uuid) const noexcept { return hash<uuids::uuid>{}(static_cast<uuids::uuid>(uuid)); }
};

#endif