#pragma once
#include "SceneSystem/Entity.hpp"
#include "Renderer/Texture.hpp"
#include "Core/Utility/StringHash.hpp"

namespace Cori {
	class Scene;

	namespace Test {
		class Renderer2D;
	}

	namespace Graphics {
		struct QuadPrimitive;

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
				AnimatorBound = 1 << 5,
			};

			glm::vec2 m_WorldOrigin;
			glm::vec2 m_LocalPosition;
			glm::vec2 m_Size;
			glm::vec4 m_TintColor;
			Entity m_Owner;
			float m_Rotation;
			uint8_t m_Layer;
			//uint32_t m_UID;
			Utils::StringHash m_ID;

			void SetTexture(const std::shared_ptr<Texture2D>& tex) {
				if ((m_States & AnimatorBound) != AnimatorBound) {
					if (!m_States & SemiTransparent) {
						SetSemiTransparency(tex->HasSemiTransparency());
					}
					m_Texture = tex;
				} else {
					CORI_CORE_WARN("Can't change a texture of a QuadPrimitive because an Animator is currently bound to that primitive.");
				}
			}

			void SetSemiTransparency(const bool b) {
				if (b) {
					m_States = m_States | SemiTransparent;
				} else {
					m_States = m_States & ~SemiTransparent;
				}
			}

			bool IsSemiTransparent() const {
				return m_States & SemiTransparent;
			}

			void SetVisibility(const bool b) {
				if (b) {
					m_States = m_States | Visible;
				} else {
					m_States = m_States & ~Visible;
				}
			}

			bool IsVisible() const {
				return m_States & Visible;
			}

			void SetValidity(const bool b) {
				if (b) {
					m_States = m_States | Valid;
				} else {
					m_States = m_States & ~Valid;
				}
			}

			bool IsValid() const {
				return m_States & Valid;
			}

			void SetFlipped(const bool b) {
				if (b) {
					m_States = m_States | Flipped;
				} else {
					m_States = m_States & ~Flipped;
				}
			}

			bool IsFlipped() const {
				return m_States & Flipped;
			}

			void SetFlatColored(const bool b) {
				if (b) {
					m_States = m_States | FlatColored;
				} else {
					m_States = m_States & ~FlatColored;
				}
			}

			bool IsFlatColored() const {
				return m_States & FlatColored;
			}

			void SetUVs(const UVs& uvs) {
				if ((m_States & AnimatorBound) != AnimatorBound) {
					m_UVs = uvs;
				} else {
					CORI_CORE_WARN("Can't change UVs of a QuadPrimitive because an Animator is currently bound to that primitive.");
				}
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

			template<typename Primitive> requires Utils::OneOf<Primitive, QuadPrimitive>
			friend class PrimitivePool;

			UVs m_UVs{};
			uint8_t m_States{0};

			std::shared_ptr<Texture2D> m_Texture{nullptr};

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