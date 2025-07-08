#pragma once
#include "SceneSystem/Entity.hpp"
#include "Renderer/Texture.hpp"

namespace Cori {
	namespace Graphics {
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
			static constexpr uint8_t SemiTransparencyMask = 1 << 0;
			static constexpr uint8_t VisibilityMask = 1 << 1;
			static constexpr uint8_t ValidityMask = 1 << 2;
			static constexpr uint8_t FlippedMask = 1 << 3;

			glm::vec2 worldPosition; // add , SetWorldPos method to render component, and update this in it
			glm::vec2 localPosition;
			glm::vec2 size;
			glm::vec4 tintColor;
			Entity owner;
			UVs uvs{};
			float rotation;
			uint8_t layer;

			void SetTexture(const std::shared_ptr<Texture2D>& t) {
				if (t) {
					SetSemiTransparency(t->HasSemiTransparency());
				}
				texture = t;
			}

			void SetSemiTransparency(const bool b) {
				if (b) {
					states = states | SemiTransparencyMask;
				} else {
					states = states & ~SemiTransparencyMask;
				}
			}

			bool GetSemiTransparency() const {
				return states & SemiTransparencyMask;
			}

			void SetVisibility(const bool b) {
				if (b) {
					states = states | VisibilityMask;
				} else {
					states = states & ~VisibilityMask;
				}
			}

			bool GetVisibility() const {
				return states & VisibilityMask;
			}

			void SetValidity(const bool b) {
				if (b) {
					states = states | ValidityMask;
				} else {
					states = states & ~ValidityMask;
				}
			}

			bool GetValidity() const {
				return states & ValidityMask;
			}

			void SetFlipped(const bool b) {
				if (b) {
					states = states | FlippedMask;
				} else {
					states = states & ~FlippedMask;
				}
			}

			bool GetFlipped() const {
				return states & FlippedMask;
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

		private:
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

	}
}