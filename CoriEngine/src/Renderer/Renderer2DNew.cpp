#include "Renderer2DNew.hpp"



namespace Cori {
	namespace Test {
		void Renderer2D::Init() {
			CORI_CORE_INFO_TAGGED({ "Graphics", "Renderer2D" }, "Initializing Renderer2D.");
			
			uint32_t indicesBatch[m_Data->MaxQuadCount];
			uint32_t offset = 0;
			for (int i = 0; i < m_Data->MaxIndexCount; i += 6) {
				indicesBatch[i + 0] = 0 + offset;
				indicesBatch[i + 1] = 1 + offset;
				indicesBatch[i + 2] = 2 + offset;

				indicesBatch[i + 3] = 2 + offset;
				indicesBatch[i + 4] = 3 + offset;
				indicesBatch[i + 5] = 0 + offset;

				offset += 4;
			}

			m_Data->QuadVertexArray = VertexArray::Create();
			m_Data->QuadVertexBuffer = VertexBuffer::Create();
			m_Data->QuadVertexBuffer->SetLayout({
				{ ShaderDataType::Vec3, "a_Position" },
				{ ShaderDataType::Vec2, "a_TexturePosition" },
				{ ShaderDataType::Vec4, "a_TintColor" }
			});

			m_Data->QuadVertexBuffer->Init(nullptr, m_Data->MaxVertexCount * m_Data->QuadVertexBuffer->GetLayout().GetStride(), DRAW_TYPE::DYNAMIC);
			m_Data->QuadVertexArray->AddVertexBuffer(m_Data->QuadVertexBuffer);

			m_Data->QuadIndexBuffer = IndexBuffer::Create(indicesBatch, sizeof(indicesBatch));
			m_Data->QuadVertexArray->AddIndexBuffer(m_Data->QuadIndexBuffer);

			m_Data->QuadVertexBufferBase = new QuadVertex[m_Data->MaxVertexCount];

			m_Data->QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
			m_Data->QuadVertexPositions[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
			m_Data->QuadVertexPositions[2] = { 0.5f,  0.5f, 0.0f, 1.0f };
			m_Data->QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

			const Texture2DDescriptor wt {
				"White Texture",
				"assets/engine/textures/white1x1.png"
			};

			m_Data->WhiteTexture = AssetManager::GetTexture2DOwning(wt);

			const ShaderProgramDescriptor shader {
				"Quad Shader",
				"assets/engine/shaders/QuadVert.glsl",
				"assets/engine/shaders/QuadFrag.glsl"
			};

			m_Data->QuadShader = AssetManager::GetShaderOwning(shader);

			m_Data->OpaqueQuadQueue.reserve(m_Data->OpaqueQuadQueueSize);
			m_Data->TransparentFrontQuadQueue.reserve(m_Data->TransparentQuadQueueSize);
			m_Data->TransparentBackQuadQueue.reserve(m_Data->TransparentQuadQueueSize);

			CORI_CORE_INFO_TAGGED({ "Graphics", "Renderer2D" }, "Renderer2D Initialized successfully.");
		}


		void Renderer2D::Shutdown() {
			CORI_CORE_INFO_TAGGED({ "Graphics", "Renderer2D" }, "Shutting down Renderer2D.");

			delete[] m_Data->QuadVertexBufferBase;
		}

		void Renderer2D::BeginScene(const Components::Scene::Camera& camera) {
			CORI_PROFILE_FUNCTION();

			m_Data->CurrentViewProjectionMatrix = camera.m_ViewProjectionMatrix;

			// later
			//m_Data->CurrentUIViewProjectionMatrix = camera.m_UIViewProjectionMatrix;
			m_Data->Stats.DrawCalls = 0;
			m_Data->Stats.QuadCount = 0;

		}

		void Renderer2D::EndScene() {
			CORI_PROFILE_FUNCTION();

			// flush funcs here
		}

		void Renderer2D::BeginBatch() {
			CORI_PROFILE_FUNCTION();

			m_Data->QuadIndexCount = 0;
			m_Data->QuadVertexBufferPtr = m_Data->QuadVertexBufferBase;
		}

		void Renderer2D::EndBatch() {
			CORI_PROFILE_FUNCTION();
			auto size = reinterpret_cast<uint8_t*>(m_Data->QuadVertexBufferPtr) - reinterpret_cast<uint8_t*>(m_Data->QuadVertexBufferBase);
			m_Data->QuadVertexBuffer->SetData(m_Data->QuadVertexBufferBase, size);

			m_Data->QuadShader->SetMat4("u_ViewProjection", m_Data->CurrentViewProjectionMatrix);
			m_Data->QuadShader->SetInt("u_Texture", 0);

			m_Data->CurrentTexture->Bind(0);
			GraphicsCall::DrawElements(m_Data->QuadVertexArray, m_Data->QuadIndexCount);
			m_Data->Stats.DrawCalls++;

			m_Data->QuadIndexCount = 0;
		}

		void Renderer2D::StartNewBatch() {
			EndBatch();
			BeginBatch();
		}

		void Renderer2D::SubmitOpaqueQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation) {
			m_Data->OpaqueQuadQueue.emplace_back(Quad{
				.position = position,
				.size = size,
				.tintColor = tintColor,
				.texture = texture ? texture : m_Data->WhiteTexture.get(),
				.uvs = uvs,
				.rotation = rotation,
				.layer = layer
			});
		}

