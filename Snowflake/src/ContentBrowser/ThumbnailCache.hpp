#pragma once
#include <imgui.h>
#include "Graphics/Vulkan/Renderer/ThumbnailAtlas.hpp"
#include "Graphics/Vulkan/VulkanMeshManager.hpp"
#include "Graphics/Vulkan/VulkanMaterialSystem.hpp"
#include "Graphics/Vulkan/VulkanTextureManager.hpp"
#include "Core/AssetManager/AssetDependency.hpp"
#include "Graphics/CameraController.hpp"
#include "WorldSystem/SceneHandle.hpp"
#include "WorldSystem/Entity.hpp"

namespace Snowflake {
	using ThumbnailHandle = uint32_t;

	class ThumbnailCache {
	public:
		enum class State : uint8_t {
			eFree,
			ePending,
			eReady,
			eOutOfSpace
		};

		struct Placement {
			ImTextureID texture{};
			ImVec2 uv0{};
			ImVec2 uv1{};
		};

		[[nodiscard]] static std::shared_ptr<ThumbnailCache> Get();

		~ThumbnailCache();

		[[nodiscard]] ThumbnailHandle RequestMesh(Cori::Core::AssetRef<Cori::Graphics::Mesh> mesh, const uint32_t requestedSize);

		[[nodiscard]] ThumbnailHandle RequestMaterial(Cori::Core::AssetRef<Cori::Graphics::Material> material, const uint32_t requestedSize);

		[[nodiscard]] ThumbnailHandle RequestTexture(Cori::Core::AssetRef<Cori::Graphics::Texture2> texture);

		[[nodiscard]] std::optional<Placement> TryGetPlacement(const ThumbnailHandle handle);

		[[nodiscard]] State GetState(const ThumbnailHandle handle) const;

		bool Resize(const ThumbnailHandle handle, const uint32_t newSize);

		bool Refresh(const ThumbnailHandle handle);

		void Release(const ThumbnailHandle handle);

		void RefreshAll();

		void ReleaseAll();

		void Tick();
		void OnUpdate(Cori::Core::GameTimer& gameTimer);
		void OnTickUpdate(Cori::Core::GameTimer& gameTimer);

		void LogCensus();

		static constexpr ThumbnailHandle GetInvalidHandle();

	private:
		static constexpr ThumbnailHandle s_InvalidThumbnail{UINT32_MAX};

		static constexpr uint32_t s_ThumbnailSizeClasses[]{64, 128, 256, 512};
		static constexpr uint32_t s_ThumbnailSizeClassCount{4};
		static constexpr uint32_t s_ThumbnailBlockExtent{512};
		static constexpr uint32_t s_PreviewSlotCount{3};
		static constexpr uint32_t s_MaxThumbnails{512};
		static constexpr float s_PreviewFovY{35.0f};
		static constexpr float s_PreviewNearPlane{0.01f};
		static constexpr float s_PreviewFarPlane{1000.0f};
		static constexpr float s_PreviewCameraDistance{3.0f};
		static constexpr float s_PreviewYaw{20.0f};
		static constexpr float s_PreviewPitch{-20.0f};

		struct RenderedSubject {
			Cori::Core::AssetRef<Cori::Graphics::Mesh> mesh{Cori::Core::Internal::EmptyRef};
			Cori::Core::AssetRef<Cori::Graphics::Material> material{Cori::Core::Internal::EmptyRef};
		};

		using Subject = std::variant<std::monostate, RenderedSubject, Cori::Core::AssetRef<Cori::Graphics::Texture2>>;
		static constexpr uint64_t s_SubjectNone{0};
		static constexpr uint64_t s_SubjectRendered{1};
		static constexpr uint64_t s_SubjectTexture{2};

