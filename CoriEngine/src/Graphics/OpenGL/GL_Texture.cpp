#include "GL_Texture.hpp"
#include "Graphics/Image.hpp"
#include <glad/gl.h>

namespace Cori {
	bool OpenGLTexture2D::PreCreateHook([[maybe_unused]] const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self , Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::Texture2D }, "Could not find image at the specified path: '{}'. A placeholder will be loaded instead", path.string());
		}

		return true;
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::filesystem::path& path) {
		CORI_PROFILE_FUNCTION();
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self , Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::Texture2D }, "(GL_RuntimeID; {}): Creating texture from '{}'", m_ID, path.string());
		const auto image = std::make_unique<Image>(path);
		m_Width = image->GetWidth();
		m_Height = image->GetHeight();
		m_HasSemiTransparency = image->HasSemiTransparency();

		glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);

		glTextureStorage2D(m_ID, 1, GL_RGBA8, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));

		glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		image->FlipVertically();

		glTextureSubImage2D(m_ID, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, image->GetPixelData());
		if (image->GetSuccessStatus()) {
			CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self , Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::Texture2D }, "(GL_RuntimeID; {}): Successfully created texture from '{}'", m_ID, path.string());
		}
		else {
			CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self , Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::Texture2D }, "(GL_RuntimeID; {}): Failed to load image '{}', a placeholder was used instead", m_ID, path.string());
		}
	}

	OpenGLTexture2D::~OpenGLTexture2D() {
		CORI_PROFILE_FUNCTION();
		glDeleteTextures(1, &m_ID);
	}

	void OpenGLTexture2D::Bind(const uint32_t slot) const {
		CORI_PROFILE_FUNCTION();
		glBindTextureUnit(slot, m_ID);
	}

	bool OpenGLTexture2D::HasSemiTransparency() const {
		return m_HasSemiTransparency;
	}
}
