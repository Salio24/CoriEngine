#include "Buffers.hpp"

namespace Cori {
	namespace Graphics {
		std::shared_ptr<VertexBuffer> VertexBuffer::Create() {
			return nullptr;
		}

		std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t* indices, const uint32_t count) {
			return nullptr;
		}
	}
}
