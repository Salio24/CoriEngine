#pragma once
#include "../VertexArray.hpp"
#include "../../Profiling/Trackable.hpp"
#include "../../AutoRegisteringFactory.hpp"


namespace Cori {

	class OpenGLVertexArray : public VertexArray, public Trackable<OpenGLVertexArray, VertexArray>, public RegisterInSharedFactory<VertexArray, OpenGLVertexArray, GraphicsAPIs, GraphicsAPIs::OpenGL> {
	CORI_DECLARE_SHARED_FACTORY_REGISTERED(OpenGLVertexArray, ());
	public:
		virtual ~OpenGLVertexArray();
		virtual void Bind() const override;
		virtual void Unbind() const override;
		 
		virtual void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override;
		virtual void AddIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

		virtual const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const override { return m_VertexBuffers; }
		virtual const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const override { return m_IndexBuffer; }



	private:
		uint32_t m_ID;
		std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
		std::shared_ptr<IndexBuffer> m_IndexBuffer;


	};
}