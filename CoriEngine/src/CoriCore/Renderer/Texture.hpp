#pragma once
#include "../AssetManager/Texture2DDescriptor.hpp"

namespace Cori {
	struct UVs {
		glm::vec2 UVmin{ 0.0f, 0.0f };
		glm::vec2 UVmax{ 1.0f, 1.0f };
	};

	class Texture {
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;
	};

	class Texture2D : public Texture {
	public:

		static std::shared_ptr<Texture2D> Create(const Texture2DDescriptor& descriptor);

	};
}