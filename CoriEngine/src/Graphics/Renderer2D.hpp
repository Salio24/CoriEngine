#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "Texture.hpp"
#include "CameraComponent.hpp"
#include "API.hpp"
#include "Core/Application.hpp"

namespace Cori {
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

	public:
		struct Statistics {
			uint32_t DrawCalls{ 0 };
			uint32_t QuadCount{ 0 };
		};

		static void BeginScene(const Components::Scene::Camera& camera);

		static void BeginWorldPass();

		static void BeginScreenSpacePass();

		static void EndScene();

		static void DrawScene(Scene* scene);

		static void FlushRenderQueues();

		static void SubmitWorldSpaceTransparentQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

		static void SubmitWorldSpaceOpaqueQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

		static void SubmitScreenSpaceTransparentQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

		static void SubmitScreenSpaceOpaqueQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored);

		static void SubmitScreenSpaceColoredQuad(const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color);

		static void SubmitWorldSpaceColoredQuad(const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color);

		static void SubmitAABB(const Utility::AABB& aabb, const float lineThickness, const glm::vec3& color);

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

		struct RendererData {
			static constexpr uint32_t TransparentQuadQueueSize{ 1024 };

			static constexpr uint32_t MaxInstanceCount{ 6144 };

			std::shared_ptr<VertexArray> QuadInstanceVertexArray;
			std::shared_ptr<VertexBuffer> QuadInstanceVertexBuffer;
			std::shared_ptr<IndexBuffer> QuadInstanceIndexBuffer;
			std::shared_ptr<ShaderProgram> QuadInstanceShader;

			std::shared_ptr<VertexArray> LineVertexArray;
			std::shared_ptr<VertexBuffer> LineVertexBuffer;
			std::shared_ptr<ShaderProgram> LineShader;

			uint32_t QuadInstanceCount{ 0 };
			Quad* QuadInstanceBufferBase{nullptr};
			Quad* QuadInstanceBufferPtr{nullptr};

			std::shared_ptr<Texture2D> WhiteTexture;
			Texture2D* NecessaryTexture{nullptr};

			Texture2D* CurrentTexture{nullptr};
			VertexArray* CurrentVertexArray{nullptr};
			VertexBuffer* CurrentVertexBuffer{nullptr};
			IndexBuffer* CurrentIndexBuffer{nullptr};
			ShaderProgram* CurrentShader{nullptr};

			Statistics Stats;

			std::vector<QuadInstance> WorldSpaceTransparentQuadQueue;
			std::vector<QuadInstance> WorldSpaceOpaqueQuadQueue;

			std::vector<QuadInstance> ScreenSpaceTransparentQuadQueue;
			std::vector<QuadInstance> ScreenSpaceOpaqueQuadQueue;

			glm::mat4 CurrentViewProjectionMatrix{ 1.0f };
			glm::mat4 WorldViewProjectionMatrix{ 1.0f };
			glm::mat4 ScreenSpaceViewProjectionMatrix{ 1.0f };
		};

		static RendererData* s_Data;

		static void DrawQuadInstanced(const QuadInstance& quad);

		static void StartNewInstancedSet();

		static void FlushWorldQueues();

		static void FlushScreenSpaceQueues();

		static void FlushInstancedQuads();
	};
}
