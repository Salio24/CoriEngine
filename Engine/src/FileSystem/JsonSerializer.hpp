#pragma once
#include <nlohmann/json.hpp>

namespace Cori {
	namespace FileSystem {
		/**
		 * @brief To satisfy use NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE macro. Refer here https://json.nlohmann.me/api/macros/nlohmann_define_type_non_intrusive/ for details.
		 */
		template<typename T>
		concept JsonSerializable = requires(T value, const nlohmann::json j) {
			{ nlohmann::json(value) } -> std::same_as<nlohmann::json>;
			{ j.get<T>() } -> std::same_as<T>;
		};

		/**
		 * @brief Class is responsible for simple loading/saving config and save files as json files.
		 */
		class JsonSerializer {
		public:
			/**
			 * @brief Saves a serializable object to a JSON file.
			 * @tparam T A type that satisfies the JsonSerializable concept.
			 * @param data The object to save.
			 * @param filepath The destination file path.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			template<JsonSerializable T>
			static std::expected<void, Core::CoriError<>> Save(const T& data, const std::filesystem::path& filepath) {
				try {

					const auto parent_dir = filepath.parent_path();

					if (!parent_dir.empty()) {
						std::filesystem::create_directories(parent_dir);
					}

					std::ofstream file(filepath);
					if (!file.is_open()) {
						return std::unexpected(Core::CoriError(std::format("Could not open file for writing: ", filepath.string())));
					}


					nlohmann::json j = data;
					file << std::fixed << std::setprecision(4);
					file << j.dump(4);

					return {};
				}
				catch (const std::exception& e) {
					return std::unexpected(Core::CoriError(std::format("An unexpected error occurred during save: ", e.what())));
				}
			}

			/**
			 * @brief Loads an object from a JSON file.
			 * @tparam T A type that satisfies the JsonSerializable concept.
			 * @param filepath The source file path.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			template<JsonSerializable T>
			static std::expected<T, Core::CoriError<>> Load(const std::filesystem::path& filepath) {
				if (!std::filesystem::exists(filepath)) {
					return std::unexpected(Core::CoriError(std::format("File does not exist: ", filepath.string())));
				}

				try {
					std::ifstream file(filepath);
					if (!file.is_open()) {
						return std::unexpected(Core::CoriError(std::format("Could not open file for reading: ", filepath.string())));
					}

					nlohmann::json j;
					file >> j;
					return j.get<T>();
				}
				catch (const nlohmann::json::exception& e) {
					return std::unexpected(Core::CoriError(std::format("JSON processing error: ", e.what())));
				}
				catch (const std::exception& e) {
					return std::unexpected(Core::CoriError(std::format("An unexpected error occurred during load: ", e.what())));
				}
			}
		};
	}
}