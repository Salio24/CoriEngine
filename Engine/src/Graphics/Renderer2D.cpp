#include "Renderer2D.hpp"
#include <ska_sort.hpp>
#include "Utility/AABB.hpp"
#include "FontData.hpp"
#include "Color.hpp"

namespace Cori {
	namespace Graphics {
		Renderer2D::RendererData* Renderer2D::s_Data{ nullptr };

		void Renderer2D::Init() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Initializing Renderer2D.");

			s_Data = new RendererData();

			constexpr auto params = Texture::Params();
			constexpr uint32_t white[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
			s_Data->WhiteTexture = Texture2D::Create(&white, 2, 2, params);

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

			s_Data->QuadInstanceVertexBuffer->Init(nullptr, RendererData::MaxInstanceCount * s_Data->QuadInstanceVertexBuffer->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
			s_Data->QuadInstanceVertexArray->AddVertexBuffer(s_Data->QuadInstanceVertexBuffer);

			uint32_t quadIndices[6] = { 0, 1, 2, 2, 3, 0 };
			s_Data->QuadInstanceIndexBuffer = IndexBuffer::Create(quadIndices, 6);
			s_Data->QuadInstanceVertexArray->AddIndexBuffer(s_Data->QuadInstanceIndexBuffer);

			s_Data->QuadInstanceBufferBase = new Quad[RendererData::MaxInstanceCount];

			s_Data->QuadInstanceShader = ShaderProgram::Create("enginedata/shaders/QuadInstancedVert.glsl", "enginedata/shaders/QuadInstancedFrag.glsl");

			s_Data->WorldSpaceTransparentQuadQueue.reserve(RendererData::WorldSpaceTransparentQuadQueueInitialSize);
			s_Data->WorldSpaceOpaqueQuadQueue.reserve(RendererData::WorldSpaceOpaqueQuadQueueInitialSize);

			s_Data->ScreenSpaceTransparentQuadQueue.reserve(RendererData::ScreenSpaceTransparentQuadQueueInitialSize);
			s_Data->ScreenSpaceOpaqueQuadQueue.reserve(RendererData::ScreenSpaceOpaqueQuadQueueInitialSize);

			// text setup

			s_Data->CharInstanceVertexArray = VertexArray::Create();
			s_Data->CharInstanceVertexBuffer = VertexBuffer::Create();
			s_Data->CharInstanceVertexBuffer->SetLayout({
					{ShaderDataType::Mat3, "a_Transform", 1},
					{ShaderDataType::Vec4, "a_TexturePosition", 1},
					{ShaderDataType::Vec4, "a_CharQuad", 1},
					{ShaderDataType::Vec4, "a_Color", 1},
					{ShaderDataType::Float, "a_Layer", 1}
				});

			s_Data->CharInstanceVertexBuffer->Init(nullptr, RendererData::MaxCharInstanceCount * s_Data->CharInstanceVertexBuffer->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
			s_Data->CharInstanceVertexArray->AddVertexBuffer(s_Data->CharInstanceVertexBuffer);

			s_Data->CharInstanceIndexBuffer = IndexBuffer::Create(quadIndices, 6);
			s_Data->CharInstanceVertexArray->AddIndexBuffer(s_Data->CharInstanceIndexBuffer);

			s_Data->CharInstanceBufferBase = new Char[RendererData::MaxCharInstanceCount];

			s_Data->CharInstanceShader = ShaderProgram::Create("enginedata/shaders/TextInstancedVert.glsl", "enginedata/shaders/TextInstancedFrag.glsl");

			s_Data->WorldSpaceTransparentTextQueue.reserve(RendererData::WorldSpaceTransparentTextQueueInitialSize);

			s_Data->ScreenSpaceTransparentTextQueue.reserve(RendererData::ScreenSpaceTransparentTextQueueInitialSize);

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Renderer2D Initialized successfully.");
		}

		void Renderer2D::Shutdown() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Shutting down Renderer2D.");

			delete[] s_Data->QuadInstanceBufferBase;
			delete s_Data;
		}



		void Renderer2D::BeginScene(const World::Components::Scene::Camera& camera) {
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

		void Renderer2D::DrawScene(World::Scene* scene) {
			CORI_PROFILE_FUNCTION();

			{
				CORI_PROFILE_SCOPE("Quad Submission");
				const auto& camera = scene->GetContextComponent<World::Components::Scene::Camera>();
				//Utility::AABB cameraBounds = { camera.m_CameraMinBound, camera.m_CameraMaxBound };
				World::EntityView view = scene->View<World::Components::Entity::QuadRenderer, World::Components::Entity::Transform>(World::Exclude<World::Components::Entity::InactiveLocallyFlag>());
				for (const auto entity : view) {
					auto& renderer = view.Get<World::Components::Entity::QuadRenderer>(entity);
					if (renderer.m_Visible) {
						if (renderer.GetTexture()) {
							auto& transform = view.Get<World::Components::Entity::Transform>(entity);
							Utility::AABB entityBounds = Utility::CalculateAABB(transform.m_WorldTransform, renderer.GetHalfSize());
							//SubmitAABB(camera.m_CameraBounds, 0.2f, {0.0f, 1.0f, 0.0f});
							if (AABBOverlapCheck(camera.m_CameraBounds, entityBounds)) {
								//SubmitAABB(entityBounds, 0.2f, {1.0f, 0.0f, 1.0f});
								if (renderer.GetSemiTransparencyState()) {
									SubmitQuad(WORLD_SPACE, SEMI_TRANSPARENT, transform.m_WorldTransform, renderer.GetHalfSize(), renderer.GetColor(), renderer.GetTexture(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
									continue;
								}
								SubmitQuad(WORLD_SPACE, OPAQUE, transform.m_WorldTransform, renderer.GetHalfSize(), renderer.GetColor(), renderer.GetTexture(), renderer.GetUVs(), transform.m_WorldDepth, renderer.m_FlipX, renderer.m_FlipY, renderer.m_FlatColored);
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

		void Renderer2D::SubmitQuad(const DrawSpace space, const ObjectTransparency transparencyMode, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, const std::shared_ptr<Texture2D>& texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
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

		void Renderer2D::BeginCharInstancedSet() {
			s_Data->CharInstanceCount = 0;
			s_Data->CharInstanceBufferPtr = s_Data->CharInstanceBufferBase;
		}

		void Renderer2D::EndCharInstancedSet(Texture2D* atlas, const glm::mat3& modelMatrix) {
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
				FlushInstancedChars(atlas, modelMatrix);
			}
		}

		void Renderer2D::SubmitQuadToQueue(std::vector<QuadInstance>& queue, const glm::mat3& transform, const glm::vec2 halfSize, const glm::vec4& tintColor, const std::shared_ptr<Texture2D>& texture, const UVs& uvs, const uint8_t depth, const bool flipX, const bool flipY, const bool flatColored) {
			if (!flipX && !flipY) {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture : texture,
					static_cast<glm::vec4>(uvs),
					depth
				);
			}
			else if (flipX && !flipY) {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture : texture,
					glm::vec4{uvs.UVmax.x, uvs.UVmin.y, uvs.UVmin.x, uvs.UVmax.y},
					depth
				);
			}
			else if (!flipX) {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture : texture,
					glm::vec4{uvs.UVmin.x, uvs.UVmax.y, uvs.UVmax.x, uvs.UVmin.y},
					depth
				);
			}
			else {
				queue.emplace_back(
					transform,
					halfSize,
					tintColor,
					flatColored ? s_Data->WhiteTexture : texture,
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

		void Renderer2D::SubmitTextToQueue(std::vector<TextInstance>& queue, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::u32string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) {
			queue.emplace_back(
				alignment,
				transform,
				fontSize,
				text,
				color,
				font,
				depth,
				limitX,
				lineSpacing,
				kerning
			);
		}

		void Renderer2D::SubmitTextToQueue(std::vector<TextInstance>& queue, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) {
			queue.emplace_back(
				alignment,
				transform,
				fontSize,
				text,
				color,
				font,
				depth,
				limitX,
				lineSpacing,
				kerning
			);
		}

		void Renderer2D::SubmitText(const DrawSpace space, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::u32string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) {
			if (space == WORLD_SPACE) {
				SubmitTextToQueue(s_Data->WorldSpaceTransparentTextQueue, alignment, transform, fontSize, text, color, font, depth, limitX, lineSpacing, kerning);
				return;
			}

			SubmitTextToQueue(s_Data->ScreenSpaceTransparentTextQueue, alignment, transform, fontSize, text, color, font, depth, limitX, lineSpacing, kerning);
		}

		void Renderer2D::SubmitText(const DrawSpace space, const TextAlignment alignment, const glm::mat3& transform, const float fontSize, const std::string_view& text, const glm::vec4& color, const std::shared_ptr<Font>& font, const uint8_t depth, const float limitX, const float lineSpacing, const float kerning) {
			if (space == WORLD_SPACE) {
				SubmitTextToQueue(s_Data->WorldSpaceTransparentTextQueue, alignment, transform, fontSize, text, color, font, depth, limitX, lineSpacing, kerning);
				return;
			}

			SubmitTextToQueue(s_Data->ScreenSpaceTransparentTextQueue, alignment, transform, fontSize, text, color, font, depth, limitX, lineSpacing, kerning);
		}

		Renderer2D::Statistics Renderer2D::GetStatistics() {
			return s_Data->Stats;
		}

		void Renderer2D::DrawQuadInstanced(const QuadInstance& quad) {
			if (!quad.m_Texture) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "DrawQuadInstanced: Texture is nullptr, trying to avoid read access violation");
				return;
			}

			if (s_Data->QuadInstanceCount >= RendererData::MaxInstanceCount) {
				StartNewInstancedSet();
			}

			if (s_Data->NecessaryTexture != quad.m_Texture.get()) {
				if (!s_Data->NecessaryTexture) {
					s_Data->NecessaryTexture = quad.m_Texture.get();
				}
				else {
					StartNewInstancedSet();
					s_Data->NecessaryTexture = quad.m_Texture.get();
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

		void Renderer2D::DrawTextInstanced(const TextInstance& text) {
			CORI_PROFILE_FUNCTION();
			if (!text.m_Font) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "DrawCharInstanced: Font is nullptr, trying to avoid read access violation");
				return;
			}

			SubmitQuad(SCREEN_SPACE, OPAQUE, glm::translate(text.m_Transform, glm::vec2(text.m_LimitX, 0.0f)), glm::vec2(1.0f, 50.0f), glm::vec4(1.0f), nullptr, UVs{}, 15, false, false, true);
			SubmitQuad(SCREEN_SPACE, OPAQUE, text.m_Transform, glm::vec2(0.5f, 0.5f), glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), nullptr, UVs{}, 15, false, false, true);

			BeginCharInstancedSet();

			const auto& fontGeometry = text.m_Font->GetData()->m_FontGeometry;
			const auto& metrics = fontGeometry.getMetrics();
			const auto atlas = text.m_Font->GetData()->m_Atlas;

			const float scale = 1.0f / (metrics.ascenderY - metrics.descenderY) * text.m_FontSize;
			float x = 0.0f;

			// maybe use something like ascender or smthg, some global font metric to center by Y
			float y = 0.0f;

			float totalLineLength = 0.0f;

			const float spaceGlyphAdvance = fontGeometry.getGlyph(' ')->getAdvance() * scale;
			constexpr uint8_t spacesInTab = CORI_SPACES_PER_TAB;

			const std::u32string_view view(text.m_Text);

			bool done = false;
			size_t currentOffset = 0;

			// this used only for tab calculation (includes spaces from tabs)
			uint32_t currentGlobalCharIndex = 0;

			constexpr std::u32string_view SEPARATORS = U" \a\b\t\n\v\f\r";

			static auto FindNextWord = [&](const std::u32string_view textView, const size_t startIndex) -> std::tuple<std::u32string_view, std::u32string_view, size_t> /* skippedPart, wordPart, wordEndIndex */ {
				if (startIndex >= textView.length()) {
					// nothing beyond startIndex, out of bounds
					return {{}, {}, std::u32string_view::npos};
				}

				const size_t wordStart = textView.find_first_not_of(SEPARATORS, startIndex);

				if (wordStart == std::u32string_view::npos) {
					// no word after startIndex, everything beyond startIndex considered skipped
					return {textView.substr(startIndex), {}, std::u32string_view::npos};
				}

				std::u32string_view skipped = textView.substr(startIndex, wordStart - startIndex);

				size_t wordEnd = textView.find_first_of(SEPARATORS, wordStart);

				// if no separator is found after the word, it extends to the end.
				if (wordEnd == std::u32string_view::npos) {
					std::u32string_view word = textView.substr(wordStart);
					return {skipped, word, std::u32string_view::npos};
				}

				// a word, a skipped part and a subsequent separator were found.
				std::u32string_view word = textView.substr(wordStart, wordEnd - wordStart);
				return {skipped, word, wordEnd};
			};

			static auto GetNextChar = [&](const uint32_t currentIndex, const std::u32string_view& localView) -> char32_t {
				if (currentIndex + 1 < localView.size()) {
					// not the last from localView
					return localView[currentIndex + 1];
				}
				// last from localView
				if (localView.data() + localView.size() == view.data() + view.size()) {
					// last from globalView
					done = true;
					return '\0';
				}

				// access one beyond end of localView
				return *(localView.data() + localView.size() + 1);
			};

			static auto ProcessGlyph = [&](const uint32_t index, const std::u32string_view& localView) {
				if (s_Data->CharInstanceCount > RendererData::MaxCharInstanceCount) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Trying to render more than '{}' in one go. Aborting further rendering of this 'Text' piece.", RendererData::MaxCharInstanceCount);
					return;
				}

				const char32_t c = localView[index];

				auto glyph = fontGeometry.getGlyph(c);
				if (!glyph) {
					glyph = fontGeometry.getGlyph('#');
				}

				if (!glyph) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Renderer2D }, "Failed to locate glyph '#' after failing to locate glyph 'UTF-32 codepoint 0x{:08X}'. Aborting further rendering of this 'Text' piece.", static_cast<uint32_t>(c));
					return;
				}

				double x0, y0, x1, y1;
				glyph->getQuadPlaneBounds(x0, y0, x1, y1);

				const glm::vec4 charQuad = { x0 * scale + x, y0 * scale + y, x1 * scale + x, y1 * scale + y };

				double u0, v0, u1, v1;
				glyph->getQuadAtlasBounds(u0, v0, u1, v1);

				const float texelWidth = 1.0f / static_cast<float>(atlas->GetWidth());
				const float texelHeight = 1.0f / static_cast<float>(atlas->GetHeight());

				const glm::vec4 UV = { u0 * texelWidth, v0 * texelHeight, u1 * texelWidth, v1 * texelHeight };

				s_Data->CharInstanceBufferPtr->m_Transform = text.m_Transform;
				s_Data->CharInstanceBufferPtr->m_TexturePosition = UV;
				s_Data->CharInstanceBufferPtr->m_CharQuad = charQuad;
				s_Data->CharInstanceBufferPtr->m_Color = text.m_Color;
				s_Data->CharInstanceBufferPtr->m_Layer = text.m_Depth;
				s_Data->CharInstanceBufferPtr++;

				s_Data->CharInstanceCount++;

				const char32_t nextChar = GetNextChar(index, localView);
				if (nextChar != '\0') {
					double advance;
					fontGeometry.getAdvance(advance, c, nextChar);
					x += scale * advance + text.m_Kerning;
					totalLineLength = x;
				} else {
					const float sizeX = x1 - x0;
					totalLineLength += sizeX * scale;
				}
			};


			static auto PreprocessWord = [&](const std::u32string_view& word) -> float {
				float totalWordAdvance = 0.0f;

				for (uint32_t i = 0; i < word.size(); i++) {
					const char32_t nextChar = GetNextChar(i, word);
					if (nextChar != '\0') {
						double advance;
						fontGeometry.getAdvance(advance, word[i], nextChar);
						totalWordAdvance += scale * advance + text.m_Kerning;
					}
				}

				return totalWordAdvance;
			};

			static auto GoToNewLine = [&] {
				switch (text.m_Alignment) {
				case LEFT:
					{
						const float offset = totalLineLength;
						SubmitQuad(SCREEN_SPACE, OPAQUE, glm::translate(text.m_Transform, glm::vec2(offset, y)), glm::vec2(0.5f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), nullptr, UVs{}, 15, false, false, true);

						break;
					}
				case CENTER:
					{
						const float offset = -(totalLineLength / 2.0f);
						SubmitQuad(SCREEN_SPACE, OPAQUE, glm::translate(text.m_Transform, glm::vec2(offset, y)), glm::vec2(0.5f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), nullptr, UVs{}, 15, false, false, true);
						EndCharInstancedSet(atlas.get(), glm::translate(glm::mat3(1.0f), glm::vec2(offset, 0.0f)));
						BeginCharInstancedSet();
						break;
					}
				case RIGHT:
					{
						const float offset = -totalLineLength;
						SubmitQuad(SCREEN_SPACE, OPAQUE, glm::translate(text.m_Transform, glm::vec2(offset, y)), glm::vec2(0.5f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), nullptr, UVs{}, 15, false, false, true);
						EndCharInstancedSet(atlas.get(), glm::translate(glm::mat3(1.0f), glm::vec2(offset, 0.0f)));
						BeginCharInstancedSet();
						break;
					}
				}

				totalLineLength = 0;
				x = 0;
				y -= scale * metrics.lineHeight + text.m_LineSpacing;
			};

			while (!done) {
				auto [skippedPart, currentWord, nextOffset] = FindNextWord(view, currentOffset);
				if (nextOffset != std::u32string_view::npos) {
					currentOffset = nextOffset;
				} else {
					done = true;
				}

				bool longWord = false;

				// only used for right align
				bool ignoreSpaces = false;
				bool globalAdvanceChanged = false;

				float wordAdvance = 0.0f;
				if (text.m_Alignment == RIGHT) {
					wordAdvance = PreprocessWord(currentWord);
					if (x + wordAdvance > text.m_LimitX) {
						ignoreSpaces = true;
					}
				}

				for (uint32_t i = 0; i < skippedPart.size(); ++i) {
					if (skippedPart[i] == ' ') {
						const char32_t nextChar = GetNextChar(i, view);
						if (nextChar != '\0' && !ignoreSpaces) {
							++currentGlobalCharIndex;
							double advance;
							fontGeometry.getAdvance(advance, skippedPart[i], nextChar);
							const float nextX = x + scale * advance + text.m_Kerning;
							if (nextX > text.m_LimitX) {
								GoToNewLine();
							} else {
								x = nextX;
								totalLineLength = x;
							}
						}
						continue;
					}

					if (skippedPart[i] == '\t') {

						const uint32_t nextTabStop = ((currentGlobalCharIndex - 1) / spacesInTab + 1) * spacesInTab;

						uint8_t spaces = nextTabStop - currentGlobalCharIndex;
						if (spaces == 0) {
							spaces = 4;
						}

						currentGlobalCharIndex += spaces;

						const float advance = static_cast<float>(spaces) * spaceGlyphAdvance;

						const float nextX = x + advance;
						if (nextX > text.m_LimitX) {
							GoToNewLine();
						}
						else {
							x = nextX;
							totalLineLength = x;
						}

						globalAdvanceChanged = true;
						continue;
					}

					if (skippedPart[i] == '\n') {
						GoToNewLine();
					}
				}

				switch (text.m_Alignment) {
					case LEFT:
					case CENTER:
						{
							wordAdvance = PreprocessWord(currentWord);
							if (wordAdvance < text.m_LimitX) {
								if (x + wordAdvance > text.m_LimitX) {
									GoToNewLine();
								}
							} else {
								longWord = true;
							}
							break;
						}
					case RIGHT:
						{
							if (ignoreSpaces && !globalAdvanceChanged) {
								if (wordAdvance < text.m_LimitX) {
									GoToNewLine();
								} else {
									longWord = true;
								}
							} else {
								wordAdvance = PreprocessWord(currentWord);
								if (wordAdvance < text.m_LimitX) {
									if (x + wordAdvance > text.m_LimitX) {
										GoToNewLine();
									}
								} else {
									longWord = true;
								}
							}
							break;
						}
				}

				for (uint32_t i = 0; i < currentWord.size(); ++i) {
					if (longWord) {
						if (x > text.m_LimitX) {
							GoToNewLine();
						}
					}

					++currentGlobalCharIndex;

					ProcessGlyph(i, currentWord);
				}
			}


			switch (text.m_Alignment) {
			case LEFT:
				{
					const float offset = totalLineLength;
					SubmitQuad(SCREEN_SPACE, OPAQUE, glm::translate(text.m_Transform, glm::vec2(offset, y)), glm::vec2(0.5f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), nullptr, UVs{}, 15, false, false, true);
					EndCharInstancedSet(atlas.get(), glm::mat3(1.0f));
					break;
				}
			case CENTER:
				{
					const float offset = -(totalLineLength / 2.0f);
					SubmitQuad(SCREEN_SPACE, OPAQUE, glm::translate(text.m_Transform, glm::vec2(offset, y)), glm::vec2(0.5f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), nullptr, UVs{}, 15, false, false, true);
					EndCharInstancedSet(atlas.get(), glm::translate(glm::mat3(1.0f), glm::vec2(offset, 0.0f)));
					break;
				}
			case RIGHT:
				{
					const float offset = -totalLineLength;
					SubmitQuad(SCREEN_SPACE, OPAQUE, glm::translate(text.m_Transform, glm::vec2(offset, y)), glm::vec2(0.5f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), nullptr, UVs{}, 15, false, false, true);
					EndCharInstancedSet(atlas.get(), glm::translate(glm::mat3(1.0f), glm::vec2(offset, 0.0f)));
					break;
				}
			}

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
							return reinterpret_cast<uint64_t>(quad.m_Texture.get());
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
			
			if (!s_Data->ScreenSpaceOpaqueQuadQueue.empty()) {
				CORI_PROFILE_SCOPE("Opaque screen space instanced quad flush");

				BeginScreenSpacePass();

				{
					CORI_PROFILE_SCOPE("Opaque screen space instanced quad texture sort");
					ska_sort(
						s_Data->ScreenSpaceOpaqueQuadQueue.begin(),
						s_Data->ScreenSpaceOpaqueQuadQueue.end(),
						[](const QuadInstance& quad) -> uint64_t {
							return reinterpret_cast<uint64_t>(quad.m_Texture.get());
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
		}

		void Renderer2D::FlushTransparentQueues() {
			if (!s_Data->WorldSpaceTransparentQuadQueue.empty()) {
				CORI_PROFILE_SCOPE("Transparent instanced quad flush");

				BeginWorldSpacePass();

				{
					CORI_PROFILE_SCOPE("Transparent instanced quad texture sort");
					ska_sort(
						s_Data->WorldSpaceTransparentQuadQueue.begin(),
						s_Data->WorldSpaceTransparentQuadQueue.end(),
						[](const QuadInstance& quad) -> uint64_t {
							return reinterpret_cast<uint64_t>(quad.m_Texture.get());
						}
					);
				}

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

			if (!s_Data->WorldSpaceTransparentTextQueue.empty()) {
				CORI_PROFILE_SCOPE("Instanced Text flush");

				BeginWorldSpacePass();

				{
					CORI_PROFILE_SCOPE("Instanced Text layer sort");
					ska_sort(
						s_Data->WorldSpaceTransparentTextQueue.begin(),
						s_Data->WorldSpaceTransparentTextQueue.end(),
						[](const TextInstance& text) -> uint8_t {
							return text.m_Depth;
						}
					);
				}

				API::EnableBlending();
				API::SetDepthMask(false);

				{
					CORI_PROFILE_SCOPE("Instanced Text draw");
					for (const auto& text : s_Data->WorldSpaceTransparentTextQueue) {
						DrawTextInstanced(text);
					}
				}

				API::SetDepthMask(true);
				API::DisableBlending();

				s_Data->WorldSpaceTransparentTextQueue.clear();
			}

			if (!s_Data->ScreenSpaceTransparentQuadQueue.empty()) {
				CORI_PROFILE_SCOPE("Transparent screen space instanced quad flush");

				BeginScreenSpacePass();

				{
					CORI_PROFILE_SCOPE("Transparent screen space instanced quad texture sort");
					ska_sort(
						s_Data->ScreenSpaceTransparentQuadQueue.begin(),
						s_Data->ScreenSpaceTransparentQuadQueue.end(),
						[](const QuadInstance& quad) -> uint64_t {
							return reinterpret_cast<uint64_t>(quad.m_Texture.get());
						}
					);
				}

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

			if (!s_Data->ScreenSpaceTransparentTextQueue.empty()) {
				CORI_PROFILE_SCOPE("Screen Space instanced Text flush");

				BeginScreenSpacePass();

				{
					CORI_PROFILE_SCOPE("Screen Space instanced Text layer sort");
					ska_sort(
						s_Data->ScreenSpaceTransparentTextQueue.begin(),
						s_Data->ScreenSpaceTransparentTextQueue.end(),
						[](const TextInstance& text) -> uint8_t {
							return text.m_Depth;
						}
					);
				}

				API::EnableBlending();
				API::SetDepthMask(false);

				{
					CORI_PROFILE_SCOPE("Screen Space instanced Text draw");
					for (const auto& text : s_Data->ScreenSpaceTransparentTextQueue) {
						DrawTextInstanced(text);
					}
				}

				API::SetDepthMask(true);
				API::DisableBlending();

				s_Data->ScreenSpaceTransparentTextQueue.clear();
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

		void Renderer2D::FlushInstancedChars(Texture2D* atlas, const glm::mat3& modelMatrix) {
			CORI_PROFILE_FUNCTION();

			const auto size = reinterpret_cast<uint8_t*>(s_Data->CharInstanceBufferPtr) - reinterpret_cast<uint8_t*>(s_Data->CharInstanceBufferBase);
			s_Data->CharInstanceVertexBuffer->SetData(s_Data->CharInstanceBufferBase, size);

			const glm::vec2 unitRange = glm::vec2(2.0f) / glm::vec2(atlas->GetWidth(), atlas->GetHeight());

			s_Data->CharInstanceShader->SetMat4("u_ViewProjection", s_Data->CurrentViewProjectionMatrix);
			s_Data->CharInstanceShader->SetMat3("u_ModelMatrix", modelMatrix);
			s_Data->CharInstanceShader->SetInt("u_Texture", 0);
			s_Data->CharInstanceShader->SetVec2("u_UnitRange", unitRange);

			if (s_Data->CurrentTexture != atlas) {
				atlas->Bind(0);
				s_Data->CurrentTexture = atlas;
			}

			API::DrawElementsInstancedTriangles(s_Data->CharInstanceCount);

			s_Data->Stats.DrawCalls++;
			s_Data->Stats.CharCount += s_Data->CharInstanceCount;

			s_Data->CharInstanceCount = 0;
		}
	}
}