		struct EntryStorage {
			std::array<State, s_MaxThumbnails> states{};
			std::array<bool, s_MaxThumbnails> dirty{};
			std::array<bool, s_MaxThumbnails> dispatched{};
			std::array<uint64_t, s_MaxThumbnails> lastTouchedFrames{};
			std::array<Cori::Graphics::ThumbnailRect, s_MaxThumbnails> rects{};
			std::array<uint32_t, s_MaxThumbnails> requestedSizes{};
			std::array<uint32_t, s_MaxThumbnails> meshIdentities{};
			std::array<uint32_t, s_MaxThumbnails> materialIdentities{};
			std::array<Subject, s_MaxThumbnails> subjects{};
			std::array<Cori::Core::AssetDependencySet, s_MaxThumbnails> deps{};
			std::array<std::array<uint32_t, Cori::Core::s_MaxAssetDependencies>, s_MaxThumbnails> depIdentities{};
		};

		struct PreviewSlot {
			Cori::World::SceneHandle scene{nullptr};
			Cori::World::Entity entity{};
			Cori::World::Entity background{};
			std::weak_ptr<Cori::World::Systems::RenderSync> renderSync;
			Cori::Graphics::SceneRendererHandle renderer{0};
			ThumbnailHandle inFlight{s_InvalidThumbnail};
			uint64_t copyCountAtDispatch{0};
			bool valid{false};
		};

		ThumbnailCache();

		[[nodiscard]] ThumbnailHandle RequestInternal(Cori::Core::AssetRef<Cori::Graphics::Mesh> mesh, Cori::Core::AssetRef<Cori::Graphics::Material> material, const uint32_t requestedSize);

		[[nodiscard]] static uint32_t QuantizeSize(const uint32_t requestedSize);

		[[nodiscard]] static uint32_t SizeClassIndex(const uint32_t quantizedSize);

		[[nodiscard]] bool AllocateRect(const uint32_t quantizedSize, Cori::Graphics::ThumbnailRect& outRect);

		void FreeRect(const Cori::Graphics::ThumbnailRect rect);

		[[nodiscard]] static bool IsSettled(const Cori::AssetStatus status);

		[[nodiscard]] bool IsEntryReadyToRender(const ThumbnailHandle handle);

		[[nodiscard]] bool IsEntryStale(const ThumbnailHandle handle);

		void SnapshotDependencies(const ThumbnailHandle handle);

		void ClearEntry(const ThumbnailHandle handle);

		void TickTextureEntries();

		void EnsurePreviewSlots();

		void DispatchTo(PreviewSlot& slot, const ThumbnailHandle handle);

		void CancelInFlight(const ThumbnailHandle handle);

		[[nodiscard]] static std::pair<float, glm::vec3> GetPreviewRotation(const float yawDegrees, const float pitchDegrees);

		void FrameEntitySubject(PreviewSlot& slot, const Cori::Core::Handle<Cori::Graphics::Mesh> mesh);

		[[nodiscard]] bool EvictOne();

		EntryStorage m_Entries{};
		std::vector<ThumbnailHandle> m_FreeHandles;

		std::array<std::vector<Cori::Graphics::ThumbnailRect>, s_ThumbnailSizeClassCount> m_FreeRects;
		uint32_t m_NextBlock{0};

		std::array<PreviewSlot, s_PreviewSlotCount> m_PreviewSlots{};
		bool m_PreviewSlotsCreated{false};

		std::vector<ThumbnailHandle> m_QueueScratch;

		uint64_t m_Frame{0};
		uint64_t m_LastCensusFrame{0};

		Cori::Core::AssetRef<Cori::Graphics::Material> m_BackgroundMaterial{Cori::Core::Internal::EmptyRef};
		Cori::Core::AssetRef<Cori::Graphics::Mesh> m_PreviewSphere{Cori::Core::Internal::EmptyRef};
		Cori::Core::AssetRef<Cori::Graphics::Material> m_PreviewMaterial{Cori::Core::Internal::EmptyRef};

		static std::weak_ptr<ThumbnailCache> s_Instance;
	};

	constexpr ThumbnailHandle ThumbnailCache::GetInvalidHandle() {
		return s_InvalidThumbnail;
	}
}
