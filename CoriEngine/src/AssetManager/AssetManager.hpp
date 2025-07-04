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
		AssetManager() = default;
		~AssetManager() = default;

		static std::shared_ptr<ShaderProgram> GetShader(const ShaderProgramDescriptor& descriptor);
		static std::shared_ptr<Texture2D> GetTexture2D(const Texture2DDescriptor& descriptor);
		static std::shared_ptr<SpriteAtlas> GetSpriteAtlas(const SpriteAtlasDescriptor& descriptor);

		static std::shared_ptr<ShaderProgram> GetShaderOwning(const ShaderProgramDescriptor& descriptor);
		static std::shared_ptr<Texture2D> GetTexture2DOwning(const Texture2DDescriptor& descriptor);
		static std::shared_ptr<SpriteAtlas> GetSpriteAtlasOwning(const SpriteAtlasDescriptor& descriptor);

		static void PreloadShaders(std::initializer_list<ShaderProgramDescriptor> descriptors);
		static void PreloadTexture2Ds(std::initializer_list<Texture2DDescriptor> descriptors);
		static void PreloadSpriteAtlases(std::initializer_list<SpriteAtlasDescriptor> descriptors);

		static void UnloadShader(const ShaderProgramDescriptor& descriptor);
		static void UnloadTexture2D(const Texture2DDescriptor& descriptor);
		static void UnloadSpriteAtlas(const SpriteAtlasDescriptor& descriptor);

		static void UnloadShaders(std::initializer_list<ShaderProgramDescriptor> descriptors);
		static void UnloadTexture2Ds(std::initializer_list<Texture2DDescriptor> descriptors);
		static void UnloadSpriteAtlases(std::initializer_list<SpriteAtlasDescriptor> descriptors);

		static void ClearShaderCache();
		static void ClearTexture2DCache();
		static void ClearSpriteAtlasCache();

	private:

		AssetManager(const AssetManager&) = delete;
		AssetManager& operator=(const AssetManager&) = delete;
		AssetManager(AssetManager&&) = delete;
		AssetManager& operator=(AssetManager&&) = delete;

		std::shared_ptr<ShaderProgram> GetShaderImpl(const ShaderProgramDescriptor& descriptor);
		std::shared_ptr<Texture2D> GetTexture2DImpl(const Texture2DDescriptor& descriptor);
		std::shared_ptr<SpriteAtlas> GetSpriteAtlasImpl(const SpriteAtlasDescriptor& descriptor);

		std::shared_ptr<Cori::ShaderProgram> GetShaderOwningImpl(const ShaderProgramDescriptor& descriptor);
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
