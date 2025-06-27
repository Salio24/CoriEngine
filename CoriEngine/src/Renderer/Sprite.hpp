#pragma once
#include "Texture.hpp"
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"

namespace Cori {
	class Sprite : public Profiling::Trackable<Sprite>, public SharedSeflFactory<Sprite> {
	public:
		static bool PreCreateHook(const std::shared_ptr<Texture2D>& texture, const UVs& uv);

		const std::shared_ptr<Texture2D> GetTexture() const { return m_Texture; }
		const UVs GetUVs() const { return m_UVs; }

	protected: 
		Sprite(const std::shared_ptr<Texture2D>& texture, const UVs& uv); 
	private:

		std::shared_ptr<Texture2D> m_Texture;
		UVs m_UVs;
	};

}