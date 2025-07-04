#include "Renderer2DNew.hpp"
#include <ska_sort.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glad/gl.h>

namespace Cori {
	namespace Test {

		Renderer2D::RendererData* Renderer2D::s_Data{ nullptr };

		void Renderer2D::Init() {
			CORI_CORE_INFO_TAGGED({ "Graphics", "Renderer2D" }, "Initializing Renderer2D.");
			
			// make this a static class
			s_Data = new RendererData();
			// use s_Data

			uint32_t indicesBatch[s_Data->MaxIndexCount];
			uint32_t offset = 0;
			for (int i = 0; i < s_Data->MaxIndexCount; i += 6) {
				indicesBatch[i + 0] = 0 + offset;
				indicesBatch[i + 1] = 1 + offset;
				indicesBatch[i + 2] = 2 + offset;

				indicesBatch[i + 3] = 2 + offset;
				indicesBatch[i + 4] = 3 + offset;
				indicesBatch[i + 5] = 0 + offset;

				offset += 4;
			}

			s_Data->QuadVertexArray = VertexArray::Create();
			s_Data->QuadVertexBuffer = VertexBuffer::Create();
			s_Data->QuadVertexBuffer->SetLayout({
				{ ShaderDataType::Vec2, "a_WorldPosition"   },
				{ ShaderDataType::Vec2, "a_LocalPosition"   },
				{ ShaderDataType::Vec2, "a_TexturePosition" },
				{ ShaderDataType::Vec2, "a_Size"            },
				{ ShaderDataType::Vec4, "a_TintColor"       },
				{ ShaderDataType::Float, "a_Layer"          },
				{ ShaderDataType::Float, "a_Rotation"       }
			});

			s_Data->QuadVertexBuffer->Init(nullptr, s_Data->MaxVertexCount * s_Data->QuadVertexBuffer->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
			s_Data->QuadVertexArray->AddVertexBuffer(s_Data->QuadVertexBuffer);

			s_Data->QuadIndexBuffer = IndexBuffer::Create(indicesBatch, s_Data->MaxIndexCount);
			s_Data->QuadVertexArray->AddIndexBuffer(s_Data->QuadIndexBuffer);

			s_Data->QuadVertexBufferBase = new QuadVertex[s_Data->MaxVertexCount];

			s_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
			s_Data->QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
			s_Data->QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
			s_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

			const Texture2DDescriptor wt {
				"White Texture",
				"assets/engine/textures/white1x1.png"
			};

			s_Data->WhiteTexture = AssetManager::GetTexture2DOwning(wt);

			const ShaderProgramDescriptor shader {
				"Quad Shader",
				"assets/engine/shaders/QuadVert.glsl",
				"assets/engine/shaders/QuadFrag.glsl"
			};

			s_Data->QuadShader = AssetManager::GetShaderOwning(shader);

			s_Data->OpaqueQuadQueue.reserve(s_Data->OpaqueQuadQueueSize);
			s_Data->TransparentFrontQuadQueue.reserve(s_Data->TransparentQuadQueueSize);
			s_Data->TransparentBackQuadQueue.reserve(s_Data->TransparentQuadQueueSize);


			s_Data->QuadInstanceVertexArray = VertexArray::Create();
			s_Data->QuadInstanceVertexBuffer = VertexBuffer::Create();
			s_Data->QuadInstanceVertexBuffer->SetLayout({
				{ ShaderDataType::Vec2, "a_WorldPosition",   1 },
				{ ShaderDataType::Vec2, "a_LocalPosition",   1 },
				{ ShaderDataType::Vec4, "a_TexturePosition", 1 },
				{ ShaderDataType::Vec2, "a_Size",            1 },
				{ ShaderDataType::Vec4, "a_TintColor",       1 },
				{ ShaderDataType::Float, "a_Layer",          1 },
				{ ShaderDataType::Float, "a_Rotation",       1 }
				});

			s_Data->QuadInstanceVertexBuffer->Init(nullptr, s_Data->MaxInstanceCount * s_Data->QuadInstanceVertexBuffer->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
			s_Data->QuadInstanceVertexArray->AddVertexBuffer(s_Data->QuadInstanceVertexBuffer);

			uint32_t quadIndices[6] = { 0, 1, 2, 2, 3, 0 };
			s_Data->QuadInstanceIndexBuffer = IndexBuffer::Create(quadIndices, 6);
			s_Data->QuadInstanceVertexArray->AddIndexBuffer(s_Data->QuadInstanceIndexBuffer);

			s_Data->QuadInstanceBufferBase = new QuadInstanace[s_Data->MaxInstanceCount];

			const ShaderProgramDescriptor shader1{
				"Quad Instanced Shader",
				"assets/engine/shaders/QuadInstancedVert.glsl",
				"assets/engine/shaders/QuadInstancedFrag.glsl"
			};

			s_Data->QuadInstanceShader = AssetManager::GetShaderOwning(shader1);

			CORI_CORE_INFO_TAGGED({ "Graphics", "Renderer2D" }, "Renderer2D Initialized successfully.");
		}


		void Renderer2D::Shutdown() {
			CORI_CORE_INFO_TAGGED({ "Graphics", "Renderer2D" }, "Shutting down Renderer2D.");

			delete[] s_Data->QuadVertexBufferBase;
			delete[] s_Data->QuadInstanceBufferBase;
			delete s_Data;
		}

		void Renderer2D::BeginScene(const Components::Scene::Camera& camera) {
			CORI_PROFILE_FUNCTION();

			s_Data->CurrentViewProjectionMatrix = camera.m_ViewProjectionMatrix;

			// later
			//s_Data->CurrentUIViewProjectionMatrix = camera.m_UIViewProjectionMatrix;
			s_Data->Stats.DrawCalls = 0;
			s_Data->Stats.QuadCount = 0;

		}

		void Renderer2D::EndScene() {
			CORI_PROFILE_FUNCTION();

			//FlushTransparentBackQuads();
			//FlushOpaqueQuads();
			//FlushTransparentFrontQuads();
			
			FlushOpaqueInstancedQuads();
		}

		void Renderer2D::BeginBatch() {
			CORI_PROFILE_FUNCTION();

			s_Data->QuadIndexCount = 0;
			s_Data->QuadVertexBufferPtr = s_Data->QuadVertexBufferBase;

		}

		void Renderer2D::EndBatch() {
			FlushQuads();
		}

		void Renderer2D::StartNewBatch() {
			EndBatch();
			BeginBatch();
		}

		void Renderer2D::BeginInstancedBatch() {
			CORI_PROFILE_FUNCTION();

			s_Data->QuadInstanceCount = 0;
			s_Data->QuadInstanceBufferPtr = s_Data->QuadInstanceBufferBase;
		}

		void Renderer2D::EndInstancedBatch() {
			FlushInstancedQuads();
		}


		void Renderer2D::StartNewInstancedBatch() {
			EndInstancedBatch();
			BeginInstancedBatch();
		}

		void Renderer2D::SubmitOpaqueQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation, bool flipped) {
			s_Data->OpaqueQuadQueue.emplace_back(position, size, tintColor.a < 1.0f ? glm::vec4{ tintColor.x, tintColor.y, tintColor.y, 1.0f } : tintColor, texture ? texture : s_Data->WhiteTexture.get(), flipped ? UVs{ {uvs.UVmax.x, uvs.UVmin.y}, {uvs.UVmin.x, uvs.UVmax.y} } : uvs, glm::radians(rotation), layer);
		}

			

		void Renderer2D::SubmitTransparentFrontQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation, bool flipped) {
			s_Data->TransparentFrontQuadQueue.emplace_back(position, size, tintColor.a < 1.0f ? glm::vec4{ tintColor.x, tintColor.y, tintColor.y, 1.0f } : tintColor, texture ? texture : s_Data->WhiteTexture.get(), flipped ? UVs{ {uvs.UVmax.x, uvs.UVmin.y}, {uvs.UVmin.x, uvs.UVmax.y} } : uvs, glm::radians(rotation), layer);
		}

