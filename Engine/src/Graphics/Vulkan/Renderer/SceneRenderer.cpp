#include "SceneRenderer.hpp"
#include "FrameData.hpp"

namespace {
	//hack to pass name to PRT only in debug builds, cuz PRT has move/copy constructors & assigment operators deleted.
	const char* GetNameForPRT(Cori::Graphics::SceneRenderer::CreateInfo& createInfo) {
		#ifdef DEBUG_BUILD
		static std::string nameBuffer;
		nameBuffer = std::format("PRT of '{}'", createInfo.name);
		return nameBuffer.c_str();
		#else
		return "";
		#endif
	}
}

namespace Cori {
	namespace Graphics {
		void SceneRenderer::OnMaterialShaderEffectChanged(void* instance, const Core::Handle<Material> material, const Core::ConstHandle<ShaderEffect> oldShaderEffect, const Core::ConstHandle<ShaderEffect> newShaderEffect) {
			auto* renderer = static_cast<SceneRenderer*>(instance);
			for (auto it = renderer->m_Objects.cbegin(); it != renderer->m_Objects.cend(); ++it) {
				if (it->m_Material.GetHandle() == material) {
					auto& oldBatch = renderer->m_Batches[it->m_OwnerBatch];
					auto mesh = oldBatch.m_Mesh;

					oldBatch.DecrementObjectCounter();
					if (oldBatch.GetObjectCount() == 0) {
						renderer->DestroyBatch(it->m_OwnerBatch);
					}

					auto [newGroup, newBatch] = renderer->FindAppropriateGroupAndBatch(newShaderEffect, std::move(mesh));
					renderer->m_Objects[it.GetIndex()].m_OwnerBatch = newBatch;
					renderer->m_Batches[newBatch].IncrementObjectCounter();
				}
			}
		}

		void SceneRenderer::ProcessFrameData() {
			FrameData** ptr_ = m_ReadyRing.Front();
			CORI_CORE_ASSERT(ptr_, "SceneRenderer FrameData wasn't ready when ProcessFrameData was called.")
			FrameData* ptr = *ptr_;
			m_ReadyRing.Pop();
			for (auto& patch : ptr->patches) {
				CORI_CORE_ASSERT(IsHandleValid(patch.handle), "FrameData contains a patch with invalid RenderObject handle");

				if (patch.isRegisterRequest) {
					RegisterObject(patch.handle, std::move(patch.mesh.value()), std::move(patch.material.value()), patch.transform, patch.uvOffsets);
				} else {
					if (patch.mesh) {
						ChangeRenderObjectMesh(patch.handle, std::move(patch.mesh.value()));
					}
					if (patch.material) {
						ChangeRenderObjectMaterial(patch.handle, std::move(patch.material.value()));
					}
					if (patch.isNewTransform) {
						ChangeRenderObjectTransform(patch.handle, patch.transform);
					}
					if (patch.isNewUvOffsets) {
						ChangeRenderObjectUVOffsets(patch.handle, patch.uvOffsets);
					}
				}
			}

			for (auto handle : ptr->deletedObjects) {
				CORI_CORE_ASSERT(IsHandleValid(handle), "FrameData contains a delete request with invalid RenderObject handle");
				UnregisterObject(handle);
			}

			if (ptr->cameraSnapshot.has_value()) {
				m_CameraSnapshot = ptr->cameraSnapshot.value();
			}

			if (ptr->resizeRequest.has_value()) {
				m_PRT.Resize(ptr->resizeRequest.value());
			}

			m_RecycleRing.Emplace(ptr);
		}

		SceneRenderer::~SceneRenderer() {
			VulkanMaterialSystem::RemoveOnShaderEffectSwappedListener(this);

			for (auto ptr : m_FrameDataAllocated) {
				delete ptr;
			}
		}

