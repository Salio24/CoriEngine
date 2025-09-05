#pragma once
#include "Image.hpp"

namespace Cori {
	struct UVs {
		glm::vec2 UVmin{ 0.0f, 0.0f };
		glm::vec2 UVmax{ 1.0f, 1.0f };

		explicit operator glm::vec4() const { return { UVmin, UVmax }; }
	};

	class Texture {
	public:
		enum PixelFormat {
			RGBA8888, RGB888
		};

		enum WrapMode {
			CLAMP_TO_EDGE, CLAMP_TO_BORDER, REPEAT
		};

		enum Filter {
			LINEAR, NEAREST
		};

		struct Params {
			PixelFormat m_PixelFormat{ RGBA8888 };
			WrapMode m_WrapMode{ CLAMP_TO_EDGE };
			Filter m_Filter{ NEAREST };
			int32_t m_UnpackAlignment{ 0 };
			bool m_HasSemiTransparency{ false };

		};

		virtual ~Texture() = default;

		[[nodiscard]] virtual uint32_t GetWidth() const = 0;
		[[nodiscard]] virtual uint32_t GetHeight() const = 0;

		[[nodiscard]] virtual bool HasSemiTransparency() const = 0;

		virtual void Bind(uint32_t slot) const = 0;
	};

	class Texture2D : public Texture {
	public:

		static std::shared_ptr<Texture2D> Create(const std::shared_ptr<Image>& image);

		static std::shared_ptr<Texture2D> Create(const void* pixelData, const uint32_t width, const uint32_t height, const Params& params);

	};
}