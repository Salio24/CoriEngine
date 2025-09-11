#pragma once
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		class Image : public Profiling::Trackable<Image> {
		public:
			void FlipVertically();
			void FlipHorizontally();
			void Mirror();

			[[nodiscard]] void* GetPixelData() const;
			[[nodiscard]] uint32_t GetWidth() const;
			[[nodiscard]] uint32_t GetHeight() const;
			[[nodiscard]] bool HasSemiTransparency() const;
			[[nodiscard]] bool GetSuccessStatus() const { return m_SuccessStatus; }
			[[nodiscard]] bool IsPadded() const { return m_IsPadded; }

			std::expected<void, Core::CoriError<>> AddPadding(const glm::u16vec2 spriteResolution);

			[[nodiscard]] static std::shared_ptr<Image> Create(const std::filesystem::path& path);

			~Image();

		private:
			explicit Image(const std::filesystem::path& path);

			bool m_HasSemiTransparency = false;

			bool m_SuccessStatus{ false };
			bool m_IsPadded{ false };
			void* m_Surface;
		};
	}
}