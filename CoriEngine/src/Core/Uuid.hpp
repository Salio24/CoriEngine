#pragma once
#include <uuid.h>

namespace Cori {
	namespace Core {
		class UUID {
		public:
			UUID() : m_ID(GenerateRandom()) {}
			explicit UUID(const std::string& uuidStr) : m_ID(LoadFromString(uuidStr)) {}

			explicit operator uuids::uuid() const noexcept { return m_ID; }

			[[nodiscard]] std::string GetSerializationString() const {
				return uuids::to_string(m_ID);
			}

		private:
			const uuids::uuid m_ID;

			static uuids::uuid GenerateRandom() {
				static uuids::uuid_random_generator gen = []{
					std::random_device rd;
					auto seed_data = std::array<int, std::mt19937::state_size>{};
					std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
					std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
					std::mt19937 generator(seq);
					return uuids::uuid_random_generator{generator};
				}();
				return gen();
			}

			static uuids::uuid LoadFromString(const std::string& uuidStr) {
				if (uuids::uuid::is_valid_uuid(uuidStr)) {
					return uuids::uuid::from_string(uuidStr).value();
				}
				uuids::uuid random_uuid = GenerateRandom();
				CORI_CORE_WARN_TAGGED({"Core", "UUID"}, "LoadFromString has failed. Provided uuidStr: '{}', is not a valid string representation of standart UUID. Generated a random UUID instead with value: '{}'", uuidStr, uuids::to_string(random_uuid));
				CORI_CORE_WARN_TAGGED({"Core", "UUID"},"Correct UUID format is: 'xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx' or '{{xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx}}'");
				return random_uuid;
			}
		};
	}
}

namespace std {
	template<>
	struct hash<Cori::Core::UUID> {
		std::size_t operator()(const Cori::Core::UUID& uuid) const noexcept { return hash<uuids::uuid>{}(static_cast<uuids::uuid>(uuid)); }
	};
}