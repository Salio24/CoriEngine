#include "Renderer2D.hpp"
#include <ska_sort.hpp>
#include "Core/Utility/AABB.hpp"
#include "AssetManager/AssetManager.hpp"

namespace Cori {
	Renderer2D::RendererData* Renderer2D::s_Data{ nullptr };

	void Renderer2D::Init() {
		CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Initializing Renderer2D.");

		s_Data = new RendererData();

		s_Data->WhiteTexture = Texture2D::Create("assets/engine/textures/white1x1.png");

		s_Data->QuadInstanceVertexArray = VertexArray::Create();
		s_Data->QuadInstanceVertexBuffer = VertexBuffer::Create();
		s_Data->QuadInstanceVertexBuffer->SetLayout({
				{ShaderDataType::Mat3, "a_Transform", 1},
				{ShaderDataType::Vec4, "a_TexturePosition", 1},
				{ShaderDataType::Vec2, "a_Size", 1},
				{ShaderDataType::Vec4, "a_TintColor", 1},
				{ShaderDataType::Float, "a_Layer", 1},
			});

		s_Data->QuadInstanceVertexBuffer->Init(nullptr, s_Data->MaxInstanceCount * s_Data->QuadInstanceVertexBuffer->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
		s_Data->QuadInstanceVertexArray->AddVertexBuffer(s_Data->QuadInstanceVertexBuffer);

		uint32_t quadIndices[6] = { 0, 1, 2, 2, 3, 0 };
		s_Data->QuadInstanceIndexBuffer = IndexBuffer::Create(quadIndices, 6);
		s_Data->QuadInstanceVertexArray->AddIndexBuffer(s_Data->QuadInstanceIndexBuffer);

		s_Data->QuadInstanceBufferBase = new Quad[s_Data->MaxInstanceCount];

		s_Data->QuadInstanceShader = ShaderProgram::Create("assets/engine/shaders/QuadInstancedVert.glsl", "assets/engine/shaders/QuadInstancedFrag.glsl");

		CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Renderer2D Initialized successfully.");
	}


	void Renderer2D::Shutdown() {
		CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Shutting down Renderer2D.");

		delete[] s_Data->QuadInstanceBufferBase;
		delete s_Data;
	}

	void Renderer2D::BeginScene(const Components::Scene::Camera& camera) {
		CORI_PROFILE_FUNCTION();

		s_Data->WorldViewProjectionMatrix = camera.m_ViewProjectionMatrix;
		s_Data->ScreenSpaceViewProjectionMatrix = camera.m_ProjectionMatrix;

		s_Data->Stats.DrawCalls = 0;
		s_Data->Stats.QuadCount = 0;
	}

	void Renderer2D::BeginWorldPass() {
		s_Data->CurrentViewProjectionMatrix = s_Data->WorldViewProjectionMatrix;
	}

	void Renderer2D::BeginScreenSpacePass() {
		s_Data->CurrentViewProjectionMatrix = s_Data->ScreenSpaceViewProjectionMatrix;
	}

	void Renderer2D::EndScene() {
		CORI_PROFILE_FUNCTION();
	}

	void Renderer2D::DrawScene(Scene* scene) {
		CORI_PROFILE_FUNCTION();

		{
			CORI_PROFILE_SCOPE("Quad Submission");
			const auto& camera = scene->GetContextComponent<Components::Scene::Camera>();
			//Utility::AABB cameraBounds = { camera.m_CameraMinBound, camera.m_CameraMaxBound };
			EntityView view = scene->View<Components::Entity::QuadRenderer, Components::Entity::Transform>(Exclude<Components::Entity::InactiveLocallyFlag>());
			for (const auto entity : view) {
				auto& renderer = view.Get<Components::Entity::QuadRenderer>(entity);
				if (renderer.m_Visible) {
					auto& transform = view.Get<Components::Entity::Transform>(entity);
					Utility::AABB entityBounds = Utility::CalculateAABB(transform.m_WorldTransform, renderer.GetHalfSize());
					//SubmitAABB(camera.m_CameraBounds, 0.2f, {0.0f, 1.0f, 0.0f});
					if (AABBOverlapCheck(camera.m_CameraBounds, entityBounds)) {
						//SubmitAABB(entityBounds, 0.2f, {1.0f, 0.0f, 1.0f});
						if (renderer.GetSemiTransparencyState()) {
							SubmitWorldSpaceTransparentQuad(transform.m_WorldTransform, renderer.GetHalfSize(), renderer.GetColor(), renderer.GetTexture().get(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
							continue;
						}
						SubmitWorldSpaceOpaqueQuad(transform.m_WorldTransform, renderer.GetHalfSize(), renderer.GetColor(), renderer.GetTexture().get(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
					}
				}
			}
		}
	}

	void Renderer2D::FlushRenderQueues() {
		BeginWorldPass();
		FlushWorldQueues();
		BeginScreenSpacePass();
		FlushScreenSpaceQueues();
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

	void Renderer2D::SubmitWorldSpaceTransparentQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
		if (!flipX && !flipY) {
			s_Data->WorldSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				static_cast<glm::vec4>(uvs),
				depth
			);
		}
		else if (flipX && !flipY) {
			s_Data->WorldSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
				depth
			);
		}
		else if (!flipX) {
			s_Data->WorldSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
				depth
			);
		}
		else {
			s_Data->WorldSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmax.y, uvs.UVmin.x, uvs.UVmin.y},
				depth
			);
		}
	}

