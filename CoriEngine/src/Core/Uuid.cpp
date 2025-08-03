#include "Uuid.hpp"

namespace {
	uuids::uuid Generate() {

		static std::mt19937 engine = [] {
			std::random_device rd;
			auto seed_data = std::array<int, std::mt19937::state_size>{};
			std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
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
			uuids::uuid random_uuid = Generate();
			CORI_CORE_WARN_TAGGED({"Core", "UUID"}, "LoadFromString has failed. Provided uuidStr: '{}', is not a valid string representation of standart UUID. Generated a random UUID instead with value: '{}'", uuidStr, uuids::to_string(random_uuid));
			CORI_CORE_WARN_TAGGED({"Core", "UUID"},"Correct UUID format is: 'xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx' or '{{xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx}}'");
			return random_uuid;
		}
	}
}