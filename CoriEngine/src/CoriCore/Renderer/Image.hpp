#pragma once

namespace Cori {
	class Image {
	public:
		Image(const std::string& path);
		~Image();

		void FlipVertically();
		void FlipHorizontally();
		void Mirror();

		void* GetPixelData() const;
		uint32_t GetWidth() const;
		uint32_t GetHeight() const;

		Image& operator=(Image&& other) = delete;
		Image(Image&& other) = delete;

		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;
	private:
		void* surface;
	};
}