#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "Texture.hpp"
#include "CameraComponent.hpp"
#include "API.hpp"
#include "Core/Application.hpp"
#include "Font.hpp"
#include "Utility/UTF.hpp"

#ifndef CORI_SPACES_PER_TAB
	#define CORI_SPACES_PER_TAB 4
#endif

namespace Cori {
	namespace Graphics {
		/**
		 * @brief The main engine renderer.
		 * @details The render is capable of rendering 2D quads, and text for now. It can render in screen space or in world space,
		 * supports both opaque object and transparent objects, and also has a layering system to position opaque and transparent objects currently (need to finish it of with a k-way merge).
		 * When rendering text it can be aligned to the left, right or center. Render uses instancing.
		 */
		class Renderer2D {
		public:
			/**
			 * @brief Available text alignment options.
			 */
			enum TextAlignment : uint8_t {
				RIGHT,
				CENTER,
				LEFT
			};
		private:
			struct QuadInstance {
				glm::mat3 m_Transform{ 0.0f };
				glm::vec2 m_Size{ 0.0f };
				uint8_t m_Depth{ 0 };
				Texture2D* m_Texture{ nullptr };
				glm::vec4 m_UVs{ 0.0f };
				glm::vec4 m_TintColor{ 0.0f };

				QuadInstance() = default;

				QuadInstance(const glm::mat3& transform, const glm::vec2 size, const glm::vec4& tintColor, Texture2D* texture, const glm::vec4& uvs, const uint8_t layer) :
					m_Transform(transform), m_Size(size), m_Depth(layer), m_Texture(texture), m_UVs(uvs), m_TintColor(tintColor) {}
			};

			struct TextInstance {
				glm::mat3 m_Transform{ 0.0f };
				float m_FontSize{ 0.0f };
				std::u32string m_Text;
				Font* m_Font{ nullptr };
				glm::vec4 m_Color{ 0.0f };
				float m_LineSpacing{ 0.0f };
				float m_Kerning{ 0.0f };
				float m_LimitX{ -1.0f };
				uint8_t m_Depth{ 0 };
				TextAlignment m_Alignment{};

				TextInstance() = default;

				TextInstance(const TextAlignment alignment, const glm::mat3& transform, const float fontSize,const std::u32string_view& text, const glm::vec4& color, Font* font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) :
					m_Transform(transform), m_FontSize(fontSize), m_Text(text), m_Font(font), m_Color(color), m_LineSpacing(lineSpacing), m_Kerning(kerning), m_LimitX(limitX), m_Depth(depth), m_Alignment(alignment) {}

				TextInstance(const TextAlignment alignment, const glm::mat3& transform, const float fontSize,const std::string_view& text, const glm::vec4& color, Font* font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) :
					m_Transform(transform), m_FontSize(fontSize), m_Text(Utility::Utf8ToUtf32(text)), m_Font(font), m_Color(color), m_LineSpacing(lineSpacing), m_Kerning(kerning), m_LimitX(limitX), m_Depth(depth), m_Alignment(alignment) {}
			};

		public:
			struct Statistics {
				uint32_t DrawCalls{ 0 };
				uint32_t QuadCount{ 0 };
				uint32_t CharCount{ 0 };
			};

			/**
			 * @brief Defines where to draw the object, in what space.
			 * @details Basically if WORLD_SPACE is selected it will use the position, rotation, scale applied to orthographic camera,
			 * and if SCREEN_SPACE it will ignore the camera position, rotation, scale and use the initial projection matrix.
			 * @note Never use UNSPECIFIED it is the default value and should not be used by the user.
			 */
			enum DrawSpace : uint8_t {
				WORLD_SPACE,
				SCREEN_SPACE,
				UNSPECIFIED
			};

			/**
			 * @note When directly using the renderer you need to correctly specify if the object you are trying to render has semi transparency or no, opaque and transparent object are processed differently.
			 */
			enum ObjectTransparency : uint8_t {
				OPAQUE,
				SEMI_TRANSPARENT
			};


			/**
			 * @brief Submits the quad to the render queue.
			 * @param space DrawSpace to draw the quad.
			 * @param transparencyMode Transparency mode, defines how to handle the quad. (hint: you can query a Texture2D if it has semi transparency or no)
			 * @param transform Rendering transform. Position defined is the center of the quad.
			 * @param halfSize Half size of the quad.
			 * @param tintColor Tint color of the quad, or a color if flatColored=true.
			 * @param texture Texture to sample from.
			 * @param uvs UVs to sample with.
			 * @param depth Quad Depth, for layering, the higher the "closer".
			 * @param flipX Flip quad on X axis.
			 * @param flipY Flip quad on Y axis.
			 * @param flatColored Ignore the texture and use a plain white texture.
			 * @warning You need to make sure that texture pointer stays valid until the end of the frame, or this will induce a dangling pointer. Be aware!
			 */
			static void SubmitQuad(const DrawSpace space, const ObjectTransparency transparencyMode,
			                       const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor,
			                       Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX,
			                       const bool flipY, const bool flatColored);

