#include "GL_Buffers.hpp"
#include <glad/gl.h>
#include <magic_enum/magic_enum.hpp>

namespace Cori {
	namespace Graphics {
		namespace Internal {
			OpenGLVertexBuffer::OpenGLVertexBuffer() {
				CORI_PROFILE_FUNCTION();
				glCreateBuffers(1, &m_ID);
			}


			OpenGLVertexBuffer::~OpenGLVertexBuffer() {
				glBindBuffer(GL_ARRAY_BUFFER, m_ID);
				glDeleteBuffers(1, &m_ID);
			}

			void OpenGLVertexBuffer::Init(const float* vertices, uint32_t size, const DRAW_TYPE drawType) {
				CORI_PROFILE_FUNCTION();
				glCreateBuffers(1, &m_ID);
				glBindBuffer(GL_ARRAY_BUFFER, m_ID);
				switch (drawType)
				{
				case DRAW_TYPE::DYNAMIC:
					glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), vertices, GL_DYNAMIC_DRAW);
					break;
				case DRAW_TYPE::STATIC:
					glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(size), vertices, GL_STATIC_DRAW);
					break;
				default:
					CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::VertexBuffer }, "Unknown draw type selected");
					break;
				}

				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::VertexBuffer }, "(GL_RuntimeID: {}): VBO with size {}, and type {}, was created successfully", m_ID, size, drawType == DRAW_TYPE::DYNAMIC ? "DYNAMIC_DRAW" : drawType == DRAW_TYPE::STATIC ? "STATIC_DRAW" : "ERROR");

#ifdef DEBUG_BUILD
				{
					std::string layoutText;

					uint32_t index = 0;
					for (const auto& element : m_Layout) {
						std::string element_layout = CORI_SECOND_LINE_SPACING + "Location: '" + std::to_string(index) +
							"' | Type: '" + static_cast<std::string>(magic_enum::enum_name(element.m_Type)) +
							"' | Name: '" + element.m_Name + "'" +
							"' | Divisor: '" + std::to_string(element.m_Divisor) + "'" +
							"' | Normalized: '" + Logger::BoolAlpha(element.m_Normalized) + "'";

						layoutText.append(element_layout);
						if (element != m_Layout.back()) { // NOLINT
							layoutText.append("\n"); // NOLINT
						}

						if (element.m_Type == ShaderDataType::Mat3) {
							index+=3;
						} else if (element.m_Type == ShaderDataType::Mat4) {
							index+=4;
						} else {
							index++;
						}

					}

					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::VertexBuffer }, "(GL_RuntimeID: {}): VBO has the following Attribute Layout: \n{}", m_ID, layoutText);
				}
#endif
			}

			void OpenGLVertexBuffer::Bind() const {
				CORI_PROFILE_FUNCTION();
				glBindBuffer(GL_ARRAY_BUFFER, m_ID);
			}

			void OpenGLVertexBuffer::Unbind() const {
				CORI_PROFILE_FUNCTION();
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}

			void OpenGLVertexBuffer::SetData(const void* data, const uint32_t size) const {
				CORI_PROFILE_FUNCTION();
				glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
			}

			void OpenGLVertexBuffer::SetLayout(const VBLayout& layout)
			{
				m_Layout = layout;
			}

			OpenGLIndexBuffer::OpenGLIndexBuffer(const uint32_t* indices, uint32_t count) : m_Count(count) {
				CORI_PROFILE_FUNCTION();
				glCreateBuffers(1, &m_ID);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(count * sizeof(uint32_t)), indices, GL_STATIC_DRAW);

				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::OpenGL, Logger::Tags::Graphics::IndexBuffer }, "(GL_RuntimeID: {}): IBO with size {}, and type STATIC_DRAW, was created successfully", m_ID, count);
			}

			OpenGLIndexBuffer::~OpenGLIndexBuffer() {
				CORI_PROFILE_FUNCTION();
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
				glDeleteBuffers(1, &m_ID);
			}

			void OpenGLIndexBuffer::Bind() const {
				CORI_PROFILE_FUNCTION();
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
			}

			void OpenGLIndexBuffer::Unbind() const {
				CORI_PROFILE_FUNCTION();
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}

			uint32_t OpenGLIndexBuffer::GetCount() const {
				return m_Count;
			}
		}
	}
}

