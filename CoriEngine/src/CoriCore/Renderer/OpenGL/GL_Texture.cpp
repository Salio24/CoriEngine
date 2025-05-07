// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_Texture.hpp"
#include "../Image.hpp"
#include <glad/gl.h>

namespace Cori {

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path) {
		std::unique_ptr<Image> image = std::make_unique<Image>(path);
		m_Width = image->GetWidth();
		m_Height = image->GetHeight();
		
		glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);

		glTextureStorage2D(m_ID, 1, GL_RGBA8, static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height));

		glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
		glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		image->FlipVertically();

		glTextureSubImage2D(m_ID, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, image->GetPixelData());
	}

	OpenGLTexture2D::~OpenGLTexture2D() {
		glDeleteTextures(1, &m_ID);
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const {
		glBindTextureUnit(slot, m_ID);
	}

}