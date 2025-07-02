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

namespace Cori {
	namespace Test {

		class Renderer2D {
		public:
			Renderer2D();
			~Renderer2D();

			void Init();
			void Shutdown();

			void BeginScene(const Components::Scene::Camera& camera);
			void EndScene();

			void BeginBatch();
			void EndBatch();

			void SubmitOpaqueQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation);
			void SubmitTransparentFrontQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation);
			void SubmitTransparentBackQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation);

			void DrawQuad(const glm::mat4& transform, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor);
			void DrawQuadFlipped(const glm::mat4& transform, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor);

			void FlushOpaqueQuads();
			void FlushTransparentFrontQuads();
			void FlushTransparentBackQuads();

			// void DrawCircle(...);
			// void DrawLine(...);


			struct Quad {
				glm::vec2 position;
				glm::vec2 size;
				glm::vec4 tintColor;
				Texture2D* texture;
				UVs uvs;
				float rotation;
				uint16_t layer;
			};

		private:
			struct Statistics {
				uint32_t DrawCalls = 0;
				uint32_t QuadCount = 0;

				uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
				uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
			};

			struct QuadVertex {
				glm::vec3 Position;
				glm::vec2 TexturePosition;
				glm::vec4 TintColor;
			};

			struct RendererData {
				static constexpr uint32_t MaxQuadCount{ 3000 };
				static constexpr uint32_t MaxVertexCount{ MaxQuadCount * 4 };
				static constexpr uint32_t MaxIndexCount{ MaxQuadCount * 6 };

				static constexpr uint32_t OpaqueQuadQueueSize{ 9000 };
				static constexpr uint32_t TransparentQuadQueueSize{ 3000 };

				std::shared_ptr<VertexArray> QuadVertexArray;
				std::shared_ptr<VertexBuffer> QuadVertexBuffer;
				std::shared_ptr<ShaderProgram> QuadShader;
				std::shared_ptr<IndexBuffer> QuadIndexBuffer;

				uint32_t QuadIndexCount{ 0 };
				QuadVertex* QuadVertexBufferBase{ nullptr };
				QuadVertex* QuadVertexBufferPtr{ nullptr };

				std::shared_ptr<Texture2D> WhiteTexture;
				Texture2D* CurrentTexture{ nullptr };

				glm::vec4 QuadVertexPositions[4];

				Statistics Stats;

				std::vector<Quad> OpaqueQuadQueue;
				std::vector<Quad> TransparentFrontQuadQueue;
				std::vector<Quad> TransparentBackQuadQueue;


				glm::mat4 CurrentViewProjectionMatrix{ 1.0f };
				glm::mat4 CurrentUIViewProjectionMatrix{ 1.0f };

			};

			std::unique_ptr<RendererData> m_Data;

			void Flush();

			void StartNewBatch();
		};
	}
}
