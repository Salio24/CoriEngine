#include "AssetManager.hpp"
#include "Core/Application.hpp"

namespace Cori {
	namespace {
		struct Cache {
			static Cache& Get() {
				static Cache instance;
				return instance;
			}

			std::unordered_map<uint32_t, std::shared_ptr<ShaderProgram>> m_ShaderCache;
			std::unordered_map<uint32_t, std::shared_ptr<Texture2D>> m_Texture2DCache;
			std::unordered_map<uint32_t, std::shared_ptr<SpriteAtlas>> m_SpriteAtlasCache;
			std::unordered_map<uint32_t, std::shared_ptr<Audio::Sound>> m_SoundCache;
		};
	}

	void AssetManager::Init() {
		Cache::Get();
	}

	void AssetManager::Shutdown() {

	}

	std::shared_ptr<ShaderProgram> AssetManager::GetShader(const ShaderProgramDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		auto it = Cache::Get().m_ShaderCache.find(descriptor.GetRuntimeID());
		if (it != Cache::Get().m_ShaderCache.end()) {
			return it->second;
		}

		CORI_CORE_DEBUG("AssetManager: Shader cache miss for '{0}' (RuntimeID: {1}). Loading...", descriptor.GetDebugName(), descriptor.GetRuntimeID());
		try {
			std::shared_ptr<ShaderProgram> newShader = ShaderProgram::Create(descriptor);
			Cache::Get().m_ShaderCache[descriptor.GetRuntimeID()] = newShader;
			return newShader;
		}
		catch (const std::exception& e) {
			CORI_CORE_ERROR("AssetManager: Error loading shaders '{0}': {1}", descriptor.GetDebugName(), e.what());
			return nullptr;
		}
	}

	std::shared_ptr<Texture2D> AssetManager::GetTexture2D(const Texture2DDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		auto it = Cache::Get().m_Texture2DCache.find(descriptor.GetRuntimeID());
		if (it != Cache::Get().m_Texture2DCache.end()) {
			return it->second;
		}

		CORI_CORE_DEBUG("AssetManager: Texture2D cache miss for '{0}' (RuntimeID: {1}). Loading...", descriptor.GetDebugName(), descriptor.GetRuntimeID());
		try {
			std::shared_ptr<Texture2D> newTexture2D = Texture2D::Create(descriptor);
			Cache::Get().m_Texture2DCache[descriptor.GetRuntimeID()] = newTexture2D;
			return newTexture2D;
		}
		catch (const std::exception& e) {
			CORI_CORE_ERROR("AssetManager: Error loading Texture2D '{0}': {1}", descriptor.GetDebugName(), e.what());
			return nullptr;
		}
	}

	std::shared_ptr<SpriteAtlas> AssetManager::GetSpriteAtlas(const SpriteAtlasDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		auto it = Cache::Get().m_SpriteAtlasCache.find(descriptor.GetRuntimeID());
		if (it != Cache::Get().m_SpriteAtlasCache.end()) {
			return it->second;
		}

		CORI_CORE_DEBUG("AssetManager: SpriteAtlas cache miss for '{0}' (RuntimeID: {1}). Loading...", descriptor.GetDebugName(), descriptor.GetRuntimeID());
		try {
			std::shared_ptr<SpriteAtlas> newSpriteAtlas = SpriteAtlas::Create(descriptor);
			Cache::Get().m_SpriteAtlasCache[descriptor.GetRuntimeID()] = newSpriteAtlas;
			return newSpriteAtlas;
		}
		catch (const std::exception& e) {
			CORI_CORE_ERROR("AssetManager: Error loading SpriteAtlas '{0}': {1}", descriptor.GetDebugName(), e.what());
			return nullptr;
		}
	}

	std::shared_ptr<Audio::Sound> AssetManager::GetSound(const SoundDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();
		auto it = Cache::Get().m_SoundCache.find(descriptor.GetRuntimeID());
		if (it != Cache::Get().m_SoundCache.end()) {
			return it->second;
		}

		CORI_CORE_DEBUG("AssetManager: Sound cache miss for '{0}' (RuntimeID: {1}). Loading...", descriptor.m_Name, descriptor.GetRuntimeID());
		try {
			std::shared_ptr<Audio::Sound> newSound = Audio::Sound::Create(descriptor.m_Name, descriptor.m_Path, descriptor.m_PreDecode);
			Cache::Get().m_SoundCache[descriptor.GetRuntimeID()] = newSound;
			return newSound;
		}
		catch (const std::exception& e) {
			CORI_CORE_ERROR("AssetManager: Error loading Sound '{0}': {1}", descriptor.m_Name, e.what());
			return nullptr;
		}
	}

