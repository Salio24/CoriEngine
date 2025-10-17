#pragma once
#include "../Buffers.hpp"
#include "Profiling/Trackable.hpp"

namespace Cori {
	namespace Graphics {
		namespace Internal {
			class OpenGLVertexArray;

			class OpenGLVertexBuffer final : public VertexBuffer, public Profiling::Trackable<OpenGLVertexBuffer, VertexBuffer> {
			public:
				OpenGLVertexBuffer();
				~OpenGLVertexBuffer() override;
				void Init(const float* vertices, uint32_t size, const DRAW_TYPE drawType) override;
				void Bind() const override;
				void Unbind() const override;

				void SetData(const void* data, uint32_t size) const override;

				void SetLayout(const VBLayout& layout) override;
				const VBLayout& GetLayout() const override { return m_Layout; }

			private:
				friend class OpenGLVertexArray;

				uint32_t m_ID;
				VBLayout m_Layout;
			};

			class OpenGLIndexBuffer final : public IndexBuffer, public Profiling::Trackable<OpenGLIndexBuffer, IndexBuffer> {
			public:
				OpenGLIndexBuffer(const uint32_t* indices, uint32_t count);
				~OpenGLIndexBuffer() override;
				void Bind() const override;
				void Unbind() const override;

				uint32_t GetCount() const override;

			private:
				friend class OpenGLVertexArray;

				uint32_t m_ID;
				uint32_t m_Count{ 0 };
			};
		}
	}
}