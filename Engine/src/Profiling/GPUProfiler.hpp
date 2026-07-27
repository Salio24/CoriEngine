#pragma once

#ifdef CORI_ENABLE_PROFILING
#include <tracy/TracyVulkan.hpp>

namespace Cori {
	using GPUProfilerContext = tracy::VkCtx*;
}

#define CORI_PROFILE_GPU_CONTEXT(physicalDevice, device, resetQueryPool, getTimeDomains, getTimestamps) TracyVkContextHostCalibrated(static_cast<VkPhysicalDevice>(physicalDevice), static_cast<VkDevice>(device), resetQueryPool, getTimeDomains, getTimestamps)
#define CORI_PROFILE_GPU_CONTEXT_NAME(ctx, name) do { if (ctx) { TracyVkContextName(ctx, name, static_cast<uint16_t>(sizeof(name) - 1)); } } while(0)
#define CORI_PROFILE_GPU_CONTEXT_DESTROY(ctx) do { if (ctx) { TracyVkDestroy(ctx); (ctx) = nullptr; } } while(0)
#define CORI_PROFILE_GPU_COLLECT(ctx) do { if (ctx) { TracyVkCollectHost(ctx); } } while(0)

#define CORI_PROFILE_GPU_ZONE(ctx, cmdbuf, name) SuppressVarShadowWarning( TracyVkNamedZone(ctx, ___cori_gpu_zone, static_cast<VkCommandBuffer>(cmdbuf), name, (ctx) != nullptr) )
#define CORI_PROFILE_GPU_ZONE_C(ctx, cmdbuf, name, color) SuppressVarShadowWarning( TracyVkNamedZoneC(ctx, ___cori_gpu_zone, static_cast<VkCommandBuffer>(cmdbuf), name, color, (ctx) != nullptr) )
#define CORI_PROFILE_GPU_ZONE_CP(part, ctx, cmdbuf, name, color) SuppressVarShadowWarning( TracyVkNamedZoneC(ctx, ___cori_gpu_zone, static_cast<VkCommandBuffer>(cmdbuf), name, color, (ctx) != nullptr && CORI_PROFILE_PART_ENABLED(part)) )
#define CORI_PROFILE_GPU_ZONE_DYNAMIC_NAME_CP(part, ctx, cmdbuf, name, color) std::string_view TracyConcat(__cori_gpu_name, TracyLine){ name }; SuppressVarShadowWarning( tracy::VkCtxScope ___cori_gpu_zone(ctx, TracyLine, TracyFile, strlen(TracyFile), TracyFunction, strlen(TracyFunction), TracyConcat(__cori_gpu_name, TracyLine).data(), TracyConcat(__cori_gpu_name, TracyLine).size(), static_cast<VkCommandBuffer>(cmdbuf), (ctx) != nullptr && CORI_PROFILE_PART_ENABLED(part)) )

namespace Cori {
	class GPUProfilerSpanZone {
	public:
		void Begin(const GPUProfilerContext ctx, const vk::CommandBuffer cmdbuf, const tracy::SourceLocationData* srcloc) {
			m_Zone.emplace(ctx, srcloc, static_cast<VkCommandBuffer>(cmdbuf), ctx != nullptr);
		}

		void End() {
			m_Zone.reset();
		}

		[[nodiscard]] bool IsOpen() const {
			return m_Zone.has_value();
		}

	private:
		std::optional<tracy::VkCtxScope> m_Zone;
	};
}

#define CORI_PROFILE_GPU_SPAN_BEGIN_C(span, ctx, cmdbuf, name, color) do { static constexpr tracy::SourceLocationData __cori_gpu_span_srcloc{ name, TracyFunction, TracyFile, static_cast<uint32_t>(TracyLine), color }; (span).Begin(ctx, cmdbuf, &__cori_gpu_span_srcloc); } while(0)
#define CORI_PROFILE_GPU_SPAN_END(span) (span).End()

#else

namespace Cori {
	using GPUProfilerContext = void*;

	class GPUProfilerSpanZone {
	public:
		[[nodiscard]] bool IsOpen() const { return false; }
	};
}

#define CORI_PROFILE_GPU_CONTEXT(physicalDevice, device, resetQueryPool, getTimeDomains, getTimestamps) nullptr
#define CORI_PROFILE_GPU_CONTEXT_NAME(ctx, name)
#define CORI_PROFILE_GPU_CONTEXT_DESTROY(ctx)
#define CORI_PROFILE_GPU_COLLECT(ctx)

#define CORI_PROFILE_GPU_ZONE(ctx, cmdbuf, name)
#define CORI_PROFILE_GPU_ZONE_C(ctx, cmdbuf, name, color)
#define CORI_PROFILE_GPU_ZONE_CP(part, ctx, cmdbuf, name, color)
#define CORI_PROFILE_GPU_ZONE_DYNAMIC_NAME_CP(part, ctx, cmdbuf, name, color)

#define CORI_PROFILE_GPU_SPAN_BEGIN_C(span, ctx, cmdbuf, name, color)
#define CORI_PROFILE_GPU_SPAN_END(span)

#endif

namespace Cori {
	namespace ProfileColors {
		inline constexpr uint32_t GPUFrame    = 0x304878; // dk blue  - whole frame command buffer
		inline constexpr uint32_t GPUPass     = 0x4878B0; // md blue  - render graph pass
		inline constexpr uint32_t GPUTransfer = 0x108848; // dk green - streaming line copy
		inline constexpr uint32_t GPUBarrier  = 0x986018; // brown    - barriers / layout transitions
		inline constexpr uint32_t GPUSync     = 0x585858; // dk grey  - per-frame sync pumps
	}
}