		SceneRenderer::SceneRenderer(CreateInfo&& createInfo)
			: cullShader(Core::AssetManager2::Load<ComputeShader>("assets/Shaders/Cull_Pass1.json")),
			cmgShader(Core::AssetManager2::Load<ComputeShader>("assets/Shaders/Cull_Pass2.json")),
			compactShader(Core::AssetManager2::Load<ComputeShader>("assets/Shaders/Cull_Pass3.json")),
			m_PRT(createInfo.initialPRTExtent, createInfo.PRTFormat, createInfo.registerPRTWithImGui, GetNameForPRT(createInfo)) {
			m_Objects.Reserve(256);
			m_Batches.Reserve(128);
			m_DrawGroups.Reserve(16);

			#ifdef DEBUG_BUILD
			m_Name = createInfo.name;
			#endif

			//std::ifstream file(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/CullingShader.spv", std::ios::ate | std::ios::binary);
			//if (!file.is_open()) {
			//	throw std::runtime_error("failed to open file!");
			//}
			//
			//std::vector<Byte> buffer(file.tellg());
			//file.seekg(0, std::ios::beg);
			//file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
			//file.close()
				#if 0
			std::ifstream file_(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/TestShader.spv", std::ios::ate | std::ios::binary);
			if (!file_.is_open()) {
				throw std::runtime_error("failed to open file!");
			}

			std::vector<Byte> buffer_(file_.tellg());
			file_.seekg(0, std::ios::beg);
			file_.read(reinterpret_cast<char*>(buffer_.data()), static_cast<std::streamsize>(buffer_.size()));
			file_.close();

			testShader = VulkanShaderManager::CreateVertexShaderPair(buffer_.data(), buffer_.size(), "vertMain", "fragMain", "Test Shader");

			std::ifstream file__(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "shaders/DefaultShader.spv", std::ios::ate | std::ios::binary);
			if (!file__.is_open()) {
				throw std::runtime_error("failed to open file!");
			}

			std::vector<Byte> buffer__(file__.tellg());
			file__.seekg(0, std::ios::beg);
			file__.read(reinterpret_cast<char*>(buffer__.data()), static_cast<std::streamsize>(buffer__.size()));
			file__.close();

			defaultShader = VulkanShaderManager::Get().AllocateShaderPairHandle();

			VulkanShaderManager::Get().CreateShaderPair(defaultShader, buffer__.data(), buffer__.size(), "vertMain", buffer__.data(), buffer__.size(), "fragMain", "Default Shader");

			float depth = 0.0f;

			std::vector<StaticVertex> vertices = {
					// Bottom-left
					{glm::vec3(-0.5f, -0.5f, depth), 0, 0, glm::vec2(0, 0), 0xFFFFFFFF},

					// Bottom-right
					{glm::vec3(0.5f, -0.5f, depth), 0, 0, glm::vec2(1, 0), 0xFFFFFFFF},

					// Top-right
					{glm::vec3(0.5f, 0.5f, depth), 0, 0, glm::vec2(1, 1), 0xFFFFFFFF},

					// Top-left
					{glm::vec3(-0.5f, 0.5f, depth), 0, 0, glm::vec2(0, 1), 0xFFFFFFFF}
				};

			std::vector<uint32_t> indices = {
					0, 1, 2, // first triangle
					2, 3, 0 // second triangle
				};

				#endif

			//quad = VulkanMeshManager::CreateMesh();
			//VulkanMeshManager::LoadToMesh(quad, vertices, std::move(indices));

			//auto image = Image::Create(FileSystem::PathManager::GetAliasedPath("ENGINE_DATA") / "placeholders/uv_sample.png");
			//texture = Core::AssetManager2::Load<Texture2>("assets/AssetNew.json");
			//swordAlbedo = Core::AssetManager2::Load<Texture2>("assets/Textures/Sword_T_albedo.json");
			//sword = Core::AssetManager2::Load<Mesh>("assets/Sword_M.json");

			//texture = VulkanTextureManager::CreateTexture(vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, { image->GetHeight(), image->GetWidth(), 1 }, 1, 1, vk::SampleCountFlagBits::e1, "UV sample texture");
			//VulkanTextureManager::UpdateTexture(texture, std::span{ static_cast<Byte*>(image->GetPixelData()), image->GetHeight() * image->GetWidth() * 4 }, { 0, 0, 0 }, { image->GetHeight(), image->GetWidth(), 1 }, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 });
			//VulkanTextureManager::ChangeView(texture, vk::ImageViewType::e2D, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

				#if 0

			PipelineState state{
					.cullMode = vk::CullModeFlagBits::eNone,
					.frontFace = vk::FrontFace::eCounterClockwise,
				};

			auto shaderEffect = VulkanMaterialSystem::CreateShaderEffect(testShader, state, {}, "Test Shader Effect");

			MaterialData materialData{
					.colorFactor = {1.0f, 1.0f, 1.0f, 1.0f},
					.albedoTexture = swordAlbedo.GetHandle(),
					.albedoSampler = 0
				};

			MaterialData materialData_{
					.colorFactor = {1.0f, 1.0f, 1.0f, 1.0f},
					.albedoTexture = VulkanTextureManager::GetPlaceholder<Texture2>(),
					.albedoSampler = 0
				};

			material2 = VulkanMaterialSystem::CreateMaterial(shaderEffect, materialData_, "Test Material 2");

			material = VulkanMaterialSystem::CreateMaterial(shaderEffect, materialData, "Test Material");


				#endif

			//glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(0.2f), glm::vec3(0.5f, 0.5, 0.5f));
			//swordMaterial = Core::AssetManager2::Load<Material>("assets/Sword_Material.json");
			//auto result = RegisterObject(sword, swordMaterial, transform);
			//transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.0f));
			//if (!result) {
			//	CORI_DEBUG("{}", to_string(result.error()));
			//}

			//RegisterObject(quad, material2, transform);

			VulkanMaterialSystem::AddOnShaderEffectSwappedListener(this, OnMaterialShaderEffectChanged);
			//FIXME: renderer crashes if we try to run it with no objects added, because we try to allocate virtual upload buffer with size 0 and vma becomes all whiny


			for (auto& ptr : m_FrameDataAllocated) {
				ptr = new FrameData();
				m_RecycleRing.Emplace(ptr);
			}
		}
	}
}