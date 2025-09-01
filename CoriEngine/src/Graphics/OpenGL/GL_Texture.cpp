#include "GL_Texture.hpp"
#include <glad/gl.h>

namespace Cori {
	bool OpenGLTexture2D::PreCreateHook([[maybe_unused]] const void* pixelData, [[maybe_unused]] const uint32_t width, [[maybe_unused]] const uint32_t height, [[maybe_unused]] const bool hasSemiTransparency, [[maybe_unused]] const PixelFormat pixelFormat, [[maybe_unused]] const WrapMode wrapMode, [[maybe_unused]] const Filter filter) {
		return true;
	}

	OpenGLTexture2D::OpenGLTexture2D(const void* pixelData, const uint32_t width, const uint32_t height, const bool hasSemiTransparency, const PixelFormat pixelFormat, const WrapMode wrapMode, const Filter filter) : m_Width(width), m_Height(height), m_HasSemiTransparency(hasSemiTransparency) {
		CORI_PROFILE_FUNCTION();
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self , Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::Texture2D }, "Creating texture from preloaded image.", m_ID);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);

		switch (pixelFormat) {
		case RGBA8888:
			glTextureStorage2D(m_ID, 1, GL_RGBA8, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
			break;
		case RGB888:
			glTextureStorage2D(m_ID, 1, GL_RGB8, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));
			m_HasSemiTransparency = false;
			break;
		}

		switch (filter) {
		case LINEAR:
			glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			break;
		case NEAREST:
			glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			break;
		}

		switch (wrapMode) {
		case CLAMP_TO_EDGE:
			glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			break;
		case CLAMP_TO_BORDER:
			glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			break;
		case REPEAT:
			glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_REPEAT);
			break;
		}

		switch (pixelFormat) {
		case RGBA8888:
			glTextureSubImage2D(m_ID, 0, 0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height), GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
			break;
		case RGB888:
			glTextureSubImage2D(m_ID, 0, 0, 0, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height), GL_RGB, GL_UNSIGNED_BYTE, pixelData);
			break;
		}

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