		void Renderer2D::SubmitTransparentBackQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation, bool flipped) {
			s_Data->TransparentBackQuadQueue.emplace_back(position, size, tintColor.a < 1.0f ? glm::vec4{ tintColor.x, tintColor.y, tintColor.y, 1.0f } : tintColor, texture ? texture : s_Data->WhiteTexture.get(), flipped ? UVs{ {uvs.UVmax.x, uvs.UVmin.y}, {uvs.UVmin.x, uvs.UVmax.y} } : uvs, glm::radians(rotation), layer);
		}

		void Renderer2D::DrawQuad(const glm::mat4& transform, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor) {
			if (CORI_CORE_ASSERT_ERROR(texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
			if (s_Data->QuadIndexCount >= s_Data->MaxIndexCount) {
				StartNewBatch();
			}

			if (s_Data->NecessaryTexture != texture) {
				if (!s_Data->NecessaryTexture) {
					s_Data->NecessaryTexture = texture;
				}
				else {
					StartNewBatch();
					s_Data->NecessaryTexture = texture;
				}
			}

			static constexpr glm::vec2 localPositions[] = {
				{ -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f,  0.5f }, { -0.5f,  0.5f }
			};

			//s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[0];
			//s_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmin.x, uvs.UVmin.y };
			//s_Data->QuadVertexBufferPtr->TintColor = tintColor;
			//s_Data->QuadVertexBufferPtr++;
			//
			//s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[1];
			//s_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmax.x, uvs.UVmin.y };
			//s_Data->QuadVertexBufferPtr->TintColor = tintColor;
			//s_Data->QuadVertexBufferPtr++;
			//
			//s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[2];
			//s_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmax.x, uvs.UVmax.y };
			//s_Data->QuadVertexBufferPtr->TintColor = tintColor;
			//s_Data->QuadVertexBufferPtr++;
			//
			//s_Data->QuadVertexBufferPtr->Position = transform * s_Data->QuadVertexPositions[3];
			//s_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmin.x, uvs.UVmax.y };
			//s_Data->QuadVertexBufferPtr->TintColor = tintColor;
			//s_Data->QuadVertexBufferPtr++;

			s_Data->QuadIndexCount += 6;

			s_Data->Stats.QuadCount++;
		}

		void Renderer2D::DrawQuad(const Quad& quad) {

			if (CORI_CORE_ASSERT_ERROR(quad.texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
			if (s_Data->QuadIndexCount >= s_Data->MaxIndexCount) {
				StartNewBatch();
			}

			if (s_Data->NecessaryTexture != quad.texture) {
				if (!s_Data->NecessaryTexture) {
					s_Data->NecessaryTexture = quad.texture;
				}
				else {
					StartNewBatch();
					s_Data->NecessaryTexture = quad.texture;
				}
			}

			static constexpr glm::vec2 localPositions[] = {
				{ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
			};

			s_Data->QuadVertexBufferPtr->WorldPosition = quad.position;
			s_Data->QuadVertexBufferPtr->LocalPosition = localPositions[0];
			s_Data->QuadVertexBufferPtr->TexturePosition = quad.uvs.UVmin;
			s_Data->QuadVertexBufferPtr->Size = quad.size;
			s_Data->QuadVertexBufferPtr->TintColor = quad.tintColor;
			s_Data->QuadVertexBufferPtr->Layer = quad.layer;
			s_Data->QuadVertexBufferPtr->Rotation = quad.rotation;
			s_Data->QuadVertexBufferPtr++;

			s_Data->QuadVertexBufferPtr->WorldPosition = quad.position;
			s_Data->QuadVertexBufferPtr->LocalPosition = localPositions[1];
			s_Data->QuadVertexBufferPtr->TexturePosition = { quad.uvs.UVmax.x, quad.uvs.UVmin.y };
			s_Data->QuadVertexBufferPtr->Size = quad.size;
			s_Data->QuadVertexBufferPtr->TintColor = quad.tintColor;
			s_Data->QuadVertexBufferPtr->Layer = quad.layer;
			s_Data->QuadVertexBufferPtr->Rotation = quad.rotation;
			s_Data->QuadVertexBufferPtr++;

			s_Data->QuadVertexBufferPtr->WorldPosition = quad.position;
			s_Data->QuadVertexBufferPtr->LocalPosition = localPositions[2];
			s_Data->QuadVertexBufferPtr->TexturePosition = quad.uvs.UVmax;
			s_Data->QuadVertexBufferPtr->Size = quad.size;
			s_Data->QuadVertexBufferPtr->TintColor = quad.tintColor;
			s_Data->QuadVertexBufferPtr->Layer = quad.layer;
			s_Data->QuadVertexBufferPtr->Rotation = quad.rotation;
			s_Data->QuadVertexBufferPtr++;

			s_Data->QuadVertexBufferPtr->WorldPosition = quad.position;
			s_Data->QuadVertexBufferPtr->LocalPosition = localPositions[3];
			s_Data->QuadVertexBufferPtr->TexturePosition = { quad.uvs.UVmin.x, quad.uvs.UVmax.y };
			s_Data->QuadVertexBufferPtr->Size = quad.size;
			s_Data->QuadVertexBufferPtr->TintColor = quad.tintColor;
			s_Data->QuadVertexBufferPtr->Layer = quad.layer;
			s_Data->QuadVertexBufferPtr->Rotation = quad.rotation;
			s_Data->QuadVertexBufferPtr++;


			s_Data->QuadIndexCount += 6;

			s_Data->Stats.QuadCount++;

		}

		// maybe add tiling factor????
		// maybe tiling factor should be n in 2^n?? and an int??, so pixelart would look nice

		void Renderer2D::FlushOpaqueQuads() {
			CORI_PROFILE_FUNCTION();

			if (s_Data->OpaqueQuadQueue.empty()) { return; }

			{
				CORI_PROFILE_SCOPE("Opaque sort");
				ska_sort(
					s_Data->OpaqueQuadQueue.begin(),
					s_Data->OpaqueQuadQueue.end(),
					[](const Quad& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.texture);
					}
				);
			}


			if (s_Data->CurrentVertexArray != s_Data->QuadVertexArray.get()) {
				s_Data->QuadVertexArray->Bind();
				s_Data->CurrentVertexArray = s_Data->QuadVertexArray.get();
			}
			if (s_Data->CurrentVertexBuffer != s_Data->QuadVertexBuffer.get()) {
				s_Data->QuadVertexBuffer->Bind();
				s_Data->CurrentVertexBuffer = s_Data->QuadVertexBuffer.get();
			}
			if (s_Data->CurrentIndexBuffer != s_Data->QuadIndexBuffer.get()) {
				s_Data->QuadIndexBuffer->Bind();
				s_Data->CurrentIndexBuffer = s_Data->QuadIndexBuffer.get();
			}
			if (s_Data->CurrentShader != s_Data->QuadShader.get()) {
				s_Data->QuadShader->Bind();
				s_Data->CurrentShader = s_Data->QuadShader.get();
			}

			GraphicsCall::DisableBlending();
			GraphicsCall::EnableDepthTest();

			BeginBatch();

			{
				CORI_PROFILE_SCOPE("Draw requests");
				for (const auto& quad : s_Data->OpaqueQuadQueue) {
					DrawQuad(quad);
				}
			}

			EndBatch();

			s_Data->OpaqueQuadQueue.clear();
		}

		void Renderer2D::FlushTransparentFrontQuads() {
			CORI_PROFILE_FUNCTION();

			if (s_Data->TransparentFrontQuadQueue.empty()) { return; }

			ska_sort(
				s_Data->TransparentFrontQuadQueue.begin(),
				s_Data->TransparentFrontQuadQueue.end(),
				[](const Quad& quad) -> uint64_t {
					return reinterpret_cast<uint64_t>(quad.texture);
				}
			);

			ska_sort(
				s_Data->TransparentFrontQuadQueue.begin(),
				s_Data->TransparentFrontQuadQueue.end(),
				[](const Quad& quad) -> uint16_t {
					return quad.layer;
				}
			);


			if (s_Data->CurrentVertexArray != s_Data->QuadVertexArray.get()) {
				s_Data->QuadVertexArray->Bind();
				s_Data->CurrentVertexArray = s_Data->QuadVertexArray.get();
			}
			if (s_Data->CurrentVertexBuffer != s_Data->QuadVertexBuffer.get()) {
				s_Data->QuadVertexBuffer->Bind();
				s_Data->CurrentVertexBuffer = s_Data->QuadVertexBuffer.get();
			}
			if (s_Data->CurrentIndexBuffer != s_Data->QuadIndexBuffer.get()) {
				s_Data->QuadIndexBuffer->Bind();
				s_Data->CurrentIndexBuffer = s_Data->QuadIndexBuffer.get();
			}
			if (s_Data->CurrentShader != s_Data->QuadShader.get()) {
				s_Data->QuadShader->Bind();
				s_Data->CurrentShader = s_Data->QuadShader.get();
			}

			GraphicsCall::DisableDepthTest();
			GraphicsCall::EnableBlending();

			BeginBatch();

			for (const auto& quad : s_Data->TransparentFrontQuadQueue) {
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), { quad.position.x, quad.position.y, 0.0f })
					* glm::rotate(glm::mat4(1.0f), glm::radians(quad.rotation), { 0.0f, 0.0f, 1.0f })
					* glm::scale(glm::mat4(1.0f), { quad.size.x, quad.size.y, 1.0f });

				//DrawQuad(transform, quad.texture, quad.uvs, quad.tintColor);
			}

			EndBatch();

			s_Data->TransparentFrontQuadQueue.clear();

		}

		void Renderer2D::FlushTransparentBackQuads() {
			CORI_PROFILE_FUNCTION();

			if (s_Data->TransparentBackQuadQueue.empty()) { return; }

			ska_sort(
				s_Data->TransparentBackQuadQueue.begin(),
				s_Data->TransparentBackQuadQueue.end(),
				[](const Quad& quad) -> uint64_t {
					return reinterpret_cast<uint64_t>(quad.texture);
				}
			);

			ska_sort(
				s_Data->TransparentBackQuadQueue.begin(),
				s_Data->TransparentBackQuadQueue.end(),
				[](const Quad& quad) -> uint16_t {
					return quad.layer;
				}
			);


			if (s_Data->CurrentVertexArray != s_Data->QuadVertexArray.get()) {
				s_Data->QuadVertexArray->Bind();
				s_Data->CurrentVertexArray = s_Data->QuadVertexArray.get();
			}
			if (s_Data->CurrentVertexBuffer != s_Data->QuadVertexBuffer.get()) {
				s_Data->QuadVertexBuffer->Bind();
				s_Data->CurrentVertexBuffer = s_Data->QuadVertexBuffer.get();
			}
			if (s_Data->CurrentIndexBuffer != s_Data->QuadIndexBuffer.get()) {
				s_Data->QuadIndexBuffer->Bind();
				s_Data->CurrentIndexBuffer = s_Data->QuadIndexBuffer.get();
			}
			if (s_Data->CurrentShader != s_Data->QuadShader.get()) {
				s_Data->QuadShader->Bind();
				s_Data->CurrentShader = s_Data->QuadShader.get();
			}

			GraphicsCall::DisableDepthTest();
			GraphicsCall::EnableBlending();

			BeginBatch();

			for (const auto& quad : s_Data->TransparentBackQuadQueue) {
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), { quad.position.x, quad.position.y, 0.0f })
					* glm::rotate(glm::mat4(1.0f), glm::radians(quad.rotation), { 0.0f, 0.0f, 1.0f })
					* glm::scale(glm::mat4(1.0f), { quad.size.x, quad.size.y, 1.0f });

				//DrawQuad(transform, quad.texture, quad.uvs, quad.tintColor);
			}

			EndBatch();

			s_Data->TransparentBackQuadQueue.clear();

		}

