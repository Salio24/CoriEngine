#pragma once
#include "../Texture.hpp"
#include "../GraphicsAPIs.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/AutoRegisteringFactory.hpp"

namespace Cori {
	class OpenGLTexture2D : public Texture2D, public Profiling::Trackable<OpenGLTexture2D, Texture2D>, public RegisterInSharedFactory<Texture2D, OpenGLTexture2D, GraphicsAPIs, GraphicsAPIs::OpenGL, const std::string&> {
	public:
		static bool PreCreateHook(const std::string& path); 
		OpenGLTexture2D(const std::string& path);
		virtual ~OpenGLTexture2D();

		virtual void Bind(uint32_t slot = 0) const override;

		virtual uint32_t GetWidth() const override { return m_Width; }
		virtual uint32_t GetHeight() const override { return m_Height; }

	private:
		uint32_t m_ID{ 0 };
		uint32_t m_Width{ 0 };
		uint32_t m_Height{ 0 };

		CORI_REGISTERED_FACTORY_INIT;
	};
}