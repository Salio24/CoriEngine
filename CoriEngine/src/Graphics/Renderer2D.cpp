#include "Renderer2D.hpp"
#include <ska_sort.hpp>
#include "Core/Utility/AABB.hpp"
#include "AssetManager/AssetManager.hpp"
#include "FontData.hpp"

namespace Cori {
	namespace Graphics {
		Renderer2D::RendererData* Renderer2D::s_Data{ nullptr };

		void Renderer2D::Init() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Initializing Renderer2D.");

			s_Data = new RendererData();

			const auto image = Image::Create("assets/engine/textures/white1x1.png");
			s_Data->WhiteTexture = Texture2D::Create(image);

			// quad setup

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

			s_Data->WorldSpaceTransparentQuadQueue.reserve(s_Data->WorldSpaceTransparentQuadQueueInitialSize);
			s_Data->WorldSpaceOpaqueQuadQueue.reserve(s_Data->WorldSpaceOpaqueQuadQueueInitialSize);

			s_Data->ScreenSpaceTransparentQuadQueue.reserve(s_Data->ScreenSpaceTransparentQuadQueueInitialSize);
			s_Data->ScreenSpaceOpaqueQuadQueue.reserve(s_Data->ScreenSpaceOpaqueQuadQueueInitialSize);

			// text setup

			s_Data->CharInstanceVertexArray = VertexArray::Create();
			s_Data->CharInstanceVertexBuffer = VertexBuffer::Create();
			s_Data->CharInstanceVertexBuffer->SetLayout({
					{ShaderDataType::Mat3, "a_Transform", 1},
					{ShaderDataType::Vec4, "a_TexturePosition", 1},
					{ShaderDataType::Vec2, "a_Size", 1},
					{ShaderDataType::Vec4, "a_TintColor", 1},
					{ShaderDataType::Float, "a_Layer", 1},
				});

			s_Data->CharInstanceVertexBuffer->Init(nullptr, s_Data->MaxInstanceCount * s_Data->CharInstanceVertexBuffer->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
			s_Data->CharInstanceVertexArray->AddVertexBuffer(s_Data->CharInstanceVertexBuffer);

			s_Data->CharInstanceIndexBuffer = IndexBuffer::Create(quadIndices, 6);
			s_Data->CharInstanceVertexArray->AddIndexBuffer(s_Data->CharInstanceIndexBuffer);

			s_Data->CharInstanceBufferBase = new Char[s_Data->MaxInstanceCount];

			s_Data->CharInstanceShader = ShaderProgram::Create("assets/engine/shaders/TextInstancedVert.glsl", "assets/engine/shaders/TextInstancedFrag.glsl");

			s_Data->WorldSpaceTransparentCharQueue.reserve(s_Data->WorldSpaceTransparentCharQueueInitialSize);
			s_Data->WorldSpaceOpaqueCharQueue.reserve(s_Data->WorldSpaceOpaqueCharQueueInitialSize);

			s_Data->ScreenSpaceTransparentCharQueue.reserve(s_Data->ScreenSpaceTransparentCharQueueInitialSize);
			s_Data->ScreenSpaceOpaqueCharQueue.reserve(s_Data->ScreenSpaceOpaqueCharQueueInitialSize);
			
			s_Data->TestFont = Font::Create("surely/invalid/path.lol", { Font::CharsetRanges::Latin, Font::CharsetRanges::LatinExtendedA, Font::CharsetRanges::LatinExtendedB });

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Renderer2D Initialized successfully.");
		}

		void Renderer2D::Shutdown() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Shutting down Renderer2D.");

			delete[] s_Data->QuadInstanceBufferBase;
			delete s_Data;
		}

		void Renderer2D::Test() {
			SubmitQuad(SCREEN_SPACE, OPAQUE, glm::mat3(1.0f), glm::vec2(1.0f), glm::vec4(1.0f), s_Data->TestFont->GetData()->m_Atlas.get(), UVs{}, 5, false, false, false);
		}

