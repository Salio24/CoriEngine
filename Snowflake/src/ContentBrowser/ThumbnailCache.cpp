#include "ThumbnailCache.hpp"
#include "Core/AssetManager/AssetManager2.hpp"
#include "Graphics/Vulkan/VulkanMaterialSystem.hpp"
#include "WorldSystem/Components.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "WorldSystem/Systems/RenderSync.hpp"
#include "LogTags.hpp"

namespace Snowflake {
	std::weak_ptr<ThumbnailCache> ThumbnailCache::s_Instance{};

	std::shared_ptr<ThumbnailCache> ThumbnailCache::Get() {
		auto existing = s_Instance.lock();
		if (existing) {
			return existing;
		}

		auto instance = std::shared_ptr<ThumbnailCache>(new ThumbnailCache());

		instance->m_FreeHandles.reserve(s_MaxThumbnails);
		for (uint32_t i = s_MaxThumbnails; i > 0; i--) {
			instance->m_FreeHandles.emplace_back(i - 1);
		}

		s_Instance = instance;
		return instance;
	}

	ThumbnailCache::~ThumbnailCache() {
		for (uint32_t i = 0; i < s_PreviewSlotCount; i++) {
			PreviewSlot& slot = m_PreviewSlots[i];
			if (slot.valid) {
				const auto result = Cori::World::SceneManager::DestroyScene(std::format("ThumbnailPreview_{}", i));
				if (!result) {
					CORI_CORE_ERROR_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Failed to destroy a thumbnail preview scene. Error: {}", result.error().what());
				}
			}
		}
	}

	ThumbnailCache::ThumbnailCache() {
		m_PreviewMaterial = Cori::Core::AssetManager2::IsRegistered("enginedata://PreviewMaterial.json") ? Cori::Core::AssetManager2::Load<Cori::Graphics::Material>("enginedata://PreviewMaterial.json") : Cori::Core::AssetRef<Cori::Graphics::Material>(Cori::Graphics::VulkanMaterialSystem::GetPlaceholder<Cori::Graphics::Material>());

		m_PreviewSphere = Cori::Core::AssetManager2::IsRegistered("enginedata://PreviewSphere.json") ? Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>("enginedata://PreviewSphere.json") : Cori::Core::AssetRef<Cori::Graphics::Mesh>(Cori::Graphics::VulkanMeshManager::GetPlaceholder<Cori::Graphics::Mesh>());

		m_BackgroundMaterial = Cori::Core::AssetManager2::IsRegistered("enginedata://CheckerboardMaterial.json") ? Cori::Core::AssetManager2::Load<Cori::Graphics::Material>("enginedata://CheckerboardMaterial.json") : Cori::Core::AssetRef<Cori::Graphics::Material>(Cori::Graphics::VulkanMaterialSystem::GetPlaceholder<Cori::Graphics::Material>());
	}

	uint32_t ThumbnailCache::QuantizeSize(const uint32_t requestedSize) {
		for (auto sizeClass : s_ThumbnailSizeClasses) {
			if (requestedSize <= sizeClass) {
				return sizeClass;
			}
		}

		return s_ThumbnailSizeClasses[s_ThumbnailSizeClassCount - 1];
	}

	uint32_t ThumbnailCache::SizeClassIndex(const uint32_t quantizedSize) {
		for (uint32_t i = 0; i < s_ThumbnailSizeClassCount; i++) {
			if (s_ThumbnailSizeClasses[i] == quantizedSize) {
				return i;
			}
		}

		return s_ThumbnailSizeClassCount - 1;
	}

	bool ThumbnailCache::AllocateRect(const uint32_t quantizedSize, Cori::Graphics::ThumbnailRect& outRect) {
		uint32_t classIndex = SizeClassIndex(quantizedSize);

		if (!m_FreeRects[classIndex].empty()) {
			outRect = m_FreeRects[classIndex].back();
			m_FreeRects[classIndex].pop_back();
			return true;
		}

		static constexpr uint32_t blocksPerSide = Cori::Graphics::s_ThumbnailAtlasExtent / s_ThumbnailBlockExtent;
		static constexpr uint32_t blockCount = blocksPerSide * blocksPerSide;

		if (m_NextBlock >= blockCount) {
			return false;
		}

		uint32_t blockX = (m_NextBlock % blocksPerSide) * s_ThumbnailBlockExtent;
		uint32_t blockY = (m_NextBlock / blocksPerSide) * s_ThumbnailBlockExtent;
		m_NextBlock++;

		uint32_t perSide = s_ThumbnailBlockExtent / quantizedSize;
		for (uint32_t y = 0; y < perSide; y++) {
			for (uint32_t x = 0; x < perSide; x++) {
				m_FreeRects[classIndex].emplace_back(blockX + x * quantizedSize, blockY + y * quantizedSize, quantizedSize);
			}
		}

		outRect = m_FreeRects[classIndex].back();
		m_FreeRects[classIndex].pop_back();
		return true;
	}

