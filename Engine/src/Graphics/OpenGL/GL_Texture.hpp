#pragma once
#include "Graphics/Texture.hpp"
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class OpenGLTexture2D final : public Texture2D, public Profiling::Trackable<OpenGLTexture2D, Texture2D, Texture> {
			public:
				OpenGLTexture2D();
				~OpenGLTexture2D() override;

				void Upload(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params) override;

				void Bind(uint32_t slot) const override;

				[[nodiscard]] uint32_t GetWidth() const override { return m_Width; }
				[[nodiscard]] uint32_t GetHeight() const override { return m_Height; }

				[[nodiscard]] bool HasSemiTransparency() const override;

			private:
				uint32_t m_ID{ 0 };
				uint32_t m_Width{ 0 };
				uint32_t m_Height{ 0 };
				bool m_HasSemiTransparency{ false };
			};
		}
	}
}