		void Renderer2D::BeginScene(const Components::Scene::Camera& camera) {
			CORI_PROFILE_FUNCTION();

			s_Data->WorldSpaceViewProjectionMatrix = camera.m_ViewProjectionMatrix;
			s_Data->ScreenSpaceViewProjectionMatrix = camera.m_ProjectionMatrix;

			s_Data->Stats.DrawCalls = 0;
			s_Data->Stats.QuadCount = 0;
		}

		void Renderer2D::BeginWorldSpacePass() {
			if (s_Data->CurrentDrawSpace != WORLD_SPACE) {
				s_Data->CurrentViewProjectionMatrix = s_Data->WorldSpaceViewProjectionMatrix;
				s_Data->CurrentDrawSpace = WORLD_SPACE;
			}
		}

		void Renderer2D::BeginScreenSpacePass() {
			if (s_Data->CurrentDrawSpace != SCREEN_SPACE) {
				s_Data->CurrentViewProjectionMatrix = s_Data->ScreenSpaceViewProjectionMatrix;
				s_Data->CurrentDrawSpace = SCREEN_SPACE;
			}
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
						if (renderer.GetTexture()) {
							auto& transform = view.Get<Components::Entity::Transform>(entity);
							Utility::AABB entityBounds = Utility::CalculateAABB(transform.m_WorldTransform, renderer.GetHalfSize());
							//SubmitAABB(camera.m_CameraBounds, 0.2f, {0.0f, 1.0f, 0.0f});
							if (AABBOverlapCheck(camera.m_CameraBounds, entityBounds)) {
								//SubmitAABB(entityBounds, 0.2f, {1.0f, 0.0f, 1.0f});
								if (renderer.GetSemiTransparencyState()) {
									SubmitQuad(WORLD_SPACE, SEMI_TRANSPARENT, transform.m_WorldTransform, renderer.GetHalfSize(), renderer.GetColor(), renderer.GetTexture().get(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
									continue;
								}
								SubmitQuad(WORLD_SPACE, SEMI_TRANSPARENT, transform.m_WorldTransform, renderer.GetHalfSize(), renderer.GetColor(), renderer.GetTexture().get(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
							}
						} else {
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "DrawScene: Texture inside Quad Renderer for Entity '{}', is null, skipping it.", entity.GetDebugData());
						}
					}
				}
			}
		}

		void Renderer2D::FlushRenderQueues() {
			BeginWorldSpacePass();
			FlushOpaqueQueues();
			BeginScreenSpacePass();
			FlushTransparentQueues();
		}

		void Renderer2D::SubmitQuad(const DrawSpace space, const ObjectTransparency transparencyMode, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
			if (space == WORLD_SPACE) {
				if (transparencyMode == OPAQUE) {
					SubmitQuadToQueue(s_Data->WorldSpaceOpaqueQuadQueue, transform, halfSize, tintColor, texture, uvs, depth, flipX, flipY, flatColored);
					return;
				}

				SubmitQuadToQueue(s_Data->WorldSpaceTransparentQuadQueue, transform, halfSize, tintColor, texture, uvs, depth, flipX, flipY, flatColored);
				return;
			}

			if (transparencyMode == OPAQUE) {
				SubmitQuadToQueue(s_Data->ScreenSpaceOpaqueQuadQueue, transform, halfSize, tintColor, texture, uvs, depth, flipX, flipY, flatColored);
				return;
			}

			SubmitQuadToQueue(s_Data->ScreenSpaceTransparentQuadQueue, transform, halfSize, tintColor, texture, uvs, depth, flipX, flipY, flatColored);
		}

