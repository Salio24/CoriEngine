#pragma once

namespace Cori {

	class FileManager {
	public:
		static std::string ReadTextFile(std::string_view filepath);

		static std::string ReadTextFile(const std::string& filepath) {
			return ReadTextFile(std::string_view(filepath));
		}

		static bool WriteTextFile(std::string_view filepath_sv, std::string_view content_sv);

		// Overload for std::string filepath
		static bool WriteTextFile(const std::string& filepath, std::string_view content_sv) {
			return WriteTextFile(std::string_view(filepath), content_sv);
		}

		// Overload for std::string content
		static bool WriteTextFile(std::string_view filepath_sv, const std::string& content) {
			return WriteTextFile(filepath_sv, std::string_view(content));
		}

		// Overload for std::string both
		static bool WriteTextFile(const std::string& filepath, const std::string& content) {
			return WriteTextFile(std::string_view(filepath), std::string_view(content));
		}

		static std::string GetFilename(std::string_view filepath);

		// Overload for std::string
		static std::string GetFilename(const std::string& filepath) {
			return GetFilename(std::string_view(filepath));
		}

	};


	

}