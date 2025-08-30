#include "AssetManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	AssetManager::Cache* AssetManager::s_Cache{nullptr};

	struct AssetManager::Cache {
		std::unordered_map<uint32_t, std::shared_ptr<ShaderProgram>> m_ShaderCache;
		std::unordered_map<uint32_t, std::shared_ptr<Texture2D>> m_Texture2DCache;
		std::unordered_map<uint32_t, std::shared_ptr<SpriteAtlas>> m_SpriteAtlasCache;
		std::unordered_map<uint32_t, std::shared_ptr<Audio::Sound>> m_SoundCache;
	};

	void AssetManager::Init() {
		s_Cache = new Cache();
	}

	void AssetManager::Shutdown() {
		delete s_Cache;
	}

	std::shared_ptr<ShaderProgram> AssetManager::GetShader(const ShaderProgramDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		if (s_Cache->m_ShaderCache.contains(descriptor.GetRuntimeID())) {
			return s_Cache->m_ShaderCache[descriptor.GetRuntimeID()];
		}

		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "Shader cache miss for '{}' (RuntimeID: {}). Loading...", descriptor.m_Name, descriptor.GetRuntimeID());
		std::shared_ptr<ShaderProgram> newShader = ShaderProgram::Create(descriptor.m_VertexPath, descriptor.m_FragmentPath, descriptor.m_GeometryPath);
		s_Cache->m_ShaderCache[descriptor.GetRuntimeID()] = newShader;
		return newShader;
	}

	std::shared_ptr<Texture2D> AssetManager::GetTexture2D(const Texture2DDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		if (s_Cache->m_Texture2DCache.contains(descriptor.GetRuntimeID())) {
			return s_Cache->m_Texture2DCache[descriptor.GetRuntimeID()];
		}

		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "Texture2D cache miss for '{}' (RuntimeID: {}). Loading...", descriptor.m_Name, descriptor.GetRuntimeID());
		const auto image = Image::Create(descriptor.m_ImagePath);
		std::shared_ptr<Texture2D> newTexture2D = Texture2D::Create(image);
		s_Cache->m_Texture2DCache[descriptor.GetRuntimeID()] = newTexture2D;
		return newTexture2D;
	}

	std::expected<std::shared_ptr<SpriteAtlas>, CoriError<>> AssetManager::GetSpriteAtlas(const SpriteAtlasDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		if (s_Cache->m_SpriteAtlasCache.contains(descriptor.GetRuntimeID())) {
			return s_Cache->m_SpriteAtlasCache[descriptor.GetRuntimeID()];
		}

		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "SpriteAtlas cache miss for '{}' (RuntimeID: {}). Loading...", descriptor.m_Name, descriptor.GetRuntimeID());
		const auto image = Image::Create(descriptor.m_TextureDescriptor.m_ImagePath);
		return std::move(SpriteAtlas::Create(descriptor.m_Name, image, descriptor.m_SpriteResolution)).transform([descriptor](auto&& atlas) {
			s_Cache->m_SpriteAtlasCache[descriptor.GetRuntimeID()] = atlas;
			return atlas;
		});
	}

	std::shared_ptr<Audio::Sound> AssetManager::GetSound(const SoundDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		if (s_Cache->m_SoundCache.contains(descriptor.GetRuntimeID())) {
			return s_Cache->m_SoundCache[descriptor.GetRuntimeID()];
		}

		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "Sound cache miss for '{}' (RuntimeID: {}). Loading...", descriptor.m_Name, descriptor.GetRuntimeID());
		std::shared_ptr<Audio::Sound> newSound = Audio::Sound::Create(descriptor.m_Name, descriptor.m_Path, descriptor.m_PreDecode);
		s_Cache->m_SoundCache[descriptor.GetRuntimeID()] = newSound;
		return newSound;
	}

	void AssetManager::PreloadShaders(const std::initializer_list<ShaderProgramDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloading {} shader(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			static_cast<void>(GetShader(descriptor)); // explicitly ignoring result
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloaded {} shader(s)", descriptors.size());
	}

	void AssetManager::PreloadTexture2Ds(const std::initializer_list<Texture2DDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloading {} Texture2D(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			static_cast<void>(GetTexture2D(descriptor));  // explicitly ignoring result
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloaded {} Texture2D(s)", descriptors.size());
	}

	void AssetManager::PreloadSpriteAtlases(const std::initializer_list<SpriteAtlasDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloading {} SpriteAtlas(es)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			GetSpriteAtlas(descriptor).transform_error([](auto&& error) {
				CORI_CORE_ERROR_TAGGED({Logger::Tags::AssetManager::Self}, "Failed to preload SpriteAtlas. Error: '{}'", error.what());
				return error;
			});
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloaded {} SpriteAtlas(es)", descriptors.size());
	}

	void AssetManager::PreloadSounds(const std::initializer_list<SoundDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloading {} Sound(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			static_cast<void>(GetSound(descriptor));  // explicitly ignoring result
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Preloaded {} Sound(es)", descriptors.size());
	}

	void AssetManager::UnloadShader(const ShaderProgramDescriptor& descriptor) {
		if (!s_Cache->m_ShaderCache.contains(descriptor.GetRuntimeID())) {
			CORI_CORE_WARN_TAGGED({Logger::Tags::AssetManager::Self}, "Trying to unload Shader that doesn't exist, name '{}', (RuntimeID: {})", descriptor.m_Name, descriptor.GetRuntimeID());
			return;
		}
		s_Cache->m_ShaderCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded Shader '{}' (RuntimeID: {}).", descriptor.m_Name, descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadTexture2D(const Texture2DDescriptor& descriptor) {
		if (!s_Cache->m_Texture2DCache.contains(descriptor.GetRuntimeID())) {
			CORI_CORE_WARN_TAGGED({Logger::Tags::AssetManager::Self}, "Trying to unload Texture2D that doesn't exist, name '{}',  (RuntimeID: {})", descriptor.m_Name, descriptor.GetRuntimeID());
			return;
		}
		s_Cache->m_Texture2DCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded Texture2D '{}' (RuntimeID: {}).", descriptor.m_Name, descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadSpriteAtlas(const SpriteAtlasDescriptor& descriptor) {
		if (!s_Cache->m_SpriteAtlasCache.contains(descriptor.GetRuntimeID())) {
			CORI_CORE_WARN_TAGGED({Logger::Tags::AssetManager::Self}, "Trying to unload SpriteAtlas that doesn't exist, name '{}', (RuntimeID: {})", descriptor.m_Name, descriptor.GetRuntimeID());
			return;
		}
		s_Cache->m_SpriteAtlasCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded SpriteAtlas '{}' (RuntimeID: {}).", descriptor.m_Name, descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadSound(const SoundDescriptor& descriptor) {
		if (!s_Cache->m_SoundCache.contains(descriptor.GetRuntimeID())) {
			CORI_CORE_WARN_TAGGED({Logger::Tags::AssetManager::Self}, "Trying to unload Sound that doesn't exist, name '{}', (RuntimeID: {})", descriptor.m_Name, descriptor.GetRuntimeID());
			return;
		}
		s_Cache->m_SpriteAtlasCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded Sound '{}' (RuntimeID: {}).", descriptor.m_Name, descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadShaders(const std::initializer_list<ShaderProgramDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloading {} shader(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadShader(descriptor);
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded {} shader(s)", descriptors.size());
	}

	void AssetManager::UnloadTexture2Ds(const std::initializer_list<Texture2DDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloading {} Texture2D(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadTexture2D(descriptor);
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded {} Texture2D(s)", descriptors.size());
	}

	void AssetManager::UnloadSpriteAtlases(const std::initializer_list<SpriteAtlasDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloading {} SpriteAtlas(es)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadSpriteAtlas(descriptor);
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded {} SpriteAtlas(es)", descriptors.size());
	}

	void AssetManager::UnloadSounds(const std::initializer_list<SoundDescriptor> descriptors) {
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloading {} Sound(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadSound(descriptor);
		}
		CORI_CORE_INFO_TAGGED({Logger::Tags::AssetManager::Self}, "Unloaded {} Sound(s)", descriptors.size());
	}

	void AssetManager::ClearShaderCache() {
		s_Cache->m_ShaderCache.clear();
	}

	void AssetManager::ClearTexture2DCache() {
		s_Cache->m_Texture2DCache.clear();
	}

	void AssetManager::ClearSpriteAtlasCache() {
		s_Cache->m_SpriteAtlasCache.clear();
	}

	void AssetManager::ClearSoundCache() {
		s_Cache->m_SoundCache.clear();
	}
}
