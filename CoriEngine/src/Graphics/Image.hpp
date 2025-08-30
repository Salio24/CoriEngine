#pragma once
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"

namespace Cori {
	class Image : public Profiling::Trackable<Image>, public SharedSelfFactory<Image> {
	public:
		static bool PreCreateHook(const std::filesystem::path& path);

		void FlipVertically();
		void FlipHorizontally();
		void Mirror();

		[[nodiscard]] void* GetPixelData() const;
		[[nodiscard]] uint32_t GetWidth() const;
		[[nodiscard]] uint32_t GetHeight() const;
		[[nodiscard]] bool HasSemiTransparency() const;
		[[nodiscard]] bool GetSuccessStatus() const { return m_SuccessStatus; }
		[[nodiscard]] bool IsPadded() const { return m_IsPadded; }

		std::expected<void, CoriError<>> AddPadding(const glm::u16vec2 spriteResolution);

	protected:
		explicit Image(const std::filesystem::path& path);
		~Image();
	private:

		bool m_HasSemiTransparency = false;

		bool m_SuccessStatus{ false };
		bool m_IsPadded{ false };
		void* m_Surface;
	};
}