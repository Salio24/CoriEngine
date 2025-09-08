#pragma once

namespace Cori {
	namespace FileSystem {
		class FileManager {
		public:
			static std::string ReadTextFile(const std::filesystem::path& filepath);

			static std::string ReadTextFile(const std::string& filepath) {
				return ReadTextFile(std::filesystem::path(filepath));
			}
		};
	}
}