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
		class Renderer2D {
		public:
			enum TextAlignment : uint8_t {
				RIGHT,
				CENTER,
				LEFT
			};
		private:
			struct QuadInstance {
				glm::mat3 m_Transform{ 0.0f };
				glm::vec2 m_Size{ 0.0f };
				uint8_t m_Layer{ 0 };
				std::shared_ptr<Texture2D> m_Texture{ nullptr };
				glm::vec4 m_UVs{ 0.0f };
				glm::vec4 m_TintColor{ 0.0f };

				QuadInstance() = default;

				QuadInstance(const glm::mat3& transform, const glm::vec2 size, const glm::vec4& tintColor, const std::shared_ptr<Texture2D>& texture, const glm::vec4& uvs, const uint8_t layer) :
					m_Transform(transform), m_Size(size), m_Layer(layer), m_Texture(texture), m_UVs(uvs), m_TintColor(tintColor) {}
			};

			struct TextInstance {
				glm::mat3 m_Transform{ 0.0f };
				float m_FontSize{ 0.0f };
				std::u32string m_Text;
				std::shared_ptr<Font> m_Font{ nullptr };
				glm::vec4 m_Color{ 0.0f };
				float m_LineSpacing{ 0.0f };
				float m_Kerning{ 0.0f };
				float m_LimitX{ -1.0f };
				uint8_t m_Depth{ 0 };
				TextAlignment m_Alignment{};

				TextInstance() = default;

				TextInstance(const TextAlignment alignment, const glm::mat3& transform, const float fontSize,const std::u32string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) :
					m_Transform(transform), m_FontSize(fontSize), m_Text(text), m_Font(font), m_Color(color), m_LineSpacing(lineSpacing), m_Kerning(kerning), m_LimitX(limitX), m_Depth(depth), m_Alignment(alignment) {}

				TextInstance(const TextAlignment alignment, const glm::mat3& transform, const float fontSize,const std::string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) :
					m_Transform(transform), m_FontSize(fontSize), m_Text(Utility::Utf8ToUtf32(text)), m_Font(font), m_Color(color), m_LineSpacing(lineSpacing), m_Kerning(kerning), m_LimitX(limitX), m_Depth(depth), m_Alignment(alignment) {}
			};

		public:
			struct Statistics {
				uint32_t DrawCalls{ 0 };
				uint32_t QuadCount{ 0 };
				uint32_t CharCount{ 0 };
			};

			enum DrawSpace : uint8_t {
				WORLD_SPACE,
				SCREEN_SPACE
			};

			enum ObjectTransparency : uint8_t {
				OPAQUE,
				SEMI_TRANSPARENT
			};

			static void BeginScene(const World::Components::Scene::Camera& camera);

			static void EndScene();

			static void DrawScene(World::Scene* scene);

			static void FlushRenderQueues();

			static void SubmitQuad(const DrawSpace space, const ObjectTransparency transparencyMode, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, const std::shared_ptr<Texture2D>& texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

			static void SubmitColoredQuad(const DrawSpace space, const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color);

			static void SubmitAABB(const Utility::AABB& aabb, const float lineThickness, const glm::vec3& color);

			static void SubmitText(const DrawSpace space, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::u32string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning);

			static void SubmitText(const DrawSpace space, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning);

			static Statistics GetStatistics();

			// void DrawCircle(...);
			// void DrawLine(...);

		private:
			friend API;
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
				DrawSpace CurrentDrawSpace;

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

			static RendererData* s_Data;

			static void BeginInstancedSet();
			static void EndInstancedSet();

			static void BeginCharInstancedSet();
			static void EndCharInstancedSet(Texture2D* atlas, const glm::mat3& modelMatrix);

			static void SubmitQuadToQueue(std::vector<QuadInstance>& queue, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, const std::shared_ptr<Texture2D>& texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

			static void SubmitTextToQueue(std::vector<TextInstance>& queue, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::u32string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning);

			static void SubmitTextToQueue(std::vector<TextInstance>& queue, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning);

			static void BeginWorldSpacePass();

			static void BeginScreenSpacePass();

			static void DrawQuadInstanced(const QuadInstance& quad);

			static void DrawTextInstanced(const TextInstance& text);

			static void StartNewInstancedSet();

			static void FlushOpaqueQueues();

			static void FlushTransparentQueues();

			static void FlushInstancedQuads();

			static void FlushInstancedChars(Texture2D* atlas, const glm::mat3& modelMatrix);
		};
	}
}