			/**
			 * @brief Convenience function mainly for debugging, draws a plain colored quad.
			 * @param space DrawSpace to draw the quad.
			 * @param position Position defined is the center of the quad.
			 * @param halfSize Half size of the quad.
			 * @param color Quad color.
			 * @note Draws on the depth 255.
			 */
			static void SubmitColoredQuad(const DrawSpace space, const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color);

			/**
			 * @brief Draws the AABB, also a debug convenience function, always draws in world space.
			 * @param aabb AABB to draw.
			 * @param lineThickness Thickness of the AABB border.
			 * @param color Border color.
			 */
			static void SubmitAABB(const Utility::AABB& aabb, const float lineThickness, const glm::vec3& color);

			/**
			 * @brief Draws a UTF-32 fixed length encoded string.
			 * @param space DrawSpace to draw text in.
			 * @param alignment Text alignment with which to draw the text.
			 * @param transform Rendering transform.
			 * @param fontSize Size of the font to render.
			 * @param text View to the UTF-32 fixed length encoded string to render.
			 * @param color Color of the text to render.
			 * @param font Font to use when rendering the text.
			 * @param depth Depth at which to render the text.
			 * @param limitX Length limit of the one line.
			 * @param lineSpacing Additional line spacing.
			 * @param kerning Additional kerning.
			 * @details Transform for TextAlignment::LEFT is a lower left border of the char in the first line.
			 * \n For TextAlignment::CENTER it's the lower bound on y of the first line, and on x the center between the left border of the char in the first line and the right border of the char in the first line.
			 * \n For TextAlignment::RIGHT it's the left lower border of the last char in the line.
			 * @note All text is considered semi transparent when rendering.
			 * @warning You need to make sure that font pointer stays valid until the end of the frame, or this will induce a dangling pointer. Be aware!
			 */
			static void SubmitText(const DrawSpace space, const TextAlignment alignment, const glm::mat3& transform,
			                       const float fontSize, const std::u32string_view& text, const glm::vec4& color,
			                       Font* font, const uint8_t depth, const float limitX, const float lineSpacing,
			                       const float kerning);

			/**
			 * @brief Draws a UTF-32 fixed length encoded string.
			 * @param space DrawSpace to draw text in.
			 * @param alignment Text alignment with which to draw the text.
			 * @param transform Rendering transform.
			 * @param fontSize Size of the font to render.
			 * @param text View to the UTF-8 variable length encoded string to render.
			 * @param color Color of the text to render.
			 * @param font Font to use when rendering the text.
			 * @param depth Depth at which to render the text.
			 * @param limitX Length limit of the one line.
			 * @param lineSpacing Additional line spacing.
			 * @param kerning Additional kerning.
			 * @details Transform for TextAlignment::LEFT is a lower left border of the char in the first line.
			 * \n For TextAlignment::CENTER it's the lower bound on y of the first line, and on x the center between the left border of the char in the first line and the right border of the char in the first line.
			 * \n For TextAlignment::RIGHT it's the left lower border of the last char in the line.
			 * @note All text is considered semi transparent when rendering.
			 * @warning You need to make sure that font pointer stays valid until the end of the frame, or this will induce a dangling pointer. Be aware!
			 */
			static void SubmitText(const DrawSpace space, const TextAlignment alignment, const glm::mat3& transform,
			                       const float fontSize, const std::string_view& text, const glm::vec4& color,
			                       Font* font, const uint8_t depth, const float limitX, const float lineSpacing,
			                       const float kerning);

			/**
			 * @brief Gives you the rendering stats of the last rendered frame.
			 * @return Last frame stats.
			 */
			static Statistics GetStatistics();

			// void DrawCircle(...);
			// void DrawLine(...);
			static void StartFrame();
			static void EndFrame();

			static void BeginScene(const World::Components::Scene::Camera& camera);
			static void EndScene();

		private:
			friend World::Scene;
			friend Internal::API;

			static void SubmitScene(World::Scene* scene);
			static void Init();
			static void Shutdown();

			struct Quad {
				glm::mat3 m_Transform{ 0.0f };
				glm::vec4 m_TexturePosition{ 0.0f };
				glm::vec2 m_Size{ 0.0f };
				glm::vec4 m_TintColor{ 0.0f };
				float m_Layer{ 0 };

				Quad() = default;

				Quad(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, const glm::vec4& uvs, const float layer) :
					m_Transform(transform), m_TexturePosition(uvs), m_Size(size), m_TintColor(tintColor), m_Layer(layer) {}
			};

			struct Char {
				glm::mat3 m_Transform{ 0.0f };
				glm::vec4 m_TexturePosition{ 0.0f };
				glm::vec4 m_CharQuad{ 0.0f };
				glm::vec4 m_Color{ 0.0f };
				float m_Layer{ 0 };

				Char() = default;