		void Renderer2D::BeginInstancedSet() {
			CORI_PROFILE_FUNCTION();

			s_Data->QuadInstanceCount = 0;
			s_Data->QuadInstanceBufferPtr = s_Data->QuadInstanceBufferBase;

			s_Data->CharInstanceCount = 0;
			s_Data->CharInstanceBufferPtr = s_Data->CharInstanceBufferBase;

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

			if (s_Data->CharInstanceCount != 0) {
				if (s_Data->CurrentVertexArray != s_Data->CharInstanceVertexArray.get()) {
					s_Data->CharInstanceVertexArray->Bind();
					s_Data->CurrentVertexArray = s_Data->CharInstanceVertexArray.get();
				}
				if (s_Data->CurrentVertexBuffer != s_Data->CharInstanceVertexBuffer.get()) {
					s_Data->CharInstanceVertexBuffer->Bind();
					s_Data->CurrentVertexBuffer = s_Data->CharInstanceVertexBuffer.get();
				}
				if (s_Data->CurrentIndexBuffer != s_Data->CharInstanceIndexBuffer.get()) {
					s_Data->CharInstanceIndexBuffer->Bind();
					s_Data->CurrentIndexBuffer = s_Data->CharInstanceIndexBuffer.get();
				}
				if (s_Data->CurrentShader != s_Data->CharInstanceShader.get()) {
					s_Data->CharInstanceShader->Bind();
					s_Data->CurrentShader = s_Data->CharInstanceShader.get();
				}
				FlushInstancedChars();
			}
		}

