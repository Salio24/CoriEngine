// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "GL_VertexArray.hpp"
#include <glad/gl.h>
#include "GL_Buffers.hpp"

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

		static_assert(sizeof(sizes) / sizeof(GLenum) == (GLenum)ShaderDataType::Bool + 1, "ShaderDataTypeToGLDataType: Size array is out of sync with ShaderDataType enum");

		if ((GLenum)type >= (sizeof(sizes) / sizeof(GLenum)))
		{
			CORI_CORE_ASSERT_ERROR(false, "ShaderDataTypeToGLDataType: Unknown shader data type");
			return 0;
		}

		return sizes[(GLenum)type];
	}

	bool OpenGLVertexArray::PreCreateHook() {
		return true;
	}

	OpenGLVertexArray::OpenGLVertexArray() {
		CORI_PROFILE_FUNCTION();
		glGenVertexArrays(1, &m_ID);
		glBindVertexArray(0);	
	}

	OpenGLVertexArray::~OpenGLVertexArray() {
		CORI_PROFILE_FUNCTION();
		glBindVertexArray(m_ID);
		glDeleteVertexArrays(1, &m_ID);
	}

	void OpenGLVertexArray::Bind() const {
		CORI_PROFILE_FUNCTION();
		glBindVertexArray(m_ID);
	}

	void OpenGLVertexArray::Unbind() const {
		CORI_PROFILE_FUNCTION();
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) {
		CORI_PROFILE_FUNCTION();
		if (CORI_CORE_ASSERT_ERROR(vertexBuffer->GetLayout().GetElements().size(), "GL_VertexArray (GL_RuntimeID: {0}): Trying to add VBO that has no layout", m_ID)) { return; }
		//explicitly binding is great for readability, tho it can impact performance, need to think of a way to strip unnecessary binds on release build
		
		glBindVertexArray(m_ID); 
		vertexBuffer->Bind();
		uint32_t index = 0;
		const VBLayout& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout) {
			glEnableVertexAttribArray(index);
			glVertexAttribPointer(index, static_cast<GLint>(element.GetComponentCount()), ShaderDataTypeToGLDataType(element.m_Type), element.m_Normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.GetStrinde()), (const void*)element.m_Offset);
			index++;
		}
		vertexBuffer->Unbind();

		m_VertexBuffers.push_back(vertexBuffer);
		CORI_CORE_TRACE("GL_VertexArray (GL_RuntimeID: {0}): VertexBuffer with GL_RuntimeID: {1} was added to successfully", m_ID ,std::static_pointer_cast<OpenGLVertexBuffer>(vertexBuffer)->m_ID);
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
		CORI_PROFILE_FUNCTION();
		if (CORI_CORE_ASSERT_ERROR(m_VertexBuffers.size(), "GL_VertexArray (GL_RuntimeID: {0}): adding IBO to VAO before a valid VBO was added", m_ID)) { return; }

		glBindVertexArray(m_ID);
		indexBuffer->Bind();
		m_IndexBuffer = indexBuffer;
		CORI_CORE_TRACE("GL_VertexArray (GL_RuntimeID: {0}) : IndexBuffer with GL_RuntimeID : {1} was added to successfully", m_ID ,reinterpret_pointer_cast<OpenGLIndexBuffer>(indexBuffer)->m_ID);
		indexBuffer->Unbind();
		glBindVertexArray(0);
	}

}