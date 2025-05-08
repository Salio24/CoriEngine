// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_Buffers.hpp"
#include <glad/gl.h>
#include <iostream>
#include <magic_enum/magic_enum.hpp>

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

		CORI_CORE_TRACE("GL_VertexBuffer (GL_RuntimeID: {0}): VBO with size {1}, and type {2}, was created successfully", m_ID, size, (drawtype == DRAW_TYPE::DYNAMIC ? "DYNAMIC_DRAW" : drawtype == DRAW_TYPE::STATIC ? "STATIC_DRAW" : "ERROR"));
		
#ifdef DEBUG_BUILD  
		std::string layoutText;

		uint32_t index = 0;
		for (const auto& element : m_Layout) {
			std::string element_layout = "    Location: '" + std::to_string(index) +
				"' | Type: '" + static_cast<std::string>(magic_enum::enum_name(element.m_Type)) +
				"' | Name: '" + element.m_Name +
				"' | Stride: '" + std::to_string(element.m_Offset) +
				"' | Size: '" + std::to_string(element.m_Size) + "'";

			layoutText.append(element_layout);

			if (element != m_Layout.back()) {
				layoutText.append("\n");
			}

			index++;
		}

		CORI_CORE_TRACE("GL_VertexBuffer (GL_RuntimeID: {0}): VBO has the following Attribute Layout: \n{1}", m_ID, layoutText);
#endif

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

	void OpenGLVertexBuffer::SetLayout(const VBLayout& layout)
	{
		m_Layout = layout;
	}

	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count) : m_Count(count) {
		glCreateBuffers(1, &m_ID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(count), indices, GL_STATIC_DRAW);

		CORI_CORE_TRACE("GL_IndexBuffer (GL_RuntimeID: {0}): IBO with size {1}, and type STATIC_DRAW, was created successfully", m_ID, count);

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

