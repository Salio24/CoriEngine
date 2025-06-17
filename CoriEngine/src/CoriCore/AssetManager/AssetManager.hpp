#pragma once
#include "ShaderDescriptor.hpp"
#include "../Renderer/ShaderProgram.hpp"
#include "Texture2DDescriptor.hpp"
#include "../Renderer/Texture.hpp"
#include "../Renderer/SpriteAtlas.hpp"
#include "SpriteAtlasDescriptor.hpp"
#include "../Renderer/Sprite.hpp"

namespace Cori {

	class AssetManager {
	public:

		// TODO: make owning getters that dont add to hashmap

		static std::shared_ptr<ShaderProgram> GetShader(const ShaderProgramDescriptor& descriptor) { return Get().GetShaderImpl(descriptor); }
		static std::shared_ptr<Texture2D> GetTexture2D(const Texture2DDescriptor& descriptor) { return Get().GetTexture2DImpl(descriptor); }
		static std::shared_ptr<SpriteAtlas> GetSpriteAtlas(const SpriteAtlasDescriptor& descriptor) { return Get().GetSpriteAtlasImpl(descriptor); }


		static void PreloadShaders(std::initializer_list<ShaderProgramDescriptor> descriptors) { Get().PreloadShadersImpl(descriptors); }
		static void PreloadTexture2Ds(std::initializer_list<Texture2DDescriptor> descriptors) { Get().PreloadTexture2DsImpl(descriptors); }
		static void PreloadSpriteAtlases(std::initializer_list<SpriteAtlasDescriptor> descriptors) { Get().PreloadSpriteAtlasesImpl(descriptors); }


		static void ClearShaderCache() { Get().ClearShaderCacheImpl(); }
		static void ClearTexture2DCache() { Get().ClearTexture2DCacheImpl(); }
		static void ClearSpriteAtlasCache() { Get().ClearSpriteAtlasCacheImpl(); }

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
		std::shared_ptr<SpriteAtlas> GetSpriteAtlasImpl(const SpriteAtlasDescriptor& descriptor);


		void PreloadShadersImpl(std::initializer_list<ShaderProgramDescriptor> descriptors);
		void PreloadTexture2DsImpl(std::initializer_list<Texture2DDescriptor> descriptors);
		void PreloadSpriteAtlasesImpl(std::initializer_list<SpriteAtlasDescriptor> descriptors);


		void ClearShaderCacheImpl() { m_ShaderCache.clear(); }
		void ClearTexture2DCacheImpl() { m_Texture2DCache.clear(); }
		void ClearSpriteAtlasCacheImpl() { m_SpriteAtlasCache.clear(); }


		std::unordered_map<uint32_t, std::shared_ptr<ShaderProgram>> m_ShaderCache;
		std::unordered_map<uint32_t, std::shared_ptr<Texture2D>> m_Texture2DCache;
		std::unordered_map<uint32_t, std::shared_ptr<SpriteAtlas>> m_SpriteAtlasCache;

	};
}
