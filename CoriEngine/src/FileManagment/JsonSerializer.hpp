#pragma once
#include <nlohmann/json.hpp>

namespace Cori {
	template<typename T>
	concept JsonSerializable = requires(T value, const nlohmann::json j) {
		{ nlohmann::json(value) } -> std::same_as<nlohmann::json>;
		{ j.template get<T>() } -> std::same_as<T>;
	};

	class JsonSerializer {
	public:
		/**
		 * @brief Saves a serializable object to a JSON file.
		 * @tparam T A type that satisfies the JsonSerializable concept.
		 * @param data The object to save.
		 * @param filepath The destination file path.
		 * @return std::expected<void, std::string> which is empty on success,
		 *         or contains an error message string on failure.
		 */
		template<JsonSerializable T>
		inline static std::expected<void, std::string> Save(const T& data, const std::filesystem::path& filepath) {
			try {

				const auto parent_dir = filepath.parent_path();

				if (!parent_dir.empty()) {
					std::filesystem::create_directories(parent_dir);
				}

				std::ofstream file(filepath);
				if (!file.is_open()) {
					return std::unexpected("Could not open file for writing: " + filepath.string());
				}


				nlohmann::json j = data;
				file << std::fixed << std::setprecision(4);
				file << j.dump(4);

				return {}; // Success
			}
			catch (const std::exception& e) {
				return std::unexpected("An unexpected error occurred during save: " + std::string(e.what()));
			}
		}

		/**
		 * @brief Loads an object from a JSON file.
		 * @tparam T A type that satisfies the JsonSerializable concept.
		 * @param filepath The source file path.
		 * @return std::expected<T, std::string> which contains the loaded object
		 *         on success, or an error message string on failure.
		 */
		template<JsonSerializable T>
		inline static std::expected<T, std::string> Load(const std::filesystem::path& filepath) {
			if (!std::filesystem::exists(filepath)) {
				return std::unexpected("File does not exist: " + filepath.string());
			}

			try {
				std::ifstream file(filepath);
				if (!file.is_open()) {
					return std::unexpected("Could not open file for reading: " + filepath.string());
				}

				nlohmann::json j;
				file >> j;
				return j.template get<T>();
			}
			catch (const nlohmann::json::exception& e) {
				return std::unexpected("JSON processing error: " + std::string(e.what()));
			}
			catch (const std::exception& e) {
				return std::unexpected("An unexpected error occurred during load: " + std::string(e.what()));
			}
		}
	};
}