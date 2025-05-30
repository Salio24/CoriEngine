#pragma once
#include "../Buffers.hpp"
#include "../../Profiling/Trackable.hpp"
#include "../../AutoRegisteringFactory.hpp"

namespace Cori {

	class OpenGLVertexArray;

	class OpenGLVertexBuffer : public VertexBuffer, public Profiling::Trackable<OpenGLVertexBuffer, VertexBuffer>, public RegisterInSharedFactory<VertexBuffer, OpenGLVertexBuffer, GraphicsAPIs, GraphicsAPIs::OpenGL> {
	public:
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
		CORI_DECLARE_SHARED_FACTORY_REGISTERED(OpenGLVertexBuffer, ());
	};

	class OpenGLIndexBuffer : public IndexBuffer, public Profiling::Trackable<OpenGLIndexBuffer, IndexBuffer>, public RegisterInSharedFactory<IndexBuffer, OpenGLIndexBuffer, GraphicsAPIs, GraphicsAPIs::OpenGL, uint32_t*, uint32_t> {
	public:

		virtual ~OpenGLIndexBuffer();
		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual uint32_t GetCount() const override;
		
	private:
		friend class OpenGLVertexArray;

		uint32_t m_ID;
		uint32_t m_Count{ 0 };
		
		CORI_DECLARE_SHARED_FACTORY_REGISTERED(OpenGLIndexBuffer, (uint32_t* indices, uint32_t count));
	};
}