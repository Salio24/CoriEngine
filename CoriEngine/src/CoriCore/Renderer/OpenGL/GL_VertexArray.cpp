// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_VertexArray.hpp"
#include <glad/gl.h>

namespace Cori {

	static constexpr GLenum ShaderDataTypeToGLDataType(ShaderDataType type) {
		constexpr GLenum sizes[] = {
			GL_NONE,            // None
			GL_FLOAT,           // Float
			GL_FLOAT,           // Vec2
			GL_FLOAT,           // Vec3
			GL_FLOAT,           // Vec4
			GL_FLOAT,           // Mat3
			GL_FLOAT,           // Mat4
			GL_INT,             // Int
			GL_INT,             // Int2
			GL_INT,             // Int3
			GL_INT,             // Int4
			GL_UNSIGNED_INT,    // UInt
			GL_UNSIGNED_INT,    // UInt2
			GL_UNSIGNED_INT,    // UInt3
			GL_UNSIGNED_INT,    // UInt4
			GL_BOOL,            // Bool
		};

		static_assert(sizeof(sizes) / sizeof(GLenum) == (GLenum)ShaderDataType::Bool + 1, "ShaderDataTypeSize: Size array is out of sync with ShaderDataType enum");

		if ((GLenum)type >= (sizeof(sizes) / sizeof(GLenum)))
		{
			CORI_CORE_ASSERT_ERROR(false, "Unknown shader data type");
			return 0;
		}

		return sizes[(GLenum)type];
	}

	OpenGLVertexArray::OpenGLVertexArray() {
		glGenVertexArrays(1, &m_ID);
		glBindVertexArray(0);
	}

	OpenGLVertexArray::~OpenGLVertexArray() {
		glBindVertexArray(m_ID);
		glDeleteVertexArrays(1, &m_ID);
	}

	void OpenGLVertexArray::Bind() const {
		glBindVertexArray(m_ID);
	}

	void OpenGLVertexArray::Unbind() const {
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) {
		if CORI_CORE_ASSERT_ERROR(vertexBuffer->GetLayout().GetElements().size(), "Vertex buffer has no layout") return;
		// explicitly binding is great for readability, tho i can impact performance, need to think of a way to strip unnecessary binds on release build

		glBindVertexArray(m_ID); 
		vertexBuffer->Bind();
		uint32_t index = 0;
		const VBLayout& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout) {
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index, element.GetComponentCount(), ShaderDataTypeToGLDataType(element.m_Type), element.m_Normalized ? GL_TRUE : GL_FALSE, layout.GetStrinde(), (const void*)element.m_Offset);
			index++;
		}
		vertexBuffer->Unbind();

		m_VertexBuffers.push_back(vertexBuffer);
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
		if CORI_CORE_ASSERT_ERROR(m_VertexBuffers.size(), "Maybe you'll add a valid vertex buffer before adding an index buffer, what do you think?") return;

		glBindVertexArray(m_ID);
		indexBuffer->Bind();
		m_IndexBuffer = indexBuffer;
		indexBuffer->Unbind();
		glBindVertexArray(0);
	}

}