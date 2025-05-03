// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "FileManager.hpp"

namespace Cori {
	std::string FileManager::ReadTextFile(const std::string& filepath) {
		std::ifstream file(filepath, std::ios::in | std::ios::binary);
		if (!file) {
			CORI_CORE_ERROR("Could not open file: {0}", filepath);
			return "";
		}

		file.seekg(0, std::ios::end);
		auto size = file.tellg();
		if (size < 0) {
			CORI_CORE_ERROR("Error while reading file: {0}", filepath);
			return "";
		}

		std::string content;
		content.reserve(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		content.assign(std::istreambuf_iterator<char>(file), {});
		return content;
	}

	bool FileManager::WriteTextFile(const std::string& filepath, const std::string& content) {
		std::ofstream file(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file) {
			CORI_CORE_ERROR("Could not open/create file for writing: {0}", filepath);
			return false;
		}
		file.write(content.data(), std::streamsize(content.size()));
		return static_cast<bool>(file);
	}

	std::string FileManager::GetFilename(const std::string& filepath) {
		std::filesystem::path path(filepath);
		std::filesystem::path filename = path.filename();
		return filename.string();
	}
}