#pragma once
#include "GL_Buffers.hpp"
#include <glad/gl.h>

namespace Cori {
	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size, DRAW_TYPE drawtype) {
		glCreateBuffers(1, &m_ID);
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
		switch (drawtype)
		{
		case DRAW_TYPE::DYNAMIC:
			glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_DYNAMIC_DRAW);
			break;
		case DRAW_TYPE::STATIC:
			glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
			break;
		default:
			break;
		}
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer() {
		glDeleteBuffers(1, &m_ID);
	}

	void OpenGLVertexBuffer::Bind() const {
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
	}

	void OpenGLVertexBuffer::Unbind() const {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}



	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count) : m_Count(count) {
		glCreateBuffers(1, &m_ID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer() {
		glDeleteBuffers(1, &m_ID);
	}

	void OpenGLIndexBuffer::Bind() const {
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
	}

	void OpenGLIndexBuffer::Unbind() const {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	uint32_t OpenGLIndexBuffer::GetCount() const {
		return m_Count;
	}

}

