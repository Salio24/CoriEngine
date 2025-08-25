#pragma once
#include "ShaderDescriptor.hpp"
#include "Texture2DDescriptor.hpp"
#include "Graphics/ShaderProgram.hpp"
#include "Graphics/Texture.hpp"
#include "Graphics/SpriteAtlas.hpp"
#include "SpriteAtlasDescriptor.hpp"
#include "Audio/Sound.hpp"
#include "SoundDescriptor.hpp"

namespace Cori {

	class AssetManager {
	public:
		static void Init();

		static void Shutdown();

		[[nodiscard]] static std::shared_ptr<ShaderProgram> GetShader(const ShaderProgramDescriptor& descriptor);
		[[nodiscard]] static std::shared_ptr<Texture2D> GetTexture2D(const Texture2DDescriptor& descriptor);
		[[nodiscard]] static std::expected<std::shared_ptr<SpriteAtlas>, CoriError<>> GetSpriteAtlas(const SpriteAtlasDescriptor& descriptor);
		[[nodiscard]] static std::shared_ptr<Audio::Sound> GetSound(const SoundDescriptor& descriptor);

		static void PreloadShaders(const std::initializer_list<ShaderProgramDescriptor> descriptors);
		static void PreloadTexture2Ds(const std::initializer_list<Texture2DDescriptor> descriptors);
		static void PreloadSpriteAtlases(const std::initializer_list<SpriteAtlasDescriptor> descriptors);
		static void PreloadSounds(const std::initializer_list<SoundDescriptor> descriptors);

		static void UnloadShader(const ShaderProgramDescriptor& descriptor);
		static void UnloadTexture2D(const Texture2DDescriptor& descriptor);
		static void UnloadSpriteAtlas(const SpriteAtlasDescriptor& descriptor);
		static void UnloadSound(const SoundDescriptor& descriptor);

		static void UnloadShaders(const std::initializer_list<ShaderProgramDescriptor> descriptors);
		static void UnloadTexture2Ds(const std::initializer_list<Texture2DDescriptor> descriptors);
		static void UnloadSpriteAtlases(const std::initializer_list<SpriteAtlasDescriptor> descriptors);
		static void UnloadSounds(const std::initializer_list<SoundDescriptor> descriptors);

		static void ClearShaderCache();
		static void ClearTexture2DCache();
		static void ClearSpriteAtlasCache();
		static void ClearSoundCache();
	private:
		struct Cache;
		static Cache* s_Cache;
	};
}
