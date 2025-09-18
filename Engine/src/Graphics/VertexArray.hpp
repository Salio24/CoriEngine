#pragma once
#include "Buffers.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class VertexArray {
			public:
				virtual ~VertexArray() = default;
				virtual void Bind() const = 0;
				virtual void Unbind() const = 0;

				virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) = 0;
				virtual void AddIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) = 0;

				[[nodiscard]] virtual const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const = 0;
				[[nodiscard]] virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const = 0;

				[[nodiscard]] static std::shared_ptr<VertexArray> Create();
			};
		}
	}
}