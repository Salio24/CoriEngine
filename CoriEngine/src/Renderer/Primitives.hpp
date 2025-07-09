#pragma once
#include "SceneSystem/Entity.hpp"
#include "Renderer/Texture.hpp"

namespace Cori {
	class Scene;

	namespace Test {
		class Renderer2D;
	}

	namespace Graphics {
		struct QuadPrimitive;

		template<typename Primitive> requires Utils::OneOf<Primitive, QuadPrimitive>
		class PrimitivePool;

		enum PrimitiveType : uint8_t {
			Quad = 1 << 0,
			Circle = 1 << 1,
			Text = 1 << 2
		};

		struct PrimitiveHandle {
			uint32_t index;
			PrimitiveType type;
		};

		struct QuadPrimitive {
			enum Options : uint8_t {
				SemiTransparent = 1 << 0,
				Visible = 1 << 1,
				Valid = 1 << 2,
				Flipped = 1 << 3,
				FlatColored = 1 << 4,
			};

			glm::vec2 worldOrigin;
			glm::vec2 localPosition;
			glm::vec2 size;
			glm::vec4 tintColor;
			Entity owner;
			UVs uvs{};
			float rotation;
			uint8_t layer;

			void SetTexture(const std::shared_ptr<Texture2D>& tex) {
				if (!states & SemiTransparent) {
					SetSemiTransparency(tex->HasSemiTransparency());
				}
				texture = tex;
			}

			void SetSemiTransparency(const bool b) {
				if (b) {
					states = states | SemiTransparent;
				} else {
					states = states & ~SemiTransparent;
				}
			}

			bool IsSemiTransparent() const {
				return states & SemiTransparent;
			}

			void SetVisibility(const bool b) {
				if (b) {
					states = states | Visible;
				} else {
					states = states & ~Visible;
				}
			}

			bool IsVisible() const {
				return states & Visible;
			}

			void SetValidity(const bool b) {
				if (b) {
					states = states | Valid;
				} else {
					states = states & ~Valid;
				}
			}

			bool IsValid() const {
				return states & Valid;
			}

			void SetFlipped(const bool b) {
				if (b) {
					states = states | Flipped;
				} else {
					states = states & ~Flipped;
				}
			}

			bool IsFlipped() const {
				return states & Flipped;
			}

			void SetFlatColored(const bool b) {
				if (b) {
					states = states | FlatColored;
				} else {
					states = states & ~FlatColored;
				}
			}

			bool IsFlatColored() const {
				return states & FlatColored;
			}

			struct Descriptor {
				glm::vec2 localPosition;
				glm::vec2 size;
				glm::vec4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f};
				UVs uvs{};
				std::shared_ptr<Texture2D> texture{nullptr};
				float rotation{ 0 };
				uint8_t layer;
			};
			protected:
			friend class Cori::Scene;
			friend class Cori::Test::Renderer2D;
			friend class PrimitivePool<QuadPrimitive>;

			uint8_t states{0};


			std::shared_ptr<Texture2D> texture{nullptr};

		};

		template<typename T>
		struct PrimitiveTypeTraits;

		template<>
		struct PrimitiveTypeTraits<QuadPrimitive> {
			static constexpr PrimitiveType type = PrimitiveType::Quad;
			static constexpr const char* name = "Quad";
		};
		// add the rest of typetraits later
	}
}