		void Renderer2D::SubmitTransparentFrontQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation) {
			m_Data->TransparentFrontQuadQueue.emplace_back(Quad{
				.position = position,
				.size = size,
				.tintColor = tintColor,
				.texture = texture ? texture : m_Data->WhiteTexture.get(),
				.uvs = uvs,
				.rotation = rotation,
				.layer = layer
			});
		}

		void Renderer2D::SubmitTransparentBackQuad(const glm::vec2 position, const glm::vec2 size, uint16_t layer, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor, float rotation) {
			m_Data->TransparentBackQuadQueue.emplace_back(Quad{
				.position = position,
				.size = size,
				.tintColor = tintColor,
				.texture = texture ? texture : m_Data->WhiteTexture.get(),
				.uvs = uvs,
				.rotation = rotation,
				.layer = layer
			});
		}

		void Renderer2D::DrawQuad(const glm::mat4& transform, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor) {
			if (CORI_CORE_ASSERT_ERROR(texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
			if (m_Data->QuadIndexCount >= m_Data->MaxIndexCount) {
				StartNewBatch();
			}

			if (m_Data->CurrentTexture != texture) {
				if (!m_Data->CurrentTexture) {
					m_Data->CurrentTexture = texture;
				}
				else {
					StartNewBatch();
					m_Data->CurrentTexture = texture;
				}
			}

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[0];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmin.x, uvs.UVmin.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[1];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmax.x, uvs.UVmin.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[2];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmax.x, uvs.UVmax.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[3];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmin.x, uvs.UVmax.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadIndexCount += 6;

			m_Data->Stats.QuadCount++;
		}

		void Renderer2D::DrawQuadFlipped(const glm::mat4& transform, Texture2D* texture, const UVs& uvs, const glm::vec4& tintColor) {
			if (CORI_CORE_ASSERT_ERROR(texture, "Texture is nullptr, trying to avoid read access violation")) { return; }
			if (m_Data->QuadIndexCount >= m_Data->MaxIndexCount) {
				StartNewBatch();
			}

			if (m_Data->CurrentTexture != texture) {
				if (!m_Data->CurrentTexture) {
					m_Data->CurrentTexture = texture;
				}
				else {
					StartNewBatch();
					m_Data->CurrentTexture = texture;
				}
			}

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[0];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmax.x, uvs.UVmin.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[1];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmin.x, uvs.UVmin.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[2];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmin.x, uvs.UVmax.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadVertexBufferPtr->Position = transform * m_Data->QuadVertexPositions[3];
			m_Data->QuadVertexBufferPtr->TexturePosition = { uvs.UVmax.x, uvs.UVmax.y };
			m_Data->QuadVertexBufferPtr->TintColor = tintColor;
			m_Data->QuadVertexBufferPtr++;

			m_Data->QuadIndexCount += 6;

			m_Data->Stats.QuadCount++;
		}

		// maybe add tiling factor????
		// maybe tiling factor should be n in 2^n?? and an int??, so pixelart would look nice

		void Renderer2D::FlushOpaqueQuads() {
			if (m_Data->OpaqueQuadQueue.empty()) { return; }

			//	DONT FORGET TO BIND ALL NECESSARY STUFF HERE

			// sort, radix sort????

			// begin batch

			// loop through sorted queue, matrix mult, call drawquad or flipped

			// end batch

			// clear queue

		}

		void Renderer2D::FlushTransparentFrontQuads() {
			if (m_Data->TransparentFrontQuadQueue.empty()) { return; }

		}

		void Renderer2D::FlushTransparentBackQuads() {
			if (m_Data->TransparentBackQuadQueue.empty()) { return; }

		}

	}
}