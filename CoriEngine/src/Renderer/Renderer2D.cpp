#include "Renderer2D.hpp"
#include <ska_sort.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glad/gl.h>
#include "Core/Utility/AABB.hpp"

namespace Cori {
	Renderer2D::RendererData* Renderer2D::s_Data{nullptr};

	void Renderer2D::Init() {
		CORI_CORE_INFO_TAGGED({ "Graphics", "Renderer2D" }, "Initializing Renderer2D.");

		// make this a static class
		s_Data = new RendererData();
		// use s_Data

		const Texture2DDescriptor wt{
				"White Texture",
				"assets/engine/textures/white1x1.png"
			};

		s_Data->WhiteTexture = AssetManager::GetTexture2DOwning(wt);

		s_Data->QuadInstanceVertexArray = VertexArray::Create();
		s_Data->QuadInstanceVertexBuffer = VertexBuffer::Create();
		s_Data->QuadInstanceVertexBuffer->SetLayout({
				{ShaderDataType::Mat3, "a_Transform", 1},
				{ShaderDataType::Vec4, "a_TexturePosition", 1},
				{ShaderDataType::Vec2, "a_Size", 1},
				{ShaderDataType::Vec4, "a_TintColor", 1},
				{ShaderDataType::Float, "a_Layer", 1},
			});

		s_Data->QuadInstanceVertexBuffer->Init(
			nullptr, s_Data->MaxInstanceCount * s_Data->QuadInstanceVertexBuffer->GetLayout().GetStride(),
			DRAW_TYPE::DYNAMIC);
		s_Data->QuadInstanceVertexArray->AddVertexBuffer(s_Data->QuadInstanceVertexBuffer);

		uint32_t quadIndices[6] = {0, 1, 2, 2, 3, 0};
		s_Data->QuadInstanceIndexBuffer = IndexBuffer::Create(quadIndices, 6);
		s_Data->QuadInstanceVertexArray->AddIndexBuffer(s_Data->QuadInstanceIndexBuffer);

		s_Data->QuadInstanceBufferBase = new Quad[s_Data->MaxInstanceCount];

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
	}