	void AssetManager::PreloadShaders(const std::initializer_list<ShaderProgramDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Preloading {0} shader(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			GetShader(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Preloaded {0} shader(s)", descriptors.size());
	}

	void AssetManager::PreloadTexture2Ds(const std::initializer_list<Texture2DDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Preloading {0} Texture2D(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			GetTexture2D(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Preloaded {0} Texture2D(s)", descriptors.size());
	}

	void AssetManager::PreloadSpriteAtlases(const std::initializer_list<SpriteAtlasDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Preloading {0} SpriteAtlas(es)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			GetSpriteAtlas(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Preloaded {0} SpriteAtlas(es)", descriptors.size());
	}

	void AssetManager::PreloadSounds(const std::initializer_list<SoundDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Preloading {0} Sound(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			GetSound(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Preloaded {0} Sound(es)", descriptors.size());
	}

	std::shared_ptr<ShaderProgram> AssetManager::GetShaderOwning(const ShaderProgramDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();

		CORI_CORE_DEBUG("AssetManager: Loading Texture2D '{0}' with manual ownership (RuntimeID: {1}).", descriptor.GetDebugName(), descriptor.GetRuntimeID());
		try {
			return ShaderProgram::Create(descriptor);
		}
		catch (const std::exception& e) {
			CORI_CORE_ERROR("AssetManager: Error loading Texture2D '{0}' with manual ownership: {1}", descriptor.GetDebugName(), e.what());
			return nullptr;
		}
	}

	std::shared_ptr<Texture2D> AssetManager::GetTexture2DOwning(const Texture2DDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();

		CORI_CORE_DEBUG("AssetManager: Loading Texture2D '{0}' with manual ownership (RuntimeID: {1}).", descriptor.GetDebugName(), descriptor.GetRuntimeID());
		try {
			return Texture2D::Create(descriptor);
		}
		catch (const std::exception& e) {
			CORI_CORE_ERROR("AssetManager: Error loading Texture2D '{0}' with manual ownership: {1}", descriptor.GetDebugName(), e.what());
			return nullptr;
		}
	}

	std::shared_ptr<SpriteAtlas> AssetManager::GetSpriteAtlasOwning(const SpriteAtlasDescriptor& descriptor) {
		CORI_PROFILE_FUNCTION();

		CORI_CORE_DEBUG("AssetManager: Loading SpriteAtlas '{0}' with manual ownership (RuntimeID: {1}).", descriptor.GetDebugName(), descriptor.GetRuntimeID());
		try {
			return SpriteAtlas::Create(descriptor);
		}
		catch (const std::exception& e) {
			CORI_CORE_ERROR("AssetManager: Error loading SpriteAtlas '{0}' with manual ownership: {1}", descriptor.GetDebugName(), e.what());
			return nullptr;
		}
	}

	void AssetManager::UnloadShader(const ShaderProgramDescriptor& descriptor) {
		if (CORI_CORE_ASSERT_WARN(Cache::Get().m_ShaderCache.contains(descriptor.GetRuntimeID()), "Trying to unload a Shader that doesn't exist, name '{0}',  (RuntimeID: {1})", descriptor.GetDebugName(), descriptor.GetRuntimeID())) { return; }
		Cache::Get().m_ShaderCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG("AssetManager: Unloaded Shader '{0}' (RuntimeID: {1}).", descriptor.GetDebugName(), descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadTexture2D(const Texture2DDescriptor& descriptor) {
		if (CORI_CORE_ASSERT_WARN(Cache::Get().m_Texture2DCache.contains(descriptor.GetRuntimeID()), "Trying to unload a Texture2D that doesn't exist, name '{0}',  (RuntimeID: {1})", descriptor.GetDebugName(), descriptor.GetRuntimeID())) { return; }
		Cache::Get().m_Texture2DCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG("AssetManager: Unloaded Texture2D '{0}' (RuntimeID: {1}).", descriptor.GetDebugName(), descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadSpriteAtlas(const SpriteAtlasDescriptor& descriptor) {
		if (CORI_CORE_ASSERT_WARN(Cache::Get().m_SpriteAtlasCache.contains(descriptor.GetRuntimeID()), "Trying to unload a SpriteAtlas that doesn't exist, name '{0}',  (RuntimeID: {1})", descriptor.GetDebugName(), descriptor.GetRuntimeID())) { return; }
		Cache::Get().m_SpriteAtlasCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG("AssetManager: Unloaded SpriteAtlas '{0}' (RuntimeID: {1}).", descriptor.GetDebugName(), descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadSound(const SoundDescriptor& descriptor) {
		if (CORI_CORE_ASSERT_WARN(Cache::Get().m_SpriteAtlasCache.contains(descriptor.GetRuntimeID()), "Trying to unload a SpriteAtlas that doesn't exist, name '{0}',  (RuntimeID: {1})", descriptor.m_Name, descriptor.GetRuntimeID())) { return; }
		Cache::Get().m_SpriteAtlasCache.erase(descriptor.GetRuntimeID());
		CORI_CORE_DEBUG("AssetManager: Unloaded Sound '{0}' (RuntimeID: {1}).", descriptor.m_Name, descriptor.GetRuntimeID());
	}

	void AssetManager::UnloadShaders(const std::initializer_list<ShaderProgramDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Unloading {0} shader(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadShader(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Unloaded {0} shader(s)", descriptors.size());
	}

	void AssetManager::UnloadTexture2Ds(const std::initializer_list<Texture2DDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Unloading {0} Texture2D(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadTexture2D(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Unloaded {0} Texture2D(s)", descriptors.size());
	}

	void AssetManager::UnloadSpriteAtlases(const std::initializer_list<SpriteAtlasDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Unloading {0} SpriteAtlas(es)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadSpriteAtlas(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Unloaded {0} SpriteAtlas(es)", descriptors.size());
	}

	void AssetManager::UnloadSounds(const std::initializer_list<SoundDescriptor> descriptors) {
		CORI_CORE_INFO("AssetManager: Unloading {0} Sound(s)", descriptors.size());
		for (const auto& descriptor : descriptors) {
			UnloadSound(descriptor);
		}
		CORI_CORE_INFO("AssetManager: Unloaded {0} Sound(s)", descriptors.size());
	}

	void AssetManager::ClearShaderCache() {
		Cache::Get().m_ShaderCache.clear();
	}

	void AssetManager::ClearTexture2DCache() {
		Cache::Get().m_Texture2DCache.clear();
	}

	void AssetManager::ClearSpriteAtlasCache() {
		Cache::Get().m_SpriteAtlasCache.clear();
	}

	void AssetManager::ClearSoundCache() {
		Cache::Get().m_SoundCache.clear();
	}
}