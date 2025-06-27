#pragma once
#include "ShaderDescriptor.hpp"
#include "Texture2DDescriptor.hpp"
#include "Renderer/ShaderProgram.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/SpriteAtlas.hpp"
#include "Renderer/Sprite.hpp"
#include "SpriteAtlasDescriptor.hpp"

namespace Cori {

	class AssetManager {
	public:

		// TODO: make owning getters that dont add to hashmap

		static std::shared_ptr<ShaderProgram> GetShader(const ShaderProgramDescriptor& descriptor) { return Get().GetShaderImpl(descriptor); }
		static std::shared_ptr<Texture2D> GetTexture2D(const Texture2DDescriptor& descriptor) { return Get().GetTexture2DImpl(descriptor); }
		static std::shared_ptr<SpriteAtlas> GetSpriteAtlas(const SpriteAtlasDescriptor& descriptor) { return Get().GetSpriteAtlasImpl(descriptor); }

		static std::shared_ptr<Texture2D> GetTexture2DOwning(const Texture2DDescriptor& descriptor) { return Get().GetTexture2DOwningImpl(descriptor); }
		static std::shared_ptr<SpriteAtlas> GetSpriteAtlasOwning(const SpriteAtlasDescriptor& descriptor) { return Get().GetSpriteAtlasOwningImpl(descriptor); }


		static void PreloadShaders(std::initializer_list<ShaderProgramDescriptor> descriptors) { Get().PreloadShadersImpl(descriptors); }
		static void PreloadTexture2Ds(std::initializer_list<Texture2DDescriptor> descriptors) { Get().PreloadTexture2DsImpl(descriptors); }
		static void PreloadSpriteAtlases(std::initializer_list<SpriteAtlasDescriptor> descriptors) { Get().PreloadSpriteAtlasesImpl(descriptors); }

		static void UnloadShader(const ShaderProgramDescriptor& descriptor) { Get().UnloadShaderImpl(descriptor); }
		static void UnloadTexture2D(const Texture2DDescriptor& descriptor) { Get().UnloadTexture2DImpl(descriptor); }
		static void UnloadSpriteAtlas(const SpriteAtlasDescriptor& descriptor) { Get().UnloadSpriteAtlasImpl(descriptor); }

		static void UnloadShaders(std::initializer_list<ShaderProgramDescriptor> descriptors) { Get().UnloadShadersImpl(descriptors); }
		static void UnloadTexture2Ds(std::initializer_list<Texture2DDescriptor> descriptors) { Get().UnloadTexture2DsImpl(descriptors); }
		static void UnloadSpriteAtlases(std::initializer_list<SpriteAtlasDescriptor> descriptors) { Get().UnloadSpriteAtlasesImpl(descriptors); }

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

		std::shared_ptr<Texture2D> GetTexture2DOwningImpl(const Texture2DDescriptor& descriptor);
		std::shared_ptr<SpriteAtlas> GetSpriteAtlasOwningImpl(const SpriteAtlasDescriptor& descriptor);

		void PreloadShadersImpl(std::initializer_list<ShaderProgramDescriptor> descriptors);
		void PreloadTexture2DsImpl(std::initializer_list<Texture2DDescriptor> descriptors);
		void PreloadSpriteAtlasesImpl(std::initializer_list<SpriteAtlasDescriptor> descriptors);

		void UnloadShaderImpl(const ShaderProgramDescriptor& descriptor);
		void UnloadTexture2DImpl(const Texture2DDescriptor& descriptor);
		void UnloadSpriteAtlasImpl(const SpriteAtlasDescriptor& descriptor);

		void UnloadShadersImpl(std::initializer_list<ShaderProgramDescriptor> descriptors);
		void UnloadTexture2DsImpl(std::initializer_list<Texture2DDescriptor> descriptors);
		void UnloadSpriteAtlasesImpl(std::initializer_list<SpriteAtlasDescriptor> descriptors);

		void ClearShaderCacheImpl() { m_ShaderCache.clear(); }
		void ClearTexture2DCacheImpl() { m_Texture2DCache.clear(); }
		void ClearSpriteAtlasCacheImpl() { m_SpriteAtlasCache.clear(); }


		std::unordered_map<uint32_t, std::shared_ptr<ShaderProgram>> m_ShaderCache;
		std::unordered_map<uint32_t, std::shared_ptr<Texture2D>> m_Texture2DCache;
		std::unordered_map<uint32_t, std::shared_ptr<SpriteAtlas>> m_SpriteAtlasCache;

	};
}