				Char(const glm::mat3& transform, const glm::vec4& charQuad, const glm::vec4& color, const glm::vec4& uvs, const float layer) :
					m_Transform(transform), m_TexturePosition(uvs), m_CharQuad(charQuad), m_Color(color), m_Layer(layer) {}
			};

			struct RendererData {
				// generic
				static constexpr uint32_t MaxInstanceCount{ 6144 };
				static constexpr uint32_t MaxCharInstanceCount{ 16384 };

				Texture2D* CurrentTexture{nullptr};
				VertexArray* CurrentVertexArray{nullptr};
				VertexBuffer* CurrentVertexBuffer{nullptr};
				IndexBuffer* CurrentIndexBuffer{nullptr};
				ShaderProgram* CurrentShader{nullptr};
				DrawSpace CurrentDrawSpace{ UNSPECIFIED };

				Statistics Stats;

				std::shared_ptr<Texture2D> WhiteTexture;
				Texture2D* NecessaryTexture{nullptr};

				glm::mat4 CurrentViewProjectionMatrix{ 1.0f };
				glm::mat4 WorldSpaceViewProjectionMatrix{ 1.0f };
				glm::mat4 ScreenSpaceViewProjectionMatrix{ 1.0f };

				// vvv quad specific

				std::shared_ptr<VertexArray> QuadInstanceVertexArray;
				std::shared_ptr<VertexBuffer> QuadInstanceVertexBuffer;
				std::shared_ptr<IndexBuffer> QuadInstanceIndexBuffer;
				std::shared_ptr<ShaderProgram> QuadInstanceShader;

				uint32_t QuadInstanceCount{ 0 };
				Quad* QuadInstanceBufferBase{nullptr};
				Quad* QuadInstanceBufferPtr{nullptr};

				static constexpr uint32_t WorldSpaceTransparentQuadQueueInitialSize{ 1024 };
				static constexpr uint32_t WorldSpaceOpaqueQuadQueueInitialSize{ 6144 };
				std::vector<QuadInstance> WorldSpaceTransparentQuadQueue;
				std::vector<QuadInstance> WorldSpaceOpaqueQuadQueue;

				static constexpr uint32_t ScreenSpaceTransparentQuadQueueInitialSize{ 128 };
				static constexpr uint32_t ScreenSpaceOpaqueQuadQueueInitialSize{ 128 };
				std::vector<QuadInstance> ScreenSpaceTransparentQuadQueue;
				std::vector<QuadInstance> ScreenSpaceOpaqueQuadQueue;

				// vvv text specific

				std::shared_ptr<VertexArray> CharInstanceVertexArray;
				std::shared_ptr<VertexBuffer> CharInstanceVertexBuffer;
				std::shared_ptr<IndexBuffer> CharInstanceIndexBuffer;
				std::shared_ptr<ShaderProgram> CharInstanceShader;

				uint32_t CharInstanceCount{ 0 };
				Char* CharInstanceBufferBase{ nullptr };
				Char* CharInstanceBufferPtr{ nullptr };

				static constexpr uint32_t WorldSpaceTransparentTextQueueInitialSize{ 96 };
				std::vector<TextInstance> WorldSpaceTransparentTextQueue;

				static constexpr uint32_t ScreenSpaceTransparentTextQueueInitialSize{ 96 };
				std::vector<TextInstance> ScreenSpaceTransparentTextQueue;
			};

			enum TransparentQueueTypes : uint8_t {
				WORLD_SPACE_QUAD,
				SCREEN_SPACE_QUAD,
				WORLD_SPACE_TEXT,
				SCREEN_SPACE_TEXT
			};

			using ElementVariantPtr = std::variant<const QuadInstance*, const TextInstance*>;

			struct ElementView {
				ElementVariantPtr m_ElementVariant;
				uint8_t m_Depth;
				TransparentQueueTypes m_QueueType;

				bool operator>(const ElementView& other) const {
					return m_Depth > other.m_Depth;
				}
			};

			static RendererData* s_Data;

			static void FlushRenderQueues();

			static void BeginQuadInstancedSet();
			static void EndQuadInstancedSet();

			static void BeginCharInstancedSet();
			static void EndCharInstancedSet(Texture2D* atlas, const glm::mat3& modelMatrix);

			static void SubmitQuadToQueue(std::vector<QuadInstance>& queue, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

			static void SubmitTextToQueue(std::vector<TextInstance>& queue, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::u32string_view& text, const glm::vec4& color, Font* font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning);

			static void SubmitTextToQueue(std::vector<TextInstance>& queue, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::string_view& text, const glm::vec4& color, Font* font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning);

			static bool BeginWorldSpacePass();

			static bool BeginScreenSpacePass();

			static void DrawQuadInstanced(const QuadInstance& quad);

			static void DrawTextInstanced(const TextInstance& text);

			static void StartNewQuadInstancedSet();

			static void FlushOpaqueQueues();

			static void FlushTransparentQueues();

			static void FlushInstancedQuads();

			static void FlushInstancedChars(Texture2D* atlas, const glm::mat3& modelMatrix);
		};
	}
}
