#pragma once
#include <uuid.h>

namespace Cori {
	namespace Core {
		class UUID {
		public:
			UUID();
			explicit UUID(const std::string& uuidStr) : m_ID(LoadFromString(uuidStr)) {}

			explicit operator uuids::uuid() const noexcept { return m_ID; }

			[[nodiscard]] std::string GetSerializationString() const {
				return uuids::to_string(m_ID);
			}

			bool operator==(const UUID& other) const {
				return m_ID ==  other.m_ID;
			}

		private:
			const uuids::uuid m_ID;

			static uuids::uuid LoadFromString(const std::string& uuidStr);
		};
	}
}

namespace std {
	template<>
	struct hash<Cori::Core::UUID> {
		std::size_t operator()(const Cori::Core::UUID& uuid) const noexcept { return hash<uuids::uuid>{}(static_cast<uuids::uuid>(uuid)); }
	};
}