	void Renderer2D::SubmitWorldSpaceOpaqueQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
		if (!flipX && !flipY) {
			s_Data->WorldSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				static_cast<glm::vec4>(uvs),
				depth
			);
		}
		else if (flipX && !flipY) {
			s_Data->WorldSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
				depth
			);
		}
		else if (!flipX) {
			s_Data->WorldSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
				depth
			);
		}
		else {
			s_Data->WorldSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmax.y, uvs.UVmin.x, uvs.UVmin.y},
				depth
			);
		}
	}

	void Renderer2D::SubmitScreenSpaceTransparentQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
		if (!flipX && !flipY) {
			s_Data->ScreenSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				static_cast<glm::vec4>(uvs),
				depth
			);
		}
		else if (flipX && !flipY) {
			s_Data->ScreenSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
				depth
			);
		}
		else if (!flipX) {
			s_Data->ScreenSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
				depth
			);
		}
		else {
			s_Data->ScreenSpaceTransparentQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmax.y, uvs.UVmin.x, uvs.UVmin.y},
				depth
			);
		}
	}

	void Renderer2D::SubmitScreenSpaceOpaqueQuad(const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
		if (!flipX && !flipY) {
			s_Data->ScreenSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				static_cast<glm::vec4>(uvs),
				depth
			);
		}
		else if (flipX && !flipY) {
			s_Data->ScreenSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
				depth
			);
		}
		else if (!flipX) {
			s_Data->ScreenSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
				depth
			);
		}
		else {
			s_Data->ScreenSpaceOpaqueQuadQueue.emplace_back(
				transform,
				halfSize,
				tintColor,
				flatColored ? s_Data->WhiteTexture.get() : texture,
				glm::vec4{uvs.UVmax.x, uvs.UVmax.y, uvs.UVmin.x, uvs.UVmin.y},
				depth
			);
		}
	}

	void Renderer2D::SubmitScreenSpaceColoredQuad(const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color) {
		const glm::mat3 transform = glm::translate(glm::mat3(1.0f), position);
		SubmitScreenSpaceOpaqueQuad(transform, halfSize, glm::vec4(color, 1.0f), nullptr, {}, 30, false, false, true);
	}

	void Renderer2D::SubmitWorldSpaceColoredQuad(const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color) {
		const glm::mat3 transform = glm::translate(glm::mat3(1.0f), position);
		SubmitWorldSpaceOpaqueQuad(transform, halfSize, glm::vec4(color, 1.0f), nullptr, {}, 30, false, false, true);
	}

	void Renderer2D::SubmitAABB(const Utility::AABB& aabb, const float lineThickness, const glm::vec3& color) {
		const glm::vec2 size = {aabb.m_Max.x - aabb.m_Min.x, aabb.m_Max.y - aabb.m_Min.y};
		SubmitWorldSpaceColoredQuad({aabb.m_Min.x, aabb.m_Max.y - size.y / 2.0f}, {lineThickness, size.y / 2.0f}, color);
		SubmitWorldSpaceColoredQuad({aabb.m_Max.x, aabb.m_Max.y - size.y / 2.0f}, {lineThickness, size.y / 2.0f}, color);


		SubmitWorldSpaceColoredQuad({aabb.m_Max.x - size.x / 2.0f, aabb.m_Min.y}, {size.x / 2.0f - lineThickness, lineThickness}, color);
		SubmitWorldSpaceColoredQuad({aabb.m_Max.x - size.x / 2.0f, aabb.m_Max.y}, {size.x / 2.0f - lineThickness, lineThickness}, color);

	}

	Renderer2D::Statistics Renderer2D::GetStatistics() {
		return s_Data->Stats;
	}

	// maybe add tiling factor????
	// maybe tiling factor should be n in 2^n?? and an int32_t??, so pixelart would look nice

	void Renderer2D::DrawQuadInstanced(const QuadInstance& quad) {
		if (!quad.m_Texture) {
			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "DrawQuadInstanced: Texture is nullptr, trying to avoid read access violation");
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


	void Renderer2D::FlushWorldQueues() {
		CORI_PROFILE_FUNCTION();

		if (!s_Data->WorldSpaceOpaqueQuadQueue.empty()) {
			CORI_PROFILE_SCOPE("Opaque instanced quad flush");

			{
				CORI_PROFILE_SCOPE("Opaque instanced quad texture sort");
				ska_sort(
					s_Data->WorldSpaceOpaqueQuadQueue.begin(),
					s_Data->WorldSpaceOpaqueQuadQueue.end(),
					[](const QuadInstance& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.m_Texture);
					}
				);
			}

			{
				CORI_PROFILE_SCOPE("Opaque instanced quad draw");
				BeginInstancedSet();

				for (const auto& quad : s_Data->WorldSpaceOpaqueQuadQueue) {
					DrawQuadInstanced(quad);
				}

				EndInstancedSet();
			}

			s_Data->WorldSpaceOpaqueQuadQueue.clear();
		}

		if (!s_Data->WorldSpaceTransparentQuadQueue.empty()) {
			CORI_PROFILE_SCOPE("Transparent instanced quad flush");

			{
				CORI_PROFILE_SCOPE("Transparent instanced quad layer sort");
				ska_sort(
					s_Data->WorldSpaceTransparentQuadQueue.begin(),
					s_Data->WorldSpaceTransparentQuadQueue.end(),
					[](const QuadInstance& quad) -> uint8_t {
						return quad.m_Layer;
					}
				);
			}

			{
				CORI_PROFILE_SCOPE("Transparent instanced quad texture sort");
				ska_sort(
					s_Data->WorldSpaceTransparentQuadQueue.begin(),
					s_Data->WorldSpaceTransparentQuadQueue.end(),
					[](const QuadInstance& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.m_Texture);
					}
				);
			}

			API::EnableBlending();
			API::SetDepthMask(false);

			{
				CORI_PROFILE_SCOPE("Transparent instanced quad draw");
				BeginInstancedSet();

				for (const auto& quad : s_Data->WorldSpaceTransparentQuadQueue) {
					DrawQuadInstanced(quad);
				}

				EndInstancedSet();
			}

			API::SetDepthMask(true);
			API::DisableBlending();

			s_Data->WorldSpaceTransparentQuadQueue.clear();
		}
	}

	void Renderer2D::FlushScreenSpaceQueues() {
		CORI_PROFILE_FUNCTION();

		if (!s_Data->ScreenSpaceOpaqueQuadQueue.empty()) {
			CORI_PROFILE_SCOPE("Opaque screen space instanced quad flush");

			{
				CORI_PROFILE_SCOPE("Opaque screen space instanced quad texture sort");
				ska_sort(
					s_Data->ScreenSpaceOpaqueQuadQueue.begin(),
					s_Data->ScreenSpaceOpaqueQuadQueue.end(),
					[](const QuadInstance& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.m_Texture);
					}
				);
			}

			{
				CORI_PROFILE_SCOPE("Opaque screen space instanced quad draw");
				BeginInstancedSet();

				for (const auto& quad : s_Data->ScreenSpaceOpaqueQuadQueue) {
					DrawQuadInstanced(quad);
				}

				EndInstancedSet();
			}

			s_Data->ScreenSpaceOpaqueQuadQueue.clear();
		}

		if (!s_Data->ScreenSpaceTransparentQuadQueue.empty()) {
			CORI_PROFILE_SCOPE("Transparent screen space instanced quad flush");

			{
				CORI_PROFILE_SCOPE("Transparent screen space instanced quad layer sort");
				ska_sort(
					s_Data->ScreenSpaceTransparentQuadQueue.begin(),
					s_Data->ScreenSpaceTransparentQuadQueue.end(),
					[](const QuadInstance& quad) -> uint8_t {
						return quad.m_Layer;
					}
				);
			}

			{
				CORI_PROFILE_SCOPE("Transparent screen space instanced quad texture sort");
				ska_sort(
					s_Data->ScreenSpaceTransparentQuadQueue.begin(),
					s_Data->ScreenSpaceTransparentQuadQueue.end(),
					[](const QuadInstance& quad) -> uint64_t {
						return reinterpret_cast<uint64_t>(quad.m_Texture);
					}
				);
			}

			API::EnableBlending();
			API::SetDepthMask(false);

			{
				CORI_PROFILE_SCOPE("Transparent screen space instanced quad draw");
				BeginInstancedSet();

				for (const auto& quad : s_Data->ScreenSpaceTransparentQuadQueue) {
					DrawQuadInstanced(quad);
				}

				EndInstancedSet();
			}

			API::SetDepthMask(true);
			API::DisableBlending();

			s_Data->ScreenSpaceTransparentQuadQueue.clear();
		}
	}


	void Renderer2D::FlushInstancedQuads() {
		CORI_PROFILE_FUNCTION();

		const auto size = reinterpret_cast<uint8_t*>(s_Data->QuadInstanceBufferPtr) - reinterpret_cast<uint8_t*>(s_Data->QuadInstanceBufferBase);
		s_Data->QuadInstanceVertexBuffer->SetData(s_Data->QuadInstanceBufferBase, size);

		s_Data->QuadInstanceShader->SetMat4("u_ViewProjection", s_Data->CurrentViewProjectionMatrix);
		s_Data->QuadInstanceShader->SetInt("u_Texture", 0);

		if (s_Data->CurrentTexture != s_Data->NecessaryTexture) {
			s_Data->NecessaryTexture->Bind(0);
			s_Data->CurrentTexture = s_Data->NecessaryTexture;
		}

		API::DrawElementsInstancedTriangles(s_Data->QuadInstanceCount);

		s_Data->Stats.DrawCalls++;
		s_Data->Stats.QuadCount += s_Data->QuadInstanceCount;

		s_Data->QuadInstanceCount = 0;
	}
}
