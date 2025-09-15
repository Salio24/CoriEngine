#pragma once

namespace Cori {
	namespace FileSystem {
		/**
		 * @brief Simple static class used to read files as string. Will likely expand its functionality later.
		 */
		class FileManager {
		public:

			/**
			 * @brief Reads any file as a string.
			 * @param filepath File to read.
			 * @return String containing data from a file.
			 */
			static std::string ReadTextFile(const std::filesystem::path& filepath);

			/**
			 * @brief Reads any file as a string.
			 * @param filepath File to read.
			 * @return String containing data from a file.
			 */
			static std::string ReadTextFile(const std::string& filepath) {
				return ReadTextFile(std::filesystem::path(filepath));
			}
		};
	}
}