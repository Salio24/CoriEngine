#pragma once
#include "ShaderDescriptor.hpp"
#include "../Renderer/ShaderProgram.hpp"
#include "Texture2DDescriptor.hpp"
#include "../Renderer/Texture.hpp"

namespace Cori {

	class AssetManager {
	public:

		static std::shared_ptr<ShaderProgram> GetShader(const ShaderProgramDescriptor& descriptor) { return Get().GetShaderImpl(descriptor); }
		static std::shared_ptr<Texture2D> GetTexture2D(const Texture2DDescriptor& descriptor) { return Get().GetTexture2DImpl(descriptor); }

		static void PreloadShaders(std::initializer_list<ShaderProgramDescriptor> descriptors) { Get().PreloadShadersImpl(descriptors); }
		static void PreloadTexture2Ds(std::initializer_list<Texture2DDescriptor> descriptors) { Get().PreloadTexture2DsImpl(descriptors); }

		static void ClearShaderCache() { Get().ClearShaderCacheImpl(); }
		static void ClearTexture2DCache() { Get().ClearTexture2DCacheImpl(); }

	private:

		AssetManager() = default;
		~AssetManager() = default;

		AssetManager(const AssetManager&) = delete;
		AssetManager& operator=(const AssetManager&) = delete;
		AssetManager(AssetManager&&) = delete;
		AssetManager& operator=(AssetManager&&) = delete;

		static AssetManager& Get();

		std::shared_ptr<ShaderProgram> GetShaderImpl(const ShaderProgramDescriptor& descriptor);
		std::shared_ptr<Texture2D> GetTexture2DImpl(const Texture2DDescriptor& descriptor);

		void PreloadShadersImpl(std::initializer_list<ShaderProgramDescriptor> descriptors);
		void PreloadTexture2DsImpl(std::initializer_list<Texture2DDescriptor> descriptors);

		void ClearShaderCacheImpl() { m_ShaderCache.clear(); }
		void ClearTexture2DCacheImpl() { m_Texture2DCache.clear(); }

		std::unordered_map<uint32_t, std::shared_ptr<ShaderProgram>> m_ShaderCache;
		std::unordered_map<uint32_t, std::shared_ptr<Texture2D>> m_Texture2DCache;

	};
}
