// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_Buffers.hpp"
#include <glad/gl.h>

namespace Cori {

	OpenGLVertexBuffer::~OpenGLVertexBuffer() {
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
		glDeleteBuffers(1, &m_ID);
	}

	void OpenGLVertexBuffer::Init(const float* vertices, uint32_t size, DRAW_TYPE drawtype) {
		glCreateBuffers(1, &m_ID);
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
		switch (drawtype)
		{
		case DRAW_TYPE::DYNAMIC:
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), vertices, GL_DYNAMIC_DRAW);
			break;
		case DRAW_TYPE::STATIC:
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), vertices, GL_STATIC_DRAW);
			break;
		default:
			CORI_CORE_WARN("Unknown draw type selected");
			break;
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void OpenGLVertexBuffer::Bind() const {
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
	}

	void OpenGLVertexBuffer::Unbind() const {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void OpenGLVertexBuffer::SetData(const void* data, uint32_t size) const {
		glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
	}

	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count) : m_Count(count) {
		glCreateBuffers(1, &m_ID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(count), indices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer() {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
		glDeleteBuffers(1, &m_ID);
	}

	void OpenGLIndexBuffer::Bind() const {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
	}

	void OpenGLIndexBuffer::Unbind() const {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	uint32_t OpenGLIndexBuffer::GetCount() const {
		return m_Count;
	}

}