		void Renderer2D::SubmitQuadToQueue(std::vector<QuadInstance>& queue, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, Texture2D* texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
			if (!flipX && !flipY) {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture.get() : texture,
					static_cast<glm::vec4>(uvs),
					depth
				);
			}
			else if (flipX && !flipY) {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture.get() : texture,
					glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
					depth
				);
			}
			else if (!flipX) {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture.get() : texture,
					glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
					depth
				);
			}
			else {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture.get() : texture,
					glm::vec4{uvs.UVmax.x, uvs.UVmax.y, uvs.UVmin.x, uvs.UVmin.y},
					depth
				);
			}
		}


		void Renderer2D::SubmitColoredQuad(const DrawSpace space, const glm::vec2 position, const glm::vec2 halfSize, const glm::vec3& color) {
			const glm::mat3 transform = glm::translate(glm::mat3(1.0f), position);
			SubmitQuad(space, OPAQUE, transform, halfSize, glm::vec4(color, 1.0f), nullptr, {}, 30, false, false, true);
		}

		void Renderer2D::SubmitAABB(const Utility::AABB& aabb, const float lineThickness, const glm::vec3& color) {
			const glm::vec2 size = {aabb.m_Max.x - aabb.m_Min.x, aabb.m_Max.y - aabb.m_Min.y};
			SubmitColoredQuad(WORLD_SPACE, {aabb.m_Min.x, aabb.m_Max.y - size.y / 2.0f}, {lineThickness, size.y / 2.0f}, color);
			SubmitColoredQuad(WORLD_SPACE, {aabb.m_Max.x, aabb.m_Max.y - size.y / 2.0f}, {lineThickness, size.y / 2.0f}, color);

			SubmitColoredQuad(WORLD_SPACE, {aabb.m_Max.x - size.x / 2.0f, aabb.m_Min.y}, {size.x / 2.0f - lineThickness, lineThickness}, color);
			SubmitColoredQuad(WORLD_SPACE, {aabb.m_Max.x - size.x / 2.0f, aabb.m_Max.y}, {size.x / 2.0f - lineThickness, lineThickness}, color);
		}

		void Renderer2D::SubmitTextToQueue(std::vector<CharInstance>& queue, const glm::mat3& transform, const std::string& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const float lineSpacing, const float kerning) {

		}

		void Renderer2D::SubmitText(const DrawSpace space, const glm::mat3& transform, const std::string& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const float lineSpacing, const float kerning) {
			if (space == WORLD_SPACE) {
				if (color.a == 1.0f) {
					SubmitTextToQueue(s_Data->WorldSpaceOpaqueCharQueue, transform, text, color, font, lineSpacing, kerning);
					return;
				}
				SubmitTextToQueue(s_Data->WorldSpaceTransparentCharQueue, transform, text, color, font, lineSpacing, kerning);
				return;
			}

			if (color.a == 1.0f) {
				SubmitTextToQueue(s_Data->ScreenSpaceOpaqueCharQueue, transform, text, color, font, lineSpacing, kerning);
				return;
			}

			SubmitTextToQueue(s_Data->ScreenSpaceTransparentCharQueue, transform, text, color, font, lineSpacing, kerning);
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

		void Renderer2D::DrawCharInstanced(const CharInstance& instance) {
			if (!instance.m_Texture) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "DrawCharInstanced: Texture is nullptr, trying to avoid read access violation");
				return;
			}

			if (s_Data->CharInstanceCount >= s_Data->MaxInstanceCount) {
				StartNewInstancedSet();
			}

			if (s_Data->NecessaryTexture != instance.m_Texture) {
				if (!s_Data->NecessaryTexture) {
					s_Data->NecessaryTexture = instance.m_Texture;
				}
				else {
					StartNewInstancedSet();
					s_Data->NecessaryTexture = instance.m_Texture;
				}
			}

			s_Data->CharInstanceBufferPtr->m_Transform = instance.m_Transform;
			s_Data->CharInstanceBufferPtr->m_TexturePosition = instance.m_UVs;
			s_Data->CharInstanceBufferPtr->m_Size = instance.m_Size;
			s_Data->CharInstanceBufferPtr->m_TintColor = instance.m_TintColor;
			s_Data->CharInstanceBufferPtr->m_Layer = instance.m_Layer;
			s_Data->CharInstanceBufferPtr++;
			
			s_Data->CharInstanceCount++;
		}

		void Renderer2D::StartNewInstancedSet() {
			EndInstancedSet();
			BeginInstancedSet();
		}


		void Renderer2D::FlushOpaqueQueues() {
			CORI_PROFILE_FUNCTION();

			if (!s_Data->WorldSpaceOpaqueQuadQueue.empty()) {
				CORI_PROFILE_SCOPE("Opaque instanced quad flush");

				BeginWorldSpacePass();

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

			if (!s_Data->WorldSpaceOpaqueCharQueue.empty()) {
				CORI_PROFILE_SCOPE("Opaque instanced Char flush");

				BeginWorldSpacePass();

				{
					CORI_PROFILE_SCOPE("Opaque instanced Char texture sort");
					ska_sort(
						s_Data->WorldSpaceOpaqueCharQueue.begin(),
						s_Data->WorldSpaceOpaqueCharQueue.end(),
						[](const CharInstance& Char) -> uint64_t {
							return reinterpret_cast<uint64_t>(Char.m_Texture);
						}
					);
				}

				{
					CORI_PROFILE_SCOPE("Opaque instanced Char draw");
					BeginInstancedSet();

					for (const auto& Char : s_Data->WorldSpaceOpaqueCharQueue) {
						DrawCharInstanced(Char);
					}

					EndInstancedSet();
				}

				s_Data->WorldSpaceOpaqueCharQueue.clear();
			}
			
			if (!s_Data->ScreenSpaceOpaqueQuadQueue.empty()) {
				CORI_PROFILE_SCOPE("Opaque screen space instanced quad flush");

				BeginScreenSpacePass();

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


			if (!s_Data->ScreenSpaceOpaqueCharQueue.empty()) {
				CORI_PROFILE_SCOPE("Opaque screen space instanced Char flush");

				BeginScreenSpacePass();

				{
					CORI_PROFILE_SCOPE("Opaque screen space instanced Char texture sort");
					ska_sort(
						s_Data->ScreenSpaceOpaqueCharQueue.begin(),
						s_Data->ScreenSpaceOpaqueCharQueue.end(),
						[](const CharInstance& Char) -> uint64_t {
							return reinterpret_cast<uint64_t>(Char.m_Texture);
						}
					);
				}

				{
					CORI_PROFILE_SCOPE("Opaque screen space instanced Char draw");
					BeginInstancedSet();

					for (const auto& Char : s_Data->ScreenSpaceOpaqueCharQueue) {
						DrawCharInstanced(Char);
					}

					EndInstancedSet();
				}

				s_Data->ScreenSpaceOpaqueCharQueue.clear();
			}
		}

		void Renderer2D::FlushTransparentQueues() {
			if (!s_Data->WorldSpaceTransparentQuadQueue.empty()) {
				CORI_PROFILE_SCOPE("Transparent instanced quad flush");

				BeginWorldSpacePass();

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

			if (!s_Data->WorldSpaceTransparentCharQueue.empty()) {
				CORI_PROFILE_SCOPE("Transparent instanced Char flush");

				BeginWorldSpacePass();

				{
					CORI_PROFILE_SCOPE("Transparent instanced Char layer sort");
					ska_sort(
						s_Data->WorldSpaceTransparentCharQueue.begin(),
						s_Data->WorldSpaceTransparentCharQueue.end(),
						[](const CharInstance& Char) -> uint8_t {
							return Char.m_Layer;
						}
					);
				}

				{
					CORI_PROFILE_SCOPE("Transparent instanced Char texture sort");
					ska_sort(
						s_Data->WorldSpaceTransparentCharQueue.begin(),
						s_Data->WorldSpaceTransparentCharQueue.end(),
						[](const CharInstance& Char) -> uint64_t {
							return reinterpret_cast<uint64_t>(Char.m_Texture);
						}
					);
				}

				API::EnableBlending();
				API::SetDepthMask(false);

				{
					CORI_PROFILE_SCOPE("Transparent instanced Char draw");
					BeginInstancedSet();

					for (const auto& Char : s_Data->WorldSpaceTransparentCharQueue) {
						DrawCharInstanced(Char);
					}

					EndInstancedSet();
				}

				API::SetDepthMask(true);
				API::DisableBlending();

				s_Data->WorldSpaceTransparentCharQueue.clear();
			}

			if (!s_Data->ScreenSpaceTransparentQuadQueue.empty()) {
				CORI_PROFILE_SCOPE("Transparent screen space instanced quad flush");

				BeginScreenSpacePass();

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

			if (!s_Data->ScreenSpaceTransparentCharQueue.empty()) {
				CORI_PROFILE_SCOPE("Transparent screen space instanced Char flush");

				BeginScreenSpacePass();

				{
					CORI_PROFILE_SCOPE("Transparent screen space instanced Char layer sort");
					ska_sort(
						s_Data->ScreenSpaceTransparentCharQueue.begin(),
						s_Data->ScreenSpaceTransparentCharQueue.end(),
						[](const CharInstance& Char) -> uint8_t {
							return Char.m_Layer;
						}
					);
				}

				{
					CORI_PROFILE_SCOPE("Transparent screen space instanced Char texture sort");
					ska_sort(
						s_Data->ScreenSpaceTransparentCharQueue.begin(),
						s_Data->ScreenSpaceTransparentCharQueue.end(),
						[](const CharInstance& Char) -> uint64_t {
							return reinterpret_cast<uint64_t>(Char.m_Texture);
						}
					);
				}

				API::EnableBlending();
				API::SetDepthMask(false);

				{
					CORI_PROFILE_SCOPE("Transparent screen space instanced Char draw");
					BeginInstancedSet();

					for (const auto& Char : s_Data->ScreenSpaceTransparentCharQueue) {
						DrawCharInstanced(Char);
					}

					EndInstancedSet();
				}

				API::SetDepthMask(true);
				API::DisableBlending();

				s_Data->ScreenSpaceTransparentCharQueue.clear();
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

		void Renderer2D::FlushInstancedChars() {
			CORI_PROFILE_FUNCTION();

			const auto size = reinterpret_cast<uint8_t*>(s_Data->CharInstanceBufferPtr) - reinterpret_cast<uint8_t*>(s_Data->CharInstanceBufferBase);
			s_Data->CharInstanceVertexBuffer->SetData(s_Data->CharInstanceBufferBase, size);

			s_Data->CharInstanceShader->SetMat4("u_ViewProjection", s_Data->CurrentViewProjectionMatrix);
			s_Data->CharInstanceShader->SetInt("u_Texture", 0);

			if (s_Data->CurrentTexture != s_Data->NecessaryTexture) {
				s_Data->NecessaryTexture->Bind(0);
				s_Data->CurrentTexture = s_Data->NecessaryTexture;
			}

			API::DrawElementsInstancedTriangles(s_Data->CharInstanceCount);

			s_Data->Stats.DrawCalls++;
			s_Data->Stats.CharCount += s_Data->CharInstanceCount;

			s_Data->CharInstanceCount = 0;
		}
	}
}
