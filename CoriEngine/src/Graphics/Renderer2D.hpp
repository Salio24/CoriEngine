#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "Texture.hpp"
#include "CameraComponent.hpp"
#include "API.hpp"
#include "Core/Application.hpp"
#include "Font.hpp"

namespace Cori {
	namespace Graphics {
		class Renderer2D {
			struct QuadInstance {
				glm::mat3 m_Transform{ 0.0f };
				glm::vec2 m_Size{ 0.0f };
				glm::vec4 m_UVs{ 0.0f };
				Texture2D* m_Texture{ nullptr };
				glm::vec4 m_TintColor{ 0.0f };
				uint8_t m_Layer{ 0 };

				QuadInstance() = default;

				QuadInstance(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, Texture2D* texture, const glm::vec4& uvs, const uint8_t layer) :
					m_Transform(transform), m_Size(size), m_UVs(uvs), m_Texture(texture), m_TintColor(tintColor), m_Layer(layer) {}
			};

			struct CharInstance {
				glm::mat3 m_Transform{ 0.0f };
				glm::vec2 m_Size{ 0.0f };
				glm::vec4 m_UVs{ 0.0f };
				Texture2D* m_Texture{ nullptr };
				glm::vec4 m_TintColor{ 0.0f };
				uint8_t m_Layer{ 0 };

				CharInstance() = default;

				CharInstance(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, Texture2D* texture, const glm::vec4& uvs, const uint8_t layer) :
					m_Transform(transform), m_Size(size), m_UVs(uvs), m_Texture(texture), m_TintColor(tintColor), m_Layer(layer) {}
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

			static void Test();

			static void BeginScene(const Components::Scene::Camera& camera);

			static void EndScene();

			static void DrawScene(Scene* scene);

			static void FlushRenderQueues();

			static void SubmitQuad(const DrawSpace space, const ObjectTransparency transparencyMode, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

			static void SubmitColoredQuad(const DrawSpace space, const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color);

			static void SubmitAABB(const Utility::AABB& aabb, const float lineThickness, const glm::vec3& color);

			static void SubmitText(const DrawSpace space, const glm::mat3& transform, const std::string& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const float lineSpacing, const float kerning);

			static Statistics GetStatistics();

			// void DrawCircle(...);
			// void DrawLine(...);

			static void BeginInstancedSet();
			static void EndInstancedSet();

		protected:
			friend API;
			static void Init();
			static void Shutdown();

		private:
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
				glm::vec2 m_Size{ 0.0f };
				glm::vec4 m_TintColor{ 0.0f };
				float m_Layer{ 0 };

				Char() = default;

				Char(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, const glm::vec4& uvs, const float layer) :
					m_Transform(transform), m_TexturePosition(uvs), m_Size(size), m_TintColor(tintColor), m_Layer(layer) {}
			};

			struct RendererData {
				// generic
				static constexpr uint32_t MaxInstanceCount{ 6144 };

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

				// quad specific

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

				// text specific

				std::shared_ptr<VertexArray> CharInstanceVertexArray;
				std::shared_ptr<VertexBuffer> CharInstanceVertexBuffer;
				std::shared_ptr<IndexBuffer> CharInstanceIndexBuffer;
				std::shared_ptr<ShaderProgram> CharInstanceShader;

				uint32_t CharInstanceCount{ 0 };
				Char* CharInstanceBufferBase{ nullptr };
				Char* CharInstanceBufferPtr{ nullptr };

				static constexpr uint32_t WorldSpaceTransparentCharQueueInitialSize{ 96 };
				static constexpr uint32_t WorldSpaceOpaqueCharQueueInitialSize{ 256 };
				std::vector<CharInstance> WorldSpaceTransparentCharQueue;
				std::vector<CharInstance> WorldSpaceOpaqueCharQueue;

				static constexpr uint32_t ScreenSpaceTransparentCharQueueInitialSize{ 256 };
				static constexpr uint32_t ScreenSpaceOpaqueCharQueueInitialSize{ 2048 };
				std::vector<CharInstance> ScreenSpaceTransparentCharQueue;
				std::vector<CharInstance> ScreenSpaceOpaqueCharQueue;

				// test
				std::shared_ptr<Font> TestFont;

			};

			static RendererData* s_Data;

			static void SubmitQuadToQueue(std::vector<QuadInstance>& queue, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

			static void SubmitTextToQueue(std::vector<CharInstance>& queue, const glm::mat3& transform, const std::string& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const float lineSpacing, const float kerning);

			static void BeginWorldSpacePass();

			static void BeginScreenSpacePass();

			static void DrawQuadInstanced(const QuadInstance& quad);

			static void DrawCharInstanced(const CharInstance& instance);

			static void StartNewInstancedSet();

			static void FlushOpaqueQueues();

			static void FlushTransparentQueues();

			static void FlushInstancedQuads();

			static void FlushInstancedChars();
		};
	}
}
