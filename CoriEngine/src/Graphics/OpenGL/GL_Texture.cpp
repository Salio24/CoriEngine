#include "GL_Texture.hpp"
#include <glad/gl.h>

namespace Cori {
	bool OpenGLTexture2D::PreCreateHook([[maybe_unused]] const std::shared_ptr<Image>& image) {
		return true;
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::shared_ptr<Image>& image) {
		CORI_PROFILE_FUNCTION();
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self , Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::Texture2D }, "Creating texture from preloaded image.", m_ID);
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

		glTextureSubImage2D(m_ID, 0, 0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height), GL_RGBA, GL_UNSIGNED_BYTE, image->GetPixelData());
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self , Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::Texture2D }, "(GL_RuntimeID; {}): Successfully created texture from preloaded image.", m_ID);
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
