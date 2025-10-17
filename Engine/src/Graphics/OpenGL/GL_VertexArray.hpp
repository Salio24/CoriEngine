#pragma once
#include "../VertexArray.hpp"
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class OpenGLVertexArray final : public VertexArray, public Profiling::Trackable<OpenGLVertexArray, VertexArray> {
			public:
				OpenGLVertexArray();
				~OpenGLVertexArray() override;
				void Bind() const override;
				void Unbind() const override;

				void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override;
				void AddIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

				[[nodiscard]] const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
				[[nodiscard]] const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }

			private:
				uint32_t m_ID;
				std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
				std::shared_ptr<IndexBuffer> m_IndexBuffer;
			};
		}
	}
}