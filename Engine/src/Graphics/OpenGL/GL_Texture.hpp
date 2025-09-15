#pragma once
#include "Graphics/Texture.hpp"
#include "Graphics/GraphicsAPIs.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/AutoRegisteringFactory.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class OpenGLTexture2D final : public Texture2D, public Profiling::Trackable<OpenGLTexture2D, Texture2D>, public Core::RegisterInFactory<Texture2D, OpenGLTexture2D, GraphicsAPIs, GraphicsAPIs::OpenGL, const void*, const uint32_t, const uint32_t, const Texture::Params&> {
			public:
				static bool PreCreateHook(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params);
				explicit OpenGLTexture2D(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params);
				~OpenGLTexture2D() override;

				void Bind(uint32_t slot) const override;

				[[nodiscard]] uint32_t GetWidth() const override { return m_Width; }
				[[nodiscard]] uint32_t GetHeight() const override { return m_Height; }

				[[nodiscard]] bool HasSemiTransparency() const override;

			private:
				uint32_t m_ID{ 0 };
				uint32_t m_Width{ 0 };
				uint32_t m_Height{ 0 };
				bool m_HasSemiTransparency{ false };

				CORI_REGISTERED_FACTORY_INIT;
			};
		}
	}
}