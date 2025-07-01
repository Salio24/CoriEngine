#include "FileManager.hpp"

namespace Cori {
	std::string FileManager::ReadTextFile(std::string_view filepath_sv) {
		std::filesystem::path fs_path(filepath_sv);

		std::ifstream file(fs_path, std::ios::in | std::ios::binary);
		if (!file) {
			CORI_CORE_ERROR("FileManager: Could not open file: '{0}'", filepath_sv);
			return "";
		}

		// Try to get file size for pre-allocation (optimization)
		file.seekg(0, std::ios::end);
		std::streampos file_size_pos = file.tellg();
		file.seekg(0, std::ios::beg); // Seek back to the beginning

		// Check for errors after seeking and telling
		if (file_size_pos == static_cast<std::streampos>(-1) || !file.good()) {
			CORI_CORE_ERROR("FileManager: Error determining file size or seeking in file: '{0}'", filepath_sv);
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
			CORI_CORE_ERROR("FileManager: Error while reading file content: '{0}'", filepath_sv);
			return "";
		}

		return content;
	}

	bool FileManager::WriteTextFile(std::string_view filepath, std::string_view content) {
		std::filesystem::path fs_path(filepath);
		std::ofstream file(fs_path, std::ios::out | std::ios::binary | std::ios::trunc);

		if (!file) {
			CORI_CORE_ERROR("FileManager: Could not open/create file for writing: '{0}'", filepath);
			return false;
		}

		file.write(content.data(), static_cast<std::streamsize>(content.size()));

		if (!file) {
			CORI_CORE_ERROR("FileManager: Error while writing to file: '{0}'", filepath);
			file.close();
			return false;
		}

		file.close();
		if (!file) {
			CORI_CORE_ERROR("FileManager: Error while closing file after writing: '{0}'", filepath);
			return false;
		}

		CORI_CORE_DEBUG("FileManager: Successfully read '{0}' as string", filepath);
		return true;
	}

	std::string FileManager::GetFilename(std::string_view filepath) {
		std::filesystem::path path(filepath);
		return path.filename().string();
	}
}