	void Renderer2D::DrawScene(Scene* scene) {
		CORI_PROFILE_FUNCTION();

		BeginScene(scene->GetContextComponent<Components::Scene::Camera>());

		{
			CORI_PROFILE_SCOPE("Quad Submission");
			auto& camera = scene->GetContextComponent<Components::Scene::Camera>();
			Utils::AABB cameraBounds = { camera.m_CameraMinBound, camera.m_CameraMaxBound };
			EntityView view = scene->View<Components::Entity::QuadRenderer, Components::Entity::Transform>(Exclude<Components::Entity::InactiveLocallyFlag>());
			for (auto entity : view) {
				auto& renderer = view.Get<Components::Entity::QuadRenderer>(entity);
				if (renderer.m_Visible) {
					auto& transform = view.Get<Components::Entity::Transform>(entity);
					Utils::AABB entityBounds = Utils::CalculateAABB(transform.m_WorldTransform, renderer.m_HalfSize);
					if (Utils::AABBOverlapCheck(cameraBounds, entityBounds)) {
						if (renderer.GetSemiTransparencyState()) {
							SubmitTransparentQuad(transform.m_WorldTransform, renderer.m_HalfSize, renderer.GetColor(), renderer.GetTexture().get(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
							continue;
						}
						SubmitOpaqueQuad(transform.m_WorldTransform, renderer.m_HalfSize, renderer.GetColor(), renderer.GetTexture().get(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
					}
				}
			}
		}

		FlushQueues();

		EndScene();
	}


	void Renderer2D::BeginInstancedSet() {
		CORI_PROFILE_FUNCTION();

		s_Data->QuadInstanceCount = 0;
		s_Data->QuadInstanceBufferPtr = s_Data->QuadInstanceBufferBase;
	}

	void Renderer2D::EndInstancedSet() {
		if (s_Data->QuadInstanceCount != 0) {
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
			FlushInstancedQuads();
		}
	}

	void Renderer2D::SubmitTransparentQuad(const glm::mat3& transform, const glm::vec2& size,
	                                       const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs,
	                                       uint8_t depth, bool flipX, bool flipY, bool flatColored) {
		if (!flipX && !flipY) {
			s_Data->TransparentQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				static_cast<glm::vec4>(uvs),
				depth
			);
		}
		else if (flipX && !flipY) {
			s_Data->TransparentQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
				depth
			);
		}
		else if (!flipX) {
			s_Data->TransparentQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
				depth
			);
		}
		else {
			s_Data->TransparentQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmax.y, uvs.UVmin.x, uvs.UVmin.y},
				depth
			);
		}
	}

	void Renderer2D::SubmitOpaqueQuad(const glm::mat3& transform, const glm::vec2& size, const glm::vec4& tintColor,
	                                  Texture2D* texture, const UVs& uvs, uint8_t depth, bool flipX, bool flipY,
	                                  bool flatColored) {
		if (!flipX && !flipY) {
			s_Data->OpaqueQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				static_cast<glm::vec4>(uvs),
				depth
			);
		}
		else if (flipX && !flipY) {
			s_Data->OpaqueQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
				depth
			);
		}
		else if (!flipX) {
			s_Data->OpaqueQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
				depth
			);
		}
		else {
			s_Data->OpaqueQuadQueue.emplace_back(
				transform,
				size,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmax.y, uvs.UVmin.x, uvs.UVmin.y},
				depth
			);
		}
	}

	Renderer2D::Statistics Renderer2D::GetStatistics() {
		return s_Data->Stats;
	}

	// maybe add tiling factor????
	// maybe tiling factor should be n in 2^n?? and an int??, so pixelart would look nice

	void Renderer2D::DrawQuadInstanced(const QuadInstance& quad) {
		if (CORI_CORE_ASSERT_ERROR(quad.m_Texture, "Texture is nullptr, trying to avoid read access violation")) {
			return;
		}
		if (s_Data->QuadInstanceCount >= s_Data->MaxInstanceCount) {
			StartNewInstancedSet();
		}

		if (s_Data->NecessaryTexture != quad.m_Texture) {
			if (!s_Data->NecessaryTexture) {
				s_Data->NecessaryTexture = quad.m_Texture;
			}
			else {
				StartNewInstancedSet();
				s_Data->NecessaryTexture = quad.m_Texture;
			}
		}

		s_Data->QuadInstanceBufferPtr->m_Transform = quad.m_Transform;
		s_Data->QuadInstanceBufferPtr->m_TexturePosition = quad.m_UVs;
		s_Data->QuadInstanceBufferPtr->m_Size = quad.m_Size;
		s_Data->QuadInstanceBufferPtr->m_TintColor = quad.m_TintColor;
		s_Data->QuadInstanceBufferPtr->m_Layer = quad.m_Layer;
		s_Data->QuadInstanceBufferPtr++;


		s_Data->QuadInstanceCount++;
	}

	void Renderer2D::StartNewInstancedSet() {
		EndInstancedSet();
		BeginInstancedSet();
	}


	void Renderer2D::FlushQueues() {
		CORI_PROFILE_FUNCTION();

		if (!s_Data->OpaqueQuadQueue.empty()) {
			CORI_PROFILE_SCOPE("Opaque instanced quad flush");

			{
				CORI_PROFILE_SCOPE("Opaque instanced quad texture sort");
				ska_sort(
					s_Data->OpaqueQuadQueue.begin(),
					s_Data->OpaqueQuadQueue.end(),
					[](const QuadInstance& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.m_Texture);
					}
				);
			}

			{
				CORI_PROFILE_SCOPE("Opaque instanced quad draw");
				BeginInstancedSet();

				for (const auto& quad : s_Data->OpaqueQuadQueue) {
					DrawQuadInstanced(quad);
				}

				EndInstancedSet();
			}

			s_Data->OpaqueQuadQueue.clear();
		}

		if (!s_Data->TransparentQuadQueue.empty()) {
			CORI_PROFILE_SCOPE("Transparent instanced quad flush");

			{
				CORI_PROFILE_SCOPE("Transparent instanced quad layer sort");
				ska_sort(
					s_Data->TransparentQuadQueue.begin(),
					s_Data->TransparentQuadQueue.end(),
					[](const QuadInstance& quad) -> uint8_t {
						return quad.m_Layer;
					}
				);
			}

			{
				CORI_PROFILE_SCOPE("Transparent instanced quad texture sort");
				ska_sort(
					s_Data->TransparentQuadQueue.begin(),
					s_Data->TransparentQuadQueue.end(),
					[](const QuadInstance& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.m_Texture);
					}
				);
			}

			GraphicsCall::EnableBlending();
			GraphicsCall::SetDepthMask(false);

			{
				CORI_PROFILE_SCOPE("Transparent instanced quad draw");
				BeginInstancedSet();

				for (const auto& quad : s_Data->TransparentQuadQueue) {
					DrawQuadInstanced(quad);
				}

				EndInstancedSet();
			}

			GraphicsCall::SetDepthMask(true);
			GraphicsCall::DisableBlending();

			s_Data->TransparentQuadQueue.clear();
		}
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

		GraphicsCall::DrawElementsInstancedTriangles(s_Data->QuadInstanceCount);

		s_Data->Stats.DrawCalls++;
		s_Data->Stats.QuadCount += s_Data->QuadInstanceCount;

		s_Data->QuadInstanceCount = 0;
	}
}
