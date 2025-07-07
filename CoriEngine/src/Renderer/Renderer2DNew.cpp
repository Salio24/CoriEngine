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

			const Texture2DDescriptor wt {
				"White Texture",
				"assets/engine/textures/white1x1.png"
			};

			s_Data->WhiteTexture = AssetManager::GetTexture2DOwning(wt);

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

			FlushOpaqueInstancedQuads();
		}


		void Renderer2D::BeginInstancedSet() {
			CORI_PROFILE_FUNCTION();

			s_Data->QuadInstanceCount = 0;
			s_Data->QuadInstanceBufferPtr = s_Data->QuadInstanceBufferBase;
		}

		void Renderer2D::EndInstancedSet() {
			FlushInstancedQuads();
		}


		void Renderer2D::StartNewInstancedSet() {
			EndInstancedSet();
			BeginInstancedSet();
		}

		void Renderer2D::SubmitTransparentQuad(glm::vec2 position, glm::vec2 size, uint8_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation, bool flipped) {
			s_Data->TransparentQuadQueue.emplace_back(position, size, tintColor, texture ? texture : s_Data->WhiteTexture.get(), flipped ? UVs{ {uvs.UVmax.x, uvs.UVmin.y}, {uvs.UVmin.x, uvs.UVmax.y} } : uvs, rotation, layer);
		}

		// maybe add tiling factor????
		// maybe tiling factor should be n in 2^n?? and an int??, so pixelart would look nice

		void Renderer2D::DrawQuadInstanced(const Quad& quad) {
			if (CORI_CORE_ASSERT_ERROR(quad.texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
			if (s_Data->QuadInstanceCount >= s_Data->MaxInstanceCount) {
				StartNewInstancedSet();
			}

			if (s_Data->NecessaryTexture != quad.texture) {
				if (!s_Data->NecessaryTexture) {
					s_Data->NecessaryTexture = quad.texture;
				}
				else {
					StartNewInstancedSet();
					s_Data->NecessaryTexture = quad.texture;
				}
			}

			s_Data->QuadInstanceBufferPtr->WorldPosition = quad.position;
			s_Data->QuadInstanceBufferPtr->LocalPosition = { 1.0f, 1.0f };
			s_Data->QuadInstanceBufferPtr->TexturePosition = { quad.uvs.UVmin, quad.uvs.UVmax };
			s_Data->QuadInstanceBufferPtr->Size = quad.size;
			s_Data->QuadInstanceBufferPtr->TintColor = quad.tintColor;
			s_Data->QuadInstanceBufferPtr->Layer = quad.layer;
			s_Data->QuadInstanceBufferPtr->Rotation = glm::radians(quad.rotation);
			s_Data->QuadInstanceBufferPtr++;


			s_Data->QuadInstanceCount++;

			s_Data->Stats.QuadCount++;

		}



		void Renderer2D::DrawTransparentInstancedQuads() {
			CORI_PROFILE_FUNCTION();
			if (s_Data->TransparentQuadQueue.empty()) { return; }

			{
				CORI_PROFILE_SCOPE("Opaque sort inst");
				ska_sort(
					s_Data->TransparentQuadQueue.begin(),
					s_Data->TransparentQuadQueue.end(),
					[](const Quad& quad) -> uint8_t {
						return quad.layer;
					}
				);
			}


		}


		void Renderer2D::FlushOpaqueInstancedQuads() {
			CORI_PROFILE_FUNCTION();
/*
			if (s_Data->OpaqueQuadQueue.empty()) { return; }

			{
				CORI_PROFILE_SCOPE("Opaque sort inst");
				ska_sort(
					s_Data->OpaqueQuadQueue.begin(),
					s_Data->OpaqueQuadQueue.end(),
					[](const Quad& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.texture);
					}
				);
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

			//GraphicsCall::DisableBlending();
			//GraphicsCall::EnableDepthTest();

			BeginInstancedSet();

			{
				CORI_PROFILE_SCOPE("Draw requests inst");
				for (const auto& quad : s_Data->OpaqueQuadQueue) {
					DrawQuadInstanced(quad);
				}
			}

			EndInstancedSet();

			s_Data->OpaqueQuadQueue.clear();
*/
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

			glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, s_Data->QuadInstanceCount);


			s_Data->Stats.DrawCalls++;
			s_Data->Stats.QuadCount += s_Data->QuadInstanceCount;

			s_Data->QuadInstanceCount = 0;
		}

	}
}