		void Renderer2D::FlushQuads() {
			CORI_PROFILE_FUNCTION();
			auto size = reinterpret_cast<uint8_t*>(s_Data->QuadVertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data->QuadVertexBufferBase);
			s_Data->QuadVertexBuffer->SetData(s_Data->QuadVertexBufferBase, size);

			s_Data->QuadShader->SetMat4("u_ViewProjection", s_Data->CurrentViewProjectionMatrix);
			s_Data->QuadShader->SetInt("u_Texture", 0);

			if (s_Data->CurrentTexture != s_Data->NecessaryTexture) {
				s_Data->NecessaryTexture->Bind(0);
				s_Data->CurrentTexture = s_Data->NecessaryTexture;
			}

			GraphicsCall::DrawElements(s_Data->QuadVertexArray, s_Data->QuadIndexCount);
			s_Data->Stats.DrawCalls++;

			s_Data->QuadIndexCount = 0;
		}

		void Renderer2D::DrawQuadInstanced(const Quad& quad) {
			if (CORI_CORE_ASSERT_ERROR(quad.texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
			if (s_Data->QuadInstanceCount >= s_Data->MaxInstanceCount) {
				StartNewInstancedBatch();
			}

			if (s_Data->NecessaryTexture != quad.texture) {
				if (!s_Data->NecessaryTexture) {
					s_Data->NecessaryTexture = quad.texture;
				}
				else {
					StartNewInstancedBatch();
					s_Data->NecessaryTexture = quad.texture;
				}
			}

			s_Data->QuadInstanceBufferPtr->WorldPosition = quad.position;
			s_Data->QuadInstanceBufferPtr->LocalPosition = { 1.0f, 1.0f };
			s_Data->QuadInstanceBufferPtr->TexturePosition = { quad.uvs.UVmin, quad.uvs.UVmax };
			s_Data->QuadInstanceBufferPtr->Size = quad.size;
			s_Data->QuadInstanceBufferPtr->TintColor = quad.tintColor;
			s_Data->QuadInstanceBufferPtr->Layer = quad.layer;
			s_Data->QuadInstanceBufferPtr->Rotation = quad.rotation;
			s_Data->QuadInstanceBufferPtr++;


			s_Data->QuadInstanceCount++;

			s_Data->Stats.QuadCount++;

		}



		void Renderer2D::FlushOpaqueInstancedQuads() {	
			CORI_PROFILE_FUNCTION();

			if (s_Data->OpaqueQuadQueue.empty()) { return; }

			{
				CORI_PROFILE_SCOPE("Opaque sort inst");
				//ska_sort(
				//	s_Data->OpaqueQuadQueue.begin(),
				//	s_Data->OpaqueQuadQueue.end(),
				//	[](const Quad& quad) -> uint64_t {
				//		return reinterpret_cast<uint64_t>(quad.texture);
				//	}
				//);
			}

			if (s_Data->CurrentVertexArray != s_Data->QuadInstanceVertexArray.get()) {
				s_Data->QuadInstanceVertexArray->Bind();
				s_Data->CurrentVertexArray = s_Data->QuadInstanceVertexArray.get();
			}
			if (s_Data->CurrentVertexBuffer != s_Data->QuadInstanceVertexBuffer.get()) {
				s_Data->QuadInstanceVertexBuffer->Bind();
				s_Data->CurrentVertexBuffer = s_Data->QuadInstanceVertexBuffer.get();
			}
			if (s_Data->CurrentIndexBuffer != s_Data->QuadInstanceIndexBuffer.get()) {
				s_Data->QuadInstanceIndexBuffer->Bind();
				s_Data->CurrentIndexBuffer = s_Data->QuadInstanceIndexBuffer.get();
			}
			if (s_Data->CurrentShader != s_Data->QuadInstanceShader.get()) {
				s_Data->QuadInstanceShader->Bind();
				s_Data->CurrentShader = s_Data->QuadInstanceShader.get();
			}

			GraphicsCall::DisableBlending();
			GraphicsCall::EnableDepthTest();

			BeginInstancedBatch();

			{
				CORI_PROFILE_SCOPE("Draw requests inst");
				for (const auto& quad : s_Data->OpaqueQuadQueue) {
					DrawQuadInstanced(quad);
				}
			}

			EndInstancedBatch();

			s_Data->OpaqueQuadQueue.clear();

		}

		void Renderer2D::FlushInstancedQuads() {

			CORI_PROFILE_FUNCTION();
			auto size = reinterpret_cast<uint8_t*>(s_Data->QuadInstanceBufferPtr) - reinterpret_cast<uint8_t*>(s_Data->QuadInstanceBufferBase);
			s_Data->QuadInstanceVertexBuffer->SetData(s_Data->QuadInstanceBufferBase, size);

			s_Data->QuadInstanceShader->SetMat4("u_ViewProjection", s_Data->CurrentViewProjectionMatrix);
			s_Data->QuadInstanceShader->SetInt("u_Texture", 0);

			if (s_Data->CurrentTexture != s_Data->NecessaryTexture) {
				s_Data->NecessaryTexture->Bind(0);
				s_Data->CurrentTexture = s_Data->NecessaryTexture;
			}

			glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, s_Data->QuadInstanceCount);	


			s_Data->Stats.DrawCalls++;

			s_Data->QuadInstanceCount = 0;
		}

	}
}