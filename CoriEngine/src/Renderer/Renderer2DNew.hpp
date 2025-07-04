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
			struct Quad {
				glm::vec2 position;
				glm::vec2 size;
				glm::vec4 tintColor;
				Texture2D* texture;
				UVs uvs;
				float rotation;
				uint16_t layer;

				Quad(const glm::vec2& pos, const glm::vec2& sz, const glm::vec4& color, Texture2D* tex, const UVs& texCoords, float rot, uint16_t lyr)
					: position(pos), size(sz), tintColor(color), texture(tex), uvs(texCoords), rotation(rot), layer(lyr) {
				}
			};

			static void Init();
			static void Shutdown();

			static void BeginScene(const Components::Scene::Camera& camera);
			static void EndScene();

			static void BeginBatch();
			static void EndBatch();

			static void SubmitOpaqueQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation, bool flipped);

			static void SubmitTransparentFrontQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation, bool flipped);
			static void SubmitTransparentBackQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation, bool flipped);

			static void DrawQuad(const glm::mat4& transform, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor);

			static void DrawQuadInstanced(const Quad& quad);
			static void DrawQuad(const Quad& quad);
			// void DrawCircle(...);
			// void DrawLine(...);

			/*
				if  will decide to support circle/segment/line rendering
				add several "custom properties" here, and an enum that will decide, if this object is a circle/quad/segment/line
				for example i have custom fields: glm::vec2 property1, glm::vec2 property2, float property3, ShapeEnum shapeType
				if ShapeType == Quad then for example
				property1 is size, property2 and 3 are not used
				if ShapeType == Circle then for example
				property1 is center in local coordinates, property2 is unused, property3 is radius
				if ShapeType == Segment then for example
				property1 is center1 in local coordinates, property2 is center2 in local coordinates, property3 is radius
				if ShapeType == Line then for example
				property1 is start in local coordinates, property2 is end in local coordinates, property3 is thickness 
				and so on

				then in flush function for transparent, sort the queue according to layer then according to shapeType, then according to textureptr
				for opaque sort according to shapeType, then according to textureptr

				then create a draw overloads 
			*/




//		private:

			struct QuadVertex {
				glm::vec2 WorldPosition;
				glm::vec2 LocalPosition;
				glm::vec2 TexturePosition;
				glm::vec2 Size;
				glm::vec4 TintColor;
				float Layer;
				float Rotation;
			};

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
					: WorldPosition(pos), LocalPosition(lpos), TexturePosition(uvs), Size(sz), TintColor(color), Layer(layer), Rotation(rot) {
				}
			};


			struct Statistics {
				uint32_t DrawCalls = 0;
				uint32_t QuadCount = 0;

				uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
				uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
			};

			struct RendererData {
				static constexpr uint32_t MaxQuadCount{ 3000 };
				static constexpr uint32_t MaxVertexCount{ MaxQuadCount * 4 };
				static constexpr uint32_t MaxIndexCount{ MaxQuadCount * 6 };

				static constexpr uint32_t OpaqueQuadQueueSize{ 9000 };
				static constexpr uint32_t TransparentQuadQueueSize{ 3000 };

				static constexpr uint32_t MaxInstanceCount{ 6144 };

				std::shared_ptr<VertexArray> QuadVertexArray;
				std::shared_ptr<VertexBuffer> QuadVertexBuffer;
				std::shared_ptr<IndexBuffer> QuadIndexBuffer;
				std::shared_ptr<ShaderProgram> QuadShader;

				std::shared_ptr<VertexArray> QuadInstanceVertexArray;
				std::shared_ptr<VertexBuffer> QuadInstanceVertexBuffer;
				std::shared_ptr<IndexBuffer> QuadInstanceIndexBuffer;
				std::shared_ptr<ShaderProgram> QuadInstanceShader;

				uint32_t QuadIndexCount{ 0 };
				QuadVertex* QuadVertexBufferBase{ nullptr };
				QuadVertex* QuadVertexBufferPtr{ nullptr };

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

				glm::vec4 QuadVertexPositions[4];

				Statistics Stats;

				std::vector<Quad> OpaqueQuadQueue;
				std::vector<Quad> TransparentFrontQuadQueue;
				std::vector<Quad> TransparentBackQuadQueue;


				glm::mat4 CurrentViewProjectionMatrix{ 1.0f };
				glm::mat4 CurrentUIViewProjectionMatrix{ 1.0f };

			};

			static RendererData* s_Data;

			static void StartNewBatch();

			static void BeginInstancedBatch();
			static void EndInstancedBatch();
			static void StartNewInstancedBatch();
			static void FlushOpaqueQuads();
			static void FlushTransparentFrontQuads();
			static void FlushTransparentBackQuads();
			static void FlushQuads();

			static void FlushOpaqueInstancedQuads();
			static void FlushInstancedQuads();
		};
	}
}
