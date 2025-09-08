#include "Uuid.hpp"

namespace {
	uuids::uuid Generate() {

		static std::mt19937 engine = [] {
			std::random_device rd;
			auto seed_data = std::array<int32_t, std::mt19937::state_size>{};
			std::ranges::generate(seed_data, std::ref(rd));
			std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
			return std::mt19937{seq};
		}();

		static uuids::uuid_random_generator gen{engine};

		return gen();
	}
}

namespace Cori {
	namespace Core {
		UUID::UUID() : m_ID(Generate()) {}

		uuids::uuid UUID::LoadFromString(const std::string& uuidStr) {
			if (uuids::uuid::is_valid_uuid(uuidStr)) {
				return uuids::uuid::from_string(uuidStr).value();
			}
			const uuids::uuid randomUuid = Generate();
			CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::UUID }, "LoadFromString has failed. Provided uuidStr: '{}', is not a valid string representation of standart UUID. Generated a random UUID instead with value: '{}'", uuidStr, uuids::to_string(randomUuid));
			CORI_CORE_WARN_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::UUID },"Correct UUID format is: 'xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx' or '{{xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx}}'");
			return randomUuid;
		}
	}
}