	void ThumbnailCache::FreeRect(const Cori::Graphics::ThumbnailRect rect) {
		if (rect.size == 0) {
			return;
		}

		m_FreeRects[SizeClassIndex(rect.size)].emplace_back(rect);
	}

	ThumbnailHandle ThumbnailCache::RequestMesh(Cori::Core::AssetRef<Cori::Graphics::Mesh> mesh, const uint32_t requestedSize) {
		return RequestInternal(std::move(mesh), m_PreviewMaterial, requestedSize);
	}

	ThumbnailHandle ThumbnailCache::RequestMaterial(Cori::Core::AssetRef<Cori::Graphics::Material> material, const uint32_t requestedSize) {
		if (!material.IsInitialized()) {
			return s_InvalidThumbnail;
		}

		return RequestInternal(m_PreviewSphere, std::move(material), requestedSize);
	}

	ThumbnailHandle ThumbnailCache::RequestTexture(Cori::Core::AssetRef<Cori::Graphics::Texture2> texture) {
		if (!texture.IsInitialized()) {
			return s_InvalidThumbnail;
		}

		if (m_FreeHandles.empty() && !EvictOne()) {
			return s_InvalidThumbnail;
		}

		const ThumbnailHandle handle = m_FreeHandles.back();
		m_FreeHandles.pop_back();

		ClearEntry(handle);

		m_Entries.lastTouchedFrames[handle] = m_Frame;
		m_Entries.states[handle] = State::ePending;
		m_Entries.subjects[handle].emplace<s_SubjectTexture>(std::move(texture));

		const auto& subject = std::get<s_SubjectTexture>(m_Entries.subjects[handle]);
		CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Request texture: handle={} texture=[{},{}]", handle, subject.GetHandle().GetIndex(), subject.GetHandle().GetVersion());

		return handle;
	}

	void ThumbnailCache::TickTextureEntries() {
		for (uint32_t i = 0; i < s_MaxThumbnails; i++) {
			if (m_Entries.states[i] == State::eFree || m_Entries.subjects[i].index() != s_SubjectTexture) {
				continue;
			}

			Cori::Core::Handle<Cori::Graphics::Texture2> handle = std::get<s_SubjectTexture>(m_Entries.subjects[i]).GetHandle();

			if (!IsSettled(Cori::Graphics::VulkanTextureManager::GetAssetStatus(handle))) {
				continue;
			}

			if (Cori::Graphics::VulkanTextureManager::GetImGuiBinding(handle)) {
				m_Entries.states[i] = State::eReady;
				continue;
			}

			m_Entries.states[i] = State::ePending;
			auto _ = Cori::Graphics::VulkanTextureManager::RequestImGuiBinding(handle);
		}
	}

	void ThumbnailCache::ClearEntry(const ThumbnailHandle handle) {
		m_Entries.states[handle] = State::eFree;
		m_Entries.dirty[handle] = false;
		m_Entries.dispatched[handle] = false;
		m_Entries.lastTouchedFrames[handle] = 0;
		m_Entries.rects[handle] = {};
		m_Entries.requestedSizes[handle] = 0;
		m_Entries.meshIdentities[handle] = 0;
		m_Entries.materialIdentities[handle] = 0;
		m_Entries.subjects[handle].emplace<s_SubjectNone>();
		m_Entries.deps[handle] = {};
		m_Entries.depIdentities[handle] = {};
	}

