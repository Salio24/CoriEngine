#include "GL_VertexArray.hpp"
#include <glad/gl.h>
#include "GL_Buffers.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
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

				static_assert(sizeof(sizes) / sizeof(GLenum) == static_cast<GLenum>(ShaderDataType::Bool) + 1, "ShaderDataTypeToGLDataType: Size array is out of sync with ShaderDataType enum");

				if (CORI_CORE_CHECK(static_cast<GLenum>(type) < sizeof(sizes) / sizeof(GLenum), "ShaderDataTypeToGLDataType: Unknown shader data type '{}'", static_cast<int>(type))) { return 0; }

				return sizes[static_cast<GLenum>(type)];
			}

			OpenGLVertexArray::OpenGLVertexArray() {
				glGenVertexArrays(1, &m_ID);
			}

			OpenGLVertexArray::~OpenGLVertexArray() {
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

				if (CORI_CORE_CHECK(!vertexBuffer->GetLayout().GetElements().empty(), "VertexArray: (GL_RuntimeID: {}): Trying to add VBO that has no layout", m_ID)) { return; }

				glBindVertexArray(m_ID);
				vertexBuffer->Bind();
				uint32_t index = 0;
				const VBLayout& layout = vertexBuffer->GetLayout();
				for (const auto& element : layout) {
					if (element.m_Type == ShaderDataType::Mat3) {
						for (size_t i = 0; i < element.GetComponentCount(); ++i) {
							glEnableVertexAttribArray(index);
							glVertexAttribPointer(index, static_cast<GLint>(element.GetComponentCount()), ShaderDataTypeToGLDataType(element.m_Type), element.m_Normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.GetStride()), reinterpret_cast<const void*>(element.m_Offset + sizeof(float) * element.GetComponentCount() * i));

							if (element.m_Divisor > 0) {
								glVertexAttribDivisor(index, element.m_Divisor);
							}

							++index;
						}
					}
					else if (element.m_Type == ShaderDataType::Mat4) {
						for (size_t i = 0; i < element.GetComponentCount(); i++) {
							glEnableVertexAttribArray(index);
							glVertexAttribPointer(index, static_cast<GLint>(element.GetComponentCount()), ShaderDataTypeToGLDataType(element.m_Type), element.m_Normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.GetStride()), reinterpret_cast<const void*>(element.m_Offset + sizeof(float) * element.GetComponentCount() * i));

							if (element.m_Divisor > 0) {
								glVertexAttribDivisor(index, element.m_Divisor);
							}

							++index;
						}
					}
					else {
						glEnableVertexAttribArray(index);
						glVertexAttribPointer(index, static_cast<GLint>(element.GetComponentCount()), ShaderDataTypeToGLDataType(element.m_Type), element.m_Normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(layout.GetStride()), reinterpret_cast<const void*>(element.m_Offset));

						if (element.m_Divisor > 0) {
							glVertexAttribDivisor(index, element.m_Divisor);
						}

						++index;
					}
				}

				m_VertexBuffers.push_back(vertexBuffer);
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::VertexArray }, "(GL_RuntimeID: {}): VertexBuffer with GL_RuntimeID: {} was added to successfully", m_ID ,std::static_pointer_cast<OpenGLVertexBuffer>(vertexBuffer)->m_ID);
			}

			void OpenGLVertexArray::AddIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) {
				CORI_PROFILE_FUNCTION();

				if (CORI_CORE_CHECK(!m_VertexBuffers.empty(), "VertexArray: (GL_RuntimeID: {}): adding IBO to VAO before a valid VBO was added", m_ID)) { return; }

				glBindVertexArray(m_ID);
				indexBuffer->Bind();
				m_IndexBuffer = indexBuffer;
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::VertexArray }, "(GL_RuntimeID: {}) : IndexBuffer with GL_RuntimeID : {} was added to successfully", m_ID ,reinterpret_pointer_cast<OpenGLIndexBuffer>(indexBuffer)->m_ID);
			}
		}
	}
}