#pragma once
#include "../Buffers.hpp"


namespace Cori {

	class OpenGLVertexArray;

	class OpenGLVertexBuffer : public VertexBuffer {
	public:
		OpenGLVertexBuffer() {};
		virtual ~OpenGLVertexBuffer();
		virtual void Init(const float* vertices, uint32_t size, DRAW_TYPE drawtype) override;
		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void SetData(const void* data, uint32_t size) const override;

		// log layout
		virtual void SetLayout(const VBLayout& layout) override;
		virtual const VBLayout& GetLayout() const override { return m_Layout; }

	private:
		friend class OpenGLVertexArray;

		uint32_t m_ID;
		VBLayout m_Layout;
	};

	class OpenGLIndexBuffer : public IndexBuffer {
	public:
		OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
		virtual ~OpenGLIndexBuffer();
		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual uint32_t GetCount() const override;
		
	private:
		friend class OpenGLVertexArray;

		uint32_t m_ID;
		uint32_t m_Count{ 0 };
		
	};
}