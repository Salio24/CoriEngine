#pragma once

namespace Cori {
	class FileManager {
	public:
		static std::string ReadTextFile(const std::string& filepath);

		static bool  WriteTextFile(const std::string& filepath, const std::string& content);

		static std::string GetFilename(const std::string& filepath);
	};
}