#pragma once
#include "Buffers.hpp"
#include "VertexArray.hpp"
#include "ShaderProgram.hpp"
#include "PipelineProgram.hpp"
#include "Texture.hpp"
#include "SpriteAtlas.hpp"
#include "Sprite.hpp"
#include "CameraComponent.hpp"
#include "AssetManager/AssetManager.hpp"
#include "GraphicsCall.hpp"
#include "PrimitivePool.hpp"

namespace Cori {
	namespace Test {
		class Renderer2D {
			struct Quad {
				glm::vec2 m_Position;
				glm::vec2 m_Size;
				glm::vec4 m_TintColor;
				Texture2D* m_Texture;
				UVs m_UVs;
				float m_Rotation;
				uint8_t m_Layer;

				Quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, Texture2D* texture, const UVs& uvs, float rotation, uint32_t layer)
					: m_Position(position), m_Size(size), m_TintColor(color), m_Texture(texture), m_UVs(uvs), m_Rotation(rotation), m_Layer(layer) {}
			};
		public:

			static void Init();
			static void Shutdown();

			static void BeginScene(const Components::Scene::Camera& camera);
			static void EndScene();

			static void SubmitTransparentQuad(const Graphics::QuadPrimitive& quad);

			static void DrawQuadInstanced(const Quad& quad);

			static void DrawQuadInstanced(const Graphics::QuadPrimitive& quad);

			//static void DrawQuadInstanced(glm::vec2 position, );

			// void DrawCircle(...);
			// void DrawLine(...);

			static void BeginInstancedSet();
			static void EndInstancedSet();



		private:


			struct QuadInstanace {
				glm::vec2 WorldPosition{ 0.0f };
				glm::vec2 LocalPosition{ 0.0f };
				glm::vec4 TexturePosition{ 0.0f };
				glm::vec2 Size{ 0.0f };
				glm::vec4 TintColor{ 0.0f };
				float Layer{ 0.0f };
				float Rotation{ 0.0f };

				QuadInstanace() = default;

				QuadInstanace(const glm::vec2& pos, const glm::vec2& lpos, const glm::vec4& uvs, glm::vec2 sz, const glm::vec4& color, float layer, float rot)
					: WorldPosition(pos), LocalPosition(lpos), TexturePosition(uvs), Size(sz), TintColor(color), Layer(layer), Rotation(rot) {}
			};


			struct Statistics {
				uint32_t DrawCalls = 0;
				uint32_t QuadCount = 0;
			};

			struct RendererData {
				static constexpr uint32_t TransparentQuadQueueSize{ 1024 };

				static constexpr uint32_t MaxInstanceCount{ 6144 };

				std::shared_ptr<VertexArray> QuadInstanceVertexArray;
				std::shared_ptr<VertexBuffer> QuadInstanceVertexBuffer;
				std::shared_ptr<IndexBuffer> QuadInstanceIndexBuffer;
				std::shared_ptr<ShaderProgram> QuadInstanceShader;

				uint32_t QuadInstanceCount{ 0 };
				QuadInstanace* QuadInstanceBufferBase{ nullptr };
				QuadInstanace* QuadInstanceBufferPtr{ nullptr };

				std::shared_ptr<Texture2D> WhiteTexture;
				Texture2D* NecessaryTexture{ nullptr };

				Texture2D* CurrentTexture{ nullptr };
				VertexArray* CurrentVertexArray{ nullptr };
				VertexBuffer* CurrentVertexBuffer{ nullptr };
				IndexBuffer* CurrentIndexBuffer{ nullptr };
				ShaderProgram* CurrentShader{ nullptr };

				Statistics Stats;

				std::vector<Quad> TransparentQuadQueue;

				glm::mat4 CurrentViewProjectionMatrix{ 1.0f };
				glm::mat4 CurrentUIViewProjectionMatrix{ 1.0f };

			};


			static RendererData* s_Data;


			static void StartNewInstancedSet();

			static void DrawTransparentInstancedQuads();

			static void FlushInstancedQuads();
		};
	}
}
