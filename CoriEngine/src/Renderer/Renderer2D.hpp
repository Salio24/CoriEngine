#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "Texture.hpp"
#include "CameraComponent.hpp"
#include "GraphicsCall.hpp"
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

			QuadInstance(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, Texture2D* texture, const glm::vec4& uvs, uint8_t layer) :
				m_Transform(transform), m_Size(size), m_UVs(uvs), m_Texture(texture), m_TintColor(tintColor), m_Layer(layer) {}
		};

	public:
		struct Statistics {
			uint32_t DrawCalls{ 0 };
			uint32_t QuadCount{ 0 };
		};

		static void BeginScene(const Components::Scene::Camera& camera);
		static void EndScene();

		static void DrawScene(Scene* scene);

		static void SubmitTransparentQuad(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, uint8_t depth, bool flipX, bool flipY, bool flatColored);

		static void SubmitOpaqueQuad(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, uint8_t depth, bool flipX, bool flipY, bool flatColored);

		static Statistics GetStatistics();

		// void DrawCircle(...);
		// void DrawLine(...);

		static void BeginInstancedSet();
		static void EndInstancedSet();

	protected:
		friend GraphicsCall;
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

			Quad(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor, const glm::vec4& uvs, float layer) :
				m_Transform(transform), m_TexturePosition(uvs), m_Size(size), m_TintColor(tintColor), m_Layer(layer) {}
		};

		struct RendererData {
			static constexpr uint32_t TransparentQuadQueueSize{ 1024 };

			static constexpr uint32_t MaxInstanceCount{ 6144 };

			std::shared_ptr<VertexArray> QuadInstanceVertexArray;
			std::shared_ptr<VertexBuffer> QuadInstanceVertexBuffer;
			std::shared_ptr<IndexBuffer> QuadInstanceIndexBuffer;
			std::shared_ptr<ShaderProgram> QuadInstanceShader;

			uint32_t QuadInstanceCount{0};
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

			std::vector<QuadInstance> TransparentQuadQueue;
			std::vector<QuadInstance> OpaqueQuadQueue;

			glm::mat4 CurrentViewProjectionMatrix{ 1.0f };
			glm::mat4 CurrentUIViewProjectionMatrix{ 1.0f };
		};

		static RendererData* s_Data;

		static void DrawQuadInstanced(const QuadInstance& quad);

		static void StartNewInstancedSet();

		static void FlushQueues();

		static void FlushInstancedQuads();
	};
}
