#include "FileManager.hpp"

namespace Cori {
	namespace FileSystem {
		std::string FileManager::ReadTextFile(const std::filesystem::path& filepath) {
			std::ifstream file(filepath, std::ios::in | std::ios::binary);
			if (!file) {
				CORI_CORE_ERROR("FileManager: Could not open file: '{0}'", filepath.string());
				return "";
			}

			// Try to get file size for pre-allocation (optimization)
			file.seekg(0, std::ios::end);
			std::streampos file_size_pos = file.tellg();
			file.seekg(0, std::ios::beg); // Seek back to the beginning

			// Check for errors after seeking and telling
			if (file_size_pos == static_cast<std::streampos>(-1) || !file.good()) {
				CORI_CORE_ERROR("FileManager: Error determining file size or seeking in file: '{0}'", filepath.string());
				return "";
			}

			auto file_size = static_cast<std::size_t>(file_size_pos);

			std::string content;
			if (file_size > 0) {
				content.reserve(file_size);
				content.assign(std::istreambuf_iterator<char>(file),
					std::istreambuf_iterator<char>());
			}
			else {
				content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
			}

			if (file.bad() || (file.fail() && !file.eof())) {
				CORI_CORE_ERROR("FileManager: Error while reading file content: '{0}'", filepath.string());
				return "";
			}

			return content;
		}
	}
}