	ThumbnailHandle ThumbnailCache::RequestInternal(Cori::Core::AssetRef<Cori::Graphics::Mesh> mesh, Cori::Core::AssetRef<Cori::Graphics::Material> material, const uint32_t requestedSize) {
		if (!mesh.IsInitialized()) {
			return s_InvalidThumbnail;
		}

		if (m_FreeHandles.empty() && !EvictOne()) {
			return s_InvalidThumbnail;
		}

		ThumbnailHandle handle = m_FreeHandles.back();
		m_FreeHandles.pop_back();

		ClearEntry(handle);

		m_Entries.subjects[handle].emplace<s_SubjectRendered>(RenderedSubject{std::move(mesh), std::move(material)});
		m_Entries.requestedSizes[handle] = QuantizeSize(requestedSize);
		m_Entries.lastTouchedFrames[handle] = m_Frame;
		m_Entries.dirty[handle] = true;

		if (!AllocateRect(m_Entries.requestedSizes[handle], m_Entries.rects[handle])) {
			if (!EvictOne() || !AllocateRect(m_Entries.requestedSizes[handle], m_Entries.rects[handle])) {
				m_Entries.states[handle] = State::eOutOfSpace;
				CORI_CORE_WARN_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Out of atlas space for handle={} size={} (blocks used {}/{})",
				               handle, m_Entries.requestedSizes[handle], m_NextBlock, (Cori::Graphics::s_ThumbnailAtlasExtent / s_ThumbnailBlockExtent) * (Cori::Graphics::s_ThumbnailAtlasExtent / s_ThumbnailBlockExtent));
				return handle;
			}
		}

		m_Entries.states[handle] = State::ePending;

		auto& [subjectMesh, subjectMaterial] = std::get<s_SubjectRendered>(m_Entries.subjects[handle]);
		CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Request: handle={} mesh=[{},{}] material=[{},{}] requestedSize={} rect=({},{} {})px", handle, subjectMesh.GetHandle().GetIndex(), subjectMesh.GetHandle().GetVersion(), subjectMaterial.GetHandle().GetIndex(), subjectMaterial.GetHandle().GetVersion(), m_Entries.requestedSizes[handle], m_Entries.rects[handle].x, m_Entries.rects[handle].y, m_Entries.rects[handle].size);

		return handle;
	}

	std::optional<ThumbnailCache::Placement> ThumbnailCache::TryGetPlacement(const ThumbnailHandle handle) {
		if (handle >= s_MaxThumbnails) {
			return std::nullopt;
		}

		if (m_Entries.states[handle] == State::eFree) {
			return std::nullopt;
		}

		m_Entries.lastTouchedFrames[handle] = m_Frame;

		if (m_Entries.states[handle] != State::eReady) {
			return std::nullopt;
		}

		if (m_Entries.subjects[handle].index() == s_SubjectTexture) {
			std::optional<ImTextureID> binding = Cori::Graphics::VulkanTextureManager::GetImGuiBinding(std::get<s_SubjectTexture>(m_Entries.subjects[handle]).GetHandle());
			if (!binding) {
				return std::nullopt;
			}

			return Placement{
				.texture = binding.value(),
				.uv0 = ImVec2(0.0f, 0.0f),
				.uv1 = ImVec2(1.0f, 1.0f)
			};
		}

		std::optional<ImTextureID> texture = Cori::Graphics::ThumbnailAtlas::GetTexture();
		if (!texture) {
			return std::nullopt;
		}

		static constexpr auto atlasExtent = static_cast<float>(Cori::Graphics::s_ThumbnailAtlasExtent);
		Cori::Graphics::ThumbnailRect rect = m_Entries.rects[handle];

		return Placement{
			.texture = texture.value(),
			.uv0 = ImVec2(static_cast<float>(rect.x) / atlasExtent, static_cast<float>(rect.y) / atlasExtent),
			.uv1 = ImVec2(static_cast<float>(rect.x + rect.size) / atlasExtent, static_cast<float>(rect.y + rect.size) / atlasExtent)
		};
	}

	ThumbnailCache::State ThumbnailCache::GetState(const ThumbnailHandle handle) const {
		if (handle >= s_MaxThumbnails) {
			return State::eFree;
		}

		return m_Entries.states[handle];
	}

	bool ThumbnailCache::Resize(const ThumbnailHandle handle, const uint32_t newSize) {
		if (handle >= s_MaxThumbnails || m_Entries.states[handle] == State::eFree) {
			return false;
		}

		if (m_Entries.subjects[handle].index() == s_SubjectTexture) {
			return true;
		}

		const uint32_t quantized = QuantizeSize(newSize);
		if (quantized == m_Entries.requestedSizes[handle] && m_Entries.states[handle] != State::eOutOfSpace) {
			return true;
		}

		CancelInFlight(handle);

		FreeRect(m_Entries.rects[handle]);
		m_Entries.rects[handle] = {};
		m_Entries.requestedSizes[handle] = quantized;

		if (!AllocateRect(quantized, m_Entries.rects[handle])) {
			m_Entries.states[handle] = State::eOutOfSpace;
			return true;
		}

		m_Entries.states[handle] = State::ePending;
		m_Entries.dirty[handle] = true;
		m_Entries.dispatched[handle] = false;
		return true;
	}

	bool ThumbnailCache::Refresh(const ThumbnailHandle handle) {
		if (handle >= s_MaxThumbnails || m_Entries.states[handle] == State::eFree) {
			return false;
		}

		m_Entries.dirty[handle] = true;
		m_Entries.dispatched[handle] = false;
		return true;
	}

	void ThumbnailCache::Release(const ThumbnailHandle handle) {
		if (handle >= s_MaxThumbnails || m_Entries.states[handle] == State::eFree) {
			return;
		}

		CancelInFlight(handle);
		FreeRect(m_Entries.rects[handle]);
		ClearEntry(handle);

		m_FreeHandles.emplace_back(handle);
	}

	void ThumbnailCache::RefreshAll() {
		uint32_t flushed = 0;

		for (uint32_t i = 0; i < s_MaxThumbnails; i++) {
			if (m_Entries.states[i] == State::eFree || m_Entries.subjects[i].index() != s_SubjectRendered) {
				continue;
			}

			CancelInFlight(i);

			m_Entries.dirty[i] = true;
			m_Entries.dispatched[i] = false;
			flushed++;
		}

		CORI_CORE_DEBUG_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Flush: {} rendered thumbnails queued for re-render", flushed);
	}

	void ThumbnailCache::ReleaseAll() {
		CORI_PROFILE_FUNCTION();
		for (uint32_t handle = 0; handle < s_MaxThumbnails; handle++) {
			Release(handle);
		}
	}

	bool ThumbnailCache::EvictOne() {
		ThumbnailHandle oldest = s_InvalidThumbnail;
		uint64_t oldestFrame = UINT64_MAX;

		for (uint32_t i = 0; i < s_MaxThumbnails; i++) {
			if (m_Entries.states[i] == State::eFree || m_Entries.lastTouchedFrames[i] >= m_Frame) {
				continue;
			}

			if (m_Entries.lastTouchedFrames[i] < oldestFrame) {
				oldestFrame = m_Entries.lastTouchedFrames[i];
				oldest = i;
			}
		}

		if (oldest == s_InvalidThumbnail) {
			return false;
		}

		Release(oldest);
		return true;
	}

	bool ThumbnailCache::IsSettled(const Cori::AssetStatus status) {
		switch (status) {
		case Cori::AssetStatus::eLoading:
		case Cori::AssetStatus::eLoadQueued:
		case Cori::AssetStatus::eStreaming:
		case Cori::AssetStatus::eStreamingQueued:
			return false;
		default:
			return true;
		}
	}

	bool ThumbnailCache::IsEntryReadyToRender(const ThumbnailHandle handle) {
		if (m_Entries.subjects[handle].index() != s_SubjectRendered) {
			return false;
		}

		auto& [mesh, material] = std::get<s_SubjectRendered>(m_Entries.subjects[handle]);

		if (!mesh.IsInitialized() || !material.IsInitialized()) {
			return false;
		}

		if (!IsSettled(Cori::Graphics::VulkanMeshManager::GetAssetStatus(mesh.GetHandle()))) {
			return false;
		}

		if (!IsSettled(Cori::Graphics::VulkanMaterialSystem::GetAssetStatus(material.GetHandle()))) {
			return false;
		}

		for (const Cori::Core::AssetDependency& dep : m_Entries.deps[handle].View()) {
			if (!IsSettled(Cori::Core::AssetManager2::GetDependencyStatus(dep))) {
				return false;
			}
		}

		return Cori::Graphics::VulkanMeshManager::GetAABB3D(mesh.GetHandle()).has_value();
	}

	bool ThumbnailCache::IsEntryStale(const ThumbnailHandle handle) {
		if (m_Entries.subjects[handle].index() != s_SubjectRendered) {
			return false;
		}

		auto& [mesh, material] = std::get<s_SubjectRendered>(m_Entries.subjects[handle]);

		if (!mesh.IsInitialized()) {
			return false;
		}

		const uint32_t meshIdentity = Cori::Graphics::VulkanMeshManager::GetIdentityVersion(mesh.GetHandle());
		if (meshIdentity != m_Entries.meshIdentities[handle]) {
			CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Stale: mesh=[{},{}] identity {} -> {}", mesh.GetHandle().GetIndex(), mesh.GetHandle().GetVersion(), m_Entries.meshIdentities[handle], meshIdentity);

			return true;
		}

		if (material.IsInitialized()) {
			const uint32_t materialIdentity = Cori::Graphics::VulkanMaterialSystem::GetIdentityVersion(material.GetHandle());
			if (materialIdentity != m_Entries.materialIdentities[handle]) {
				CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Stale: material=[{},{}] identity {} -> {}", material.GetHandle().GetIndex(), material.GetHandle().GetVersion(), m_Entries.materialIdentities[handle], materialIdentity);

				return true;
			}
		}

		Cori::Core::AssetDependencySet& deps = m_Entries.deps[handle];
		for (uint32_t i = 0; i < deps.count; i++) {
			const Cori::Core::AssetDependency& dep = deps.deps[i];
			const uint32_t current = Cori::Core::AssetManager2::GetDependencyIdentityVersion(dep);

			if (current != m_Entries.depIdentities[handle][i]) {
				CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Stale: dependency typeHash={} handle=[{},{}] identity {} -> {}", dep.typeHash, dep.index, dep.version, m_Entries.depIdentities[handle][i], current);

				return true;
			}
		}

		return false;
	}

	void ThumbnailCache::SnapshotDependencies(const ThumbnailHandle handle) {
		if (m_Entries.subjects[handle].index() != s_SubjectRendered) {
			return;
		}

		auto& [mesh, material] = std::get<s_SubjectRendered>(m_Entries.subjects[handle]);
		Cori::Core::AssetDependencySet& deps = m_Entries.deps[handle];
		deps = {};

		const auto Append = [&deps](const Cori::Core::AssetDependencySet& source) {
			for (const Cori::Core::AssetDependency& dep : source.View()) {
				if (deps.count >= Cori::Core::s_MaxAssetDependencies) {
					return;
				}

				deps.deps[deps.count] = dep;
				deps.count++;
			}
		};

		m_Entries.meshIdentities[handle] = Cori::Graphics::VulkanMeshManager::GetIdentityVersion(mesh.GetHandle());
		if (const auto meshDependencies = Cori::Graphics::VulkanMeshManager::TryReadDependencies(mesh.GetHandle())) {
			m_Entries.meshIdentities[handle] = meshDependencies->second;
			Append(meshDependencies->first);
		}

		if (material.IsInitialized()) {
			m_Entries.materialIdentities[handle] = Cori::Graphics::VulkanMaterialSystem::GetIdentityVersion(material.GetHandle());

			if (const auto materialDependencies = Cori::Graphics::VulkanMaterialSystem::TryReadDependencies(material.GetHandle())) {
				m_Entries.materialIdentities[handle] = materialDependencies->second;
				Append(materialDependencies->first);
			}
		}

		m_Entries.depIdentities[handle] = {};
		for (uint32_t i = 0; i < deps.count; i++) {
			m_Entries.depIdentities[handle][i] = Cori::Core::AssetManager2::GetDependencyIdentityVersion(deps.deps[i]);
		}
	}

	void ThumbnailCache::EnsurePreviewSlots() {
		if (m_PreviewSlotsCreated) {
			return;
		}

		m_PreviewSlotsCreated = true;

		for (uint32_t i = 0; i < s_PreviewSlotCount; i++) {
			PreviewSlot& slot = m_PreviewSlots[i];

			const std::string name = std::format("ThumbnailPreview_{}", i);
			const auto scene = Cori::World::SceneManager::CreateScene(name);
			if (!scene) {
				CORI_CORE_ERROR_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Failed to create thumbnail preview scene '{}'. Error: {}", name, scene.error().what());
				continue;
			}

			slot.scene = scene.value();

			auto& camera = slot.scene.GetActiveCamera();
			camera.CreatePerspectiveCamera(s_PreviewFovY, 1.0f, s_PreviewNearPlane, s_PreviewFarPlane);
			camera.SetPosition3D(glm::vec3{-s_PreviewCameraDistance, 0.0f, 0.0f});
			camera.SetYawPitch(0.0f, 0.0f);
			camera.RecalculateVP();

			Cori::Graphics::SceneRenderer::CreateInfo info{
				.initialPRTExtent = vk::Extent2D{s_ThumbnailBlockExtent, s_ThumbnailBlockExtent},
				.PRTFormat = vk::Format::eR8G8B8A8Unorm,
				#ifdef DEBUG_BUILD
				.name = name,
				#endif
				.registerPRTWithImGui = false
			};

			slot.scene.RegisterSystem<Cori::World::Systems::RenderSync>(std::move(info));

			const auto renderSync = slot.scene.GetSystem<Cori::World::Systems::RenderSync>();
			if (!renderSync) {
				CORI_CORE_ERROR_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Failed to retrieve RenderSync for thumbnail preview scene '{}'. Error: {}", name, renderSync.error().what());
				continue;
			}

			slot.renderSync = renderSync.value();

			auto locked = slot.renderSync.lock();
			if (locked) {
				slot.renderer = locked->GetRendererHandle();
				locked->Sleep();
			}

			slot.entity = slot.scene.CreateEntity(std::format("ThumbnailSubject_{}", i));
			slot.entity.AddComponent<Cori::World::Components::Entity::Rendering>(
				Cori::Core::AssetRef<Cori::Graphics::Mesh>{Cori::Core::Internal::EmptyRef},
				Cori::Core::AssetRef<Cori::Graphics::Material>(m_PreviewMaterial),
				glm::vec4{0.0f, 0.0f, 1.0f, 1.0f});

			slot.valid = true;

			slot.background = slot.scene.CreateEntity("Background");
			slot.background.AddComponent<Cori::World::Components::Entity::Rendering>(
				Cori::Core::AssetRef<Cori::Graphics::Mesh>{Cori::Graphics::VulkanMeshManager::GetPlaceholder<Cori::Graphics::Mesh>()},
				Cori::Core::AssetRef<Cori::Graphics::Material>(m_BackgroundMaterial),
				glm::vec4{0.0f, 0.0f, 16.0f, 16.0f});


			auto& tc = slot.background.GetComponents<Cori::World::Components::Entity::Transform>();
			tc.SetLocalPosition({115.0f, 0.0f, 0.0f});
			tc.SetLocalScale({100.0f, 100.0f, 100.0f});

			CORI_CORE_INFO_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Preview slot {} ready: renderer={} prtExtent={}px", i, slot.renderer, s_ThumbnailBlockExtent);
		}
	}

	std::pair<float, glm::vec3> ThumbnailCache::GetPreviewRotation(const float yawDegrees, const float pitchDegrees) {
		const float yaw = glm::radians(yawDegrees);
		const float pitch = glm::radians(pitchDegrees);
		const float cosPitch = std::cos(pitch);

		const glm::vec3 viewDirection = glm::normalize(glm::vec3{cosPitch * std::cos(yaw), cosPitch * std::sin(yaw), std::sin(pitch)});
		const glm::vec3 cameraForward{1.0f, 0.0f, 0.0f};

		const glm::vec3 cross = glm::cross(viewDirection, cameraForward);
		const float sine = glm::length(cross);

		if (sine < 0.0001f) {
			return {0.0f, glm::vec3{0.0f, 0.0f, 1.0f}};
		}

		return {glm::degrees(std::atan2(sine, glm::dot(viewDirection, cameraForward))), cross / sine};
	}

	void ThumbnailCache::FrameEntitySubject(PreviewSlot& slot, const Cori::Core::Handle<Cori::Graphics::Mesh> mesh) {
		auto aabb = Cori::Graphics::VulkanMeshManager::GetAABB3D(mesh);
		if (!aabb) {
			return;
		}

		float yaw = s_PreviewYaw;
		float pitch = s_PreviewPitch;

		if (m_PreviewSphere.GetHandle() == mesh) {
			yaw = 90.0f;
			pitch = 90.0f;
		}

		glm::vec3 center{aabb->bxCenter, aabb->byCenter, aabb->bzCenter};
		glm::vec3 extent{std::abs(aabb->bxExtent), std::abs(aabb->byExtent), std::abs(aabb->bzExtent)};

		auto [angle, axis] = GetPreviewRotation(yaw, pitch);
		glm::mat3 rotation{glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis)};

		std::array<glm::vec3, 8> corners{};
		uint32_t cornerCount = 0;
		for (int32_t signX = -1; signX <= 1; signX += 2) {
			for (int32_t signY = -1; signY <= 1; signY += 2) {
				for (int32_t signZ = -1; signZ <= 1; signZ += 2) {
					corners[cornerCount] = rotation * glm::vec3{static_cast<float>(signX) * extent.x, static_cast<float>(signY) * extent.y, static_cast<float>(signZ) * extent.z};
					cornerCount++;
				}
			}
		}

		float nearestCorner = 0.0f;
		for (const glm::vec3& corner : corners) {
			nearestCorner = std::min(nearestCorner, corner.x);
		}

		static constexpr float aspect = 1.0f;
		float tanHalfFovY = std::tan(glm::radians(s_PreviewFovY) * 0.5f);
		float tanHalfFovX = tanHalfFovY * aspect;

		float leftReach = std::numeric_limits<float>::lowest();
		float rightReach = std::numeric_limits<float>::lowest();
		float bottomReach = std::numeric_limits<float>::lowest();
		float topReach = std::numeric_limits<float>::lowest();

		for (const auto& corner : corners) {
			leftReach = std::max(leftReach, -corner.y + corner.x * tanHalfFovX);
			rightReach = std::max(rightReach, corner.y + corner.x * tanHalfFovX);
			bottomReach = std::max(bottomReach, corner.z + corner.x * tanHalfFovY);
			topReach = std::max(topReach, -corner.z + corner.x * tanHalfFovY);
		}

		float horizontalFit = 2.0f * s_PreviewCameraDistance * tanHalfFovX / std::max(leftReach + rightReach, 0.0001f);
		float verticalFit = 2.0f * s_PreviewCameraDistance * tanHalfFovY / std::max(bottomReach + topReach, 0.0001f);

		float scale = std::min(horizontalFit, verticalFit);

		if (nearestCorner < 0.0f) {
			scale = std::min(scale, (s_PreviewCameraDistance - s_PreviewNearPlane) / -nearestCorner);
		}

		float cameraX = scale * (leftReach - rightReach) * 0.5f;
		float cameraY = scale * (bottomReach - topReach) * 0.5f;

		glm::vec3 position = glm::vec3{0.0f, -cameraX, cameraY} - scale * (rotation * center);

		auto& tc = slot.entity.GetComponents<Cori::World::Components::Entity::Transform>();
		tc.SetLocalRotation(angle, axis);
		tc.SetLocalScale(glm::vec3{scale});
		tc.SetLocalPosition(position);

		CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Frame the subject: mesh=[{},{}] aabbExtent=({:.3f}, {:.3f}, {:.3f}) angle={:.2f} axis=({:.3f}, {:.3f}, {:.3f}) scale={:.4f} position=({:.3f}, {:.3f}, {:.3f}) nearestDepth={:.4f}", mesh.GetIndex(), mesh.GetVersion(), extent.x, extent.y, extent.z, angle, axis.x, axis.y, axis.z, scale, position.x, position.y, position.z, s_PreviewCameraDistance + scale * nearestCorner);
	}

	void ThumbnailCache::DispatchTo(PreviewSlot& slot, const ThumbnailHandle handle) {
		auto& [mesh, material] = std::get<s_SubjectRendered>(m_Entries.subjects[handle]);

		auto& rc = slot.entity.GetComponents<Cori::World::Components::Entity::Rendering>();
		rc.ChangeMesh(mesh);
		rc.ChangeMaterial(material);

		FrameEntitySubject(slot, mesh.GetHandle());

		if (const auto locked = slot.renderSync.lock()) {
			locked->WakeUp();
			locked->RequestThumbnailCopy(m_Entries.rects[handle]);
			slot.copyCountAtDispatch = locked->GetThumbnailCopyCount();
		}

		m_Entries.dirty[handle] = false;
		m_Entries.dispatched[handle] = true;

		slot.inFlight = handle;

		CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Dispatch: handle={} -> renderer={} rect=({},{} {}px) frame={}", handle, slot.renderer, m_Entries.rects[handle].x, m_Entries.rects[handle].y, m_Entries.rects[handle].size, m_Frame);
	}

	void ThumbnailCache::LogCensus() {
		uint32_t pending = 0;
		uint32_t ready = 0;
		uint32_t outOfSpace = 0;
		uint32_t dirtyNotReady = 0;
		uint32_t inFlight = 0;

		for (uint32_t i = 0; i < s_MaxThumbnails; i++) {
			switch (m_Entries.states[i]) {
			case State::ePending: pending++;
				break;
			case State::eReady: ready++;
				break;
			case State::eOutOfSpace: outOfSpace++;
				break;
			default: continue;
			}

			if (m_Entries.dirty[i] && !m_Entries.dispatched[i] && !IsEntryReadyToRender(i)) {
				dirtyNotReady++;
			}
		}

		for (const PreviewSlot& slot : m_PreviewSlots) {
			if (slot.inFlight != s_InvalidThumbnail) {
				inFlight++;
			}
		}

		CORI_CORE_DEBUG_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Census frame={}: pending={} ready={} outOfSpace={} blockedOnAssets={} slotsBusy={}/{} atlasBlocksUsed={}", m_Frame, pending, ready, outOfSpace, dirtyNotReady, inFlight, s_PreviewSlotCount, m_NextBlock);
	}

	void ThumbnailCache::CancelInFlight(const ThumbnailHandle handle) {
		for (PreviewSlot& slot : m_PreviewSlots) {
			if (slot.inFlight != handle) {
				continue;
			}

			slot.inFlight = s_InvalidThumbnail;

			if (const auto locked = slot.renderSync.lock()) {
				locked->Sleep();
			}
		}
	}

	void ThumbnailCache::Tick() {
		CORI_PROFILE_FUNCTION();
		m_Frame++;

		EnsurePreviewSlots();

		TickTextureEntries();

		for (PreviewSlot& slot : m_PreviewSlots) {
			if (slot.inFlight == s_InvalidThumbnail) {
				continue;
			}

			const auto locked = slot.renderSync.lock();
			if (!locked || locked->GetThumbnailCopyCount() == slot.copyCountAtDispatch) {
				continue;
			}

			if (m_Entries.states[slot.inFlight] != State::eFree) {
				m_Entries.states[slot.inFlight] = State::eReady;
			}

			Cori::Graphics::ThumbnailRect rect = m_Entries.rects[slot.inFlight];
			CORI_CORE_TRACE_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::ThumbnailCache }, "Complete: handle={} rect=({},{} {}px) frame={}", slot.inFlight, rect.x, rect.y, rect.size, m_Frame);

			slot.inFlight = s_InvalidThumbnail;
			locked->Sleep();
		}

		m_QueueScratch.clear();

		for (uint32_t i = 0; i < s_MaxThumbnails; i++) {
			if (m_Entries.states[i] == State::eFree || m_Entries.states[i] == State::eOutOfSpace || m_Entries.subjects[i].index() != s_SubjectRendered) {
				continue;
			}

			if (!m_Entries.dirty[i] && m_Entries.dispatched[i] && IsEntryStale(i)) {
				m_Entries.dirty[i] = true;
				m_Entries.dispatched[i] = false;
			}

			if (m_Entries.dirty[i] && !m_Entries.dispatched[i]) {
				SnapshotDependencies(i);

				if (IsEntryReadyToRender(i)) {
					m_QueueScratch.emplace_back(i);
				}
			}
		}

		std::ranges::sort(m_QueueScratch, [this](const ThumbnailHandle a, const ThumbnailHandle b) {
			return m_Entries.lastTouchedFrames[a] > m_Entries.lastTouchedFrames[b];
		});

		#ifdef DEBUG_BUILD
		if (m_Frame - m_LastCensusFrame >= 120) {
			m_LastCensusFrame = m_Frame;
			LogCensus();
		}
		#endif

		uint32_t dispatched = 0;
		for (PreviewSlot& slot : m_PreviewSlots) {
			if (dispatched >= m_QueueScratch.size()) {
				break;
			}

			if (!slot.valid || slot.inFlight != s_InvalidThumbnail) {
				continue;
			}

			DispatchTo(slot, m_QueueScratch[dispatched]);
			dispatched++;
		}
	}

	void ThumbnailCache::OnUpdate(Cori::Core::GameTimer& gameTimer) {
		for (auto& slot : m_PreviewSlots) {
			if (slot.scene.IsValid()) {
				slot.scene.OnUpdate(gameTimer);
			}
		}
	}

	void ThumbnailCache::OnTickUpdate(Cori::Core::GameTimer& gameTimer) {
		for (auto& slot : m_PreviewSlots) {
			if (slot.scene.IsValid()) {
				slot.scene.OnTickUpdate(gameTimer);
			}
		}
	}
}
