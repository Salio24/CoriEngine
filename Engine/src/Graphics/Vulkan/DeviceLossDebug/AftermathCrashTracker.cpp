#include "AftermathCrashTracker.hpp"

#ifdef CORI_VK_DL_DEBUG_NVIDIA

#include "FileSystem/PathManager.hpp"
#include <nlohmann/json.hpp>

#include <vulkan/vulkan.h>
#include "GFSDK_Aftermath.h"
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"

namespace {
	constexpr uint32_t NVIDIA_VENDOR_ID{ 0x10DE };

	constexpr std::chrono::seconds CRASH_DUMP_TIMEOUT{ 3 };

	std::string ResultToString(const GFSDK_Aftermath_Result result) {
		switch (result) {
		case GFSDK_Aftermath_Result_Success: return "success";
		case GFSDK_Aftermath_Result_NotAvailable: return "not available";
		case GFSDK_Aftermath_Result_FAIL_VersionMismatch: return "version mismatch between the Aftermath SDK and the display driver";
		case GFSDK_Aftermath_Result_FAIL_NotInitialized: return "Aftermath is not initialized";
		case GFSDK_Aftermath_Result_FAIL_InvalidAdapter: return "invalid adapter, Aftermath requires an NVIDIA GPU";
		case GFSDK_Aftermath_Result_FAIL_InvalidParameter: return "invalid parameter";
		case GFSDK_Aftermath_Result_FAIL_ApiError: return "API error";
		case GFSDK_Aftermath_Result_FAIL_AlreadyInitialized: return "already initialized";
		case GFSDK_Aftermath_Result_FAIL_DriverInitFailed: return "driver initialization failed";
		case GFSDK_Aftermath_Result_FAIL_DriverVersionNotSupported: return "unsupported driver version, an R495 or newer NVIDIA display driver is required";
		case GFSDK_Aftermath_Result_FAIL_OutOfMemory: return "out of memory";
		case GFSDK_Aftermath_Result_FAIL_FeatureNotEnabled: return "feature not enabled, the device is missing the NV diagnostics extensions";
		case GFSDK_Aftermath_Result_FAIL_Disabled: return "Aftermath is disabled on this system";
		default: return std::format("error 0x{:08X}", static_cast<uint32_t>(result));
		}
	}

	std::string StatusToString(const GFSDK_Aftermath_CrashDump_Status status) {
		switch (status) {
		case GFSDK_Aftermath_CrashDump_Status_NotStarted: return "no GPU crash was detected by Aftermath";
		case GFSDK_Aftermath_CrashDump_Status_CollectingData: return "still collecting crash dump data";
		case GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed: return "collecting crash dump data failed";
		case GFSDK_Aftermath_CrashDump_Status_InvokingCallback: return "invoking the crash dump callback";
		case GFSDK_Aftermath_CrashDump_Status_Finished: return "finished";
		default: return std::format("unknown status {}", static_cast<uint32_t>(status));
		}
	}

	struct ShaderDebugInfoIdentifierLess {
		bool operator()(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& lhs, const GFSDK_Aftermath_ShaderDebugInfoIdentifier& rhs) const noexcept {
			return lhs.id[0] == rhs.id[0] ? lhs.id[1] < rhs.id[1] : lhs.id[0] < rhs.id[0];
		}
	};

	struct ShaderBinaryHashLess {
		bool operator()(const GFSDK_Aftermath_ShaderBinaryHash& lhs, const GFSDK_Aftermath_ShaderBinaryHash& rhs) const noexcept {
			return lhs.hash < rhs.hash;
		}
	};

	std::string IdentifierToString(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier) {
		return std::format("{:016x}-{:016x}", identifier.id[0], identifier.id[1]);
	}

	std::string_view DeviceStatusToString(const GFSDK_Aftermath_Device_Status status) {
		switch (status) {
		case GFSDK_Aftermath_Device_Status_Active: return "active";
		case GFSDK_Aftermath_Device_Status_Timeout: return "timeout, a long running shader or operation exceeded the driver timeout";
		case GFSDK_Aftermath_Device_Status_OutOfMemory: return "out of memory";
		case GFSDK_Aftermath_Device_Status_PageFault: return "page fault, an invalid GPU virtual address was accessed";
		case GFSDK_Aftermath_Device_Status_Stopped: return "stopped, the GPU stopped executing";
		case GFSDK_Aftermath_Device_Status_Reset: return "reset";
		default: return "unknown, the driver may be too old for this Aftermath feature";
		}
	}

	std::string_view FaultTypeToString(const GFSDK_Aftermath_FaultType type) {
		switch (type) {
		case GFSDK_Aftermath_FaultType_AddressTranslationError: return "address translation error";
		case GFSDK_Aftermath_FaultType_IllegalAccessError: return "illegal access error";
		default: return "unknown";
		}
	}

	std::string_view AccessTypeToString(const GFSDK_Aftermath_AccessType type) {
		switch (type) {
		case GFSDK_Aftermath_AccessType_Read: return "read";
		case GFSDK_Aftermath_AccessType_Write: return "write";
		case GFSDK_Aftermath_AccessType_Atomic: return "atomic";
		default: return "unknown";
		}
	}

	std::string_view EngineToString(const GFSDK_Aftermath_Engine engine) {
		switch (engine) {
		case GFSDK_Aftermath_Engine_Graphics: return "graphics";
		case GFSDK_Aftermath_Engine_GraphicsCompute: return "graphics/compute";
		case GFSDK_Aftermath_Engine_Display: return "display";
		case GFSDK_Aftermath_Engine_CopyEngine: return "copy";
		case GFSDK_Aftermath_Engine_VideoDecoder: return "video decoder";
		case GFSDK_Aftermath_Engine_VideoEncoder: return "video encoder";
		case GFSDK_Aftermath_Engine_Other: return "other";
		default: return "unknown";
		}
	}

	std::string_view ClientToString(const GFSDK_Aftermath_Client client) {
		switch (client) {
		case GFSDK_Aftermath_Client_HostInterface: return "host interface";
		case GFSDK_Aftermath_Client_FrontEnd: return "front end";
		case GFSDK_Aftermath_Client_PrimitiveDistributor: return "primitive distributor";
		case GFSDK_Aftermath_Client_GraphicsProcessingCluster: return "graphics processing cluster";
		case GFSDK_Aftermath_Client_PolymorphEngine: return "polymorph engine";
		case GFSDK_Aftermath_Client_RasterEngine: return "raster engine";
		case GFSDK_Aftermath_Client_Rasterizer2D: return "2D rasterizer";
		case GFSDK_Aftermath_Client_RenderOutputUnit: return "render output unit";
		case GFSDK_Aftermath_Client_TextureProcessingCluster: return "texture processing cluster";
		case GFSDK_Aftermath_Client_CopyEngine: return "copy engine";
		case GFSDK_Aftermath_Client_VideoDecoder: return "video decoder";
		case GFSDK_Aftermath_Client_VideoEncoder: return "video encoder";
		case GFSDK_Aftermath_Client_Other: return "other";
		default: return "unknown";
		}
	}

	std::string_view ResidencyToString(const GFSDK_Aftermath_ResourceResidency residency) {
		switch (residency) {
		case GFSDK_Aftermath_ResourceResidency_FullyResident: return "fully resident";
		case GFSDK_Aftermath_ResourceResidency_Evicted: return "evicted";
		case GFSDK_Aftermath_ResourceResidency_MemoryFreed: return "memory freed";
		case GFSDK_Aftermath_ResourceResidency_MemoryUnbound: return "memory unbound";
		default: return "unknown";
		}
	}

	std::string_view ShaderTypeToString(const GFSDK_Aftermath_ShaderType type) {
		switch (type) {
		case GFSDK_Aftermath_ShaderType_Vertex: return "vertex";
		case GFSDK_Aftermath_ShaderType_Tessellation_Control: return "tessellation control";
		case GFSDK_Aftermath_ShaderType_Tessellation_Evaluation: return "tessellation evaluation";
		case GFSDK_Aftermath_ShaderType_Geometry: return "geometry";
		case GFSDK_Aftermath_ShaderType_Fragment: return "fragment";
		case GFSDK_Aftermath_ShaderType_Compute: return "compute";
		case GFSDK_Aftermath_ShaderType_Mesh: return "mesh";
		case GFSDK_Aftermath_ShaderType_Task: return "task";
		default: return "unknown";
		}
	}

	std::string_view ContextStatusToString(const GFSDK_Aftermath_Context_Status status) {
		switch (status) {
		case GFSDK_Aftermath_Context_Status_NotStarted: return "not started";
		case GFSDK_Aftermath_Context_Status_Executing: return "executing";
		case GFSDK_Aftermath_Context_Status_Finished: return "finished";
		default: return "invalid";
		}
	}
}

namespace Cori {
	namespace Graphics {
		class AftermathState {
		public:
			explicit AftermathState(std::filesystem::path dumpDirectory) : m_DumpDirectory(std::move(dumpDirectory)) {}

			void OnCrashDump(const void* gpuCrashDump, const uint32_t gpuCrashDumpSize) {
				std::lock_guard lk(m_Mutex);

				CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "GPU crash detected, writing a crash dump ({} bytes).", gpuCrashDumpSize);

				WriteCrashDumpToFile(gpuCrashDump, gpuCrashDumpSize);
			}

			void OnShaderDebugInfo(const void* shaderDebugInfo, const uint32_t shaderDebugInfoSize) {
				std::lock_guard lk(m_Mutex);

				GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier{};
				const auto result = GFSDK_Aftermath_GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API, shaderDebugInfo, shaderDebugInfoSize, &identifier);

				if (!GFSDK_Aftermath_SUCCEED(result)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to get the shader debug info identifier, {}.", ResultToString(result));
					return;
				}

				const auto* bytes = static_cast<const uint8_t*>(shaderDebugInfo);
				m_ShaderDebugInfo[identifier].assign(bytes, bytes + shaderDebugInfoSize);

				WriteToFile(m_DumpDirectory / std::format("shader-{}.nvdbg", IdentifierToString(identifier)), shaderDebugInfo, shaderDebugInfoSize);
			}

			void OnDescription(const PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription) const {
				addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "CoriEngine");
				addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "1.0");

				uint32_t key = GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined;

				{
					std::lock_guard lk(m_MarkerMutex);
					addDescription(key++, std::format("Frame index at crash: {}", m_MarkerFrameIndex).c_str());
				}

				for (const auto& description : m_Descriptions) {
					addDescription(key++, description.c_str());
				}
			}

			void AddDescription(std::string description) {
				std::lock_guard lk(m_Mutex);
				m_Descriptions.emplace_back(std::move(description));
			}

			void OnResolveMarker(const void* markerData, const PFN_GFSDK_Aftermath_ResolveMarker resolveMarker) const {
				std::string marker;
				if (FindMarker(reinterpret_cast<uint64_t>(markerData), marker)) {
					resolveMarker(marker.data(), static_cast<uint32_t>(marker.length()));
				}
			}

			bool FindMarker(const uint64_t markerID, std::string& outMarker) const {
				std::lock_guard lk(m_MarkerMutex);

				for (const auto& markers : m_MarkerMap) {
					const auto it = markers.find(markerID);
					if (it != markers.end()) {
						outMarker = it->second;
						return true;
					}
				}

				return false;
			}

			void OnShaderDebugInfoLookup(const GFSDK_Aftermath_ShaderDebugInfoIdentifier& identifier, const PFN_GFSDK_Aftermath_SetData setShaderDebugInfo) const {
				const auto it = m_ShaderDebugInfo.find(identifier);
				if (it == m_ShaderDebugInfo.end()) {
					return;
				}

				setShaderDebugInfo(it->second.data(), static_cast<uint32_t>(it->second.size()));
			}

			void OnShaderLookup(const GFSDK_Aftermath_ShaderBinaryHash& shaderHash, const PFN_GFSDK_Aftermath_SetData setShaderBinary) const {
				const auto it = m_ShaderBinaries.find(shaderHash);
				if (it == m_ShaderBinaries.end()) {
					return;
				}

				setShaderBinary(it->second.data(), static_cast<uint32_t>(it->second.size()));
			}

			void RegisterShaderBinary(const void* spirvCode, const size_t codeSizeBytes, const std::string_view debugName) {
				GFSDK_Aftermath_SpirvCode shader{};
				shader.pData = spirvCode;
				shader.size = static_cast<uint32_t>(codeSizeBytes);

				GFSDK_Aftermath_ShaderBinaryHash shaderHash{};
				const auto result = GFSDK_Aftermath_GetShaderHashSpirv(GFSDK_Aftermath_Version_API, &shader, &shaderHash);

				if (!GFSDK_Aftermath_SUCCEED(result)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to hash the SPIR-V of '{}', {}. Crash dumps will not be able to resolve this shader.", debugName, ResultToString(result));
					return;
				}

				const auto* bytes = static_cast<const uint8_t*>(spirvCode);

				std::lock_guard lk(m_Mutex);
				m_ShaderBinaries[shaderHash].assign(bytes, bytes + codeSizeBytes);
			}

			void BeginFrame(const uint64_t frameIndex) {
				std::lock_guard lk(m_MarkerMutex);
				m_MarkerMap[frameIndex % AftermathCrashTracker::s_MarkerFrameHistory].clear();
				m_MarkerFrameIndex = frameIndex;
			}

			uint64_t StoreMarker(const std::string_view markerData) {
				std::lock_guard lk(m_MarkerMutex);

				auto& markers = m_MarkerMap[m_MarkerFrameIndex % AftermathCrashTracker::s_MarkerFrameHistory];

				const uint64_t markerID = (m_MarkerFrameIndex % AftermathCrashTracker::s_MarkerFrameHistory) * s_MaxMarkersPerFrame + markers.size() + 1;
				markers[markerID] = markerData;
				return markerID;
			}

			const std::filesystem::path& GetDumpDirectory() const { return m_DumpDirectory; }

		private:
			static constexpr uint64_t s_MaxMarkersPerFrame{ 100000 };

			void WriteCrashDumpToFile(const void* gpuCrashDump, const uint32_t gpuCrashDumpSize) {
				GFSDK_Aftermath_GpuCrashDump_Decoder decoder{};
				auto result = GFSDK_Aftermath_GpuCrashDump_CreateDecoder(GFSDK_Aftermath_Version_API, gpuCrashDump, gpuCrashDumpSize, &decoder);

				if (!GFSDK_Aftermath_SUCCEED(result)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to create a crash dump decoder, {}. Writing the raw dump only.", ResultToString(result));
					WriteToFile(m_DumpDirectory / std::format("CoriEngine-{}.nv-gpudmp", ++m_DumpCount), gpuCrashDump, gpuCrashDumpSize);
					return;
				}

				GFSDK_Aftermath_GpuCrashDump_BaseInfo baseInfo{};
				result = GFSDK_Aftermath_GpuCrashDump_GetBaseInfo(decoder, &baseInfo);

				const uint32_t pid = GFSDK_Aftermath_SUCCEED(result) ? baseInfo.pid : 0;

				const std::filesystem::path dumpPath = m_DumpDirectory / std::format("CoriEngine-{}-{}.nv-gpudmp", pid, ++m_DumpCount);
				WriteToFile(dumpPath, gpuCrashDump, gpuCrashDumpSize);

				LogSummary(decoder);
				WriteDecodedJSON(decoder, dumpPath);

				GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder);

				CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "GPU crash dump written to '{}'. Open it with Nsight Graphics for the full report.", dumpPath.string());
			}

			void LogSummary(const GFSDK_Aftermath_GpuCrashDump_Decoder decoder) const {
				const Cori::LogTagList tags = { Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath };

				GFSDK_Aftermath_GpuCrashDump_DeviceInfo deviceInfo{};
				if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetDeviceInfo(decoder, &deviceInfo))) {
					CORI_CORE_FATAL_TAGGED(tags, "Device status: {}. Adapter reset: {}. Engine reset: {}.", DeviceStatusToString(deviceInfo.status), deviceInfo.adapterReset, deviceInfo.engineReset);
				}

				GFSDK_Aftermath_GpuCrashDump_SystemInfo systemInfo{};
				if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetSystemInfo(decoder, &systemInfo))) {
					CORI_CORE_FATAL_TAGGED(tags, "Display driver: {}.{}. OS: {}.", systemInfo.displayDriver.major, systemInfo.displayDriver.minor, systemInfo.osVersion);
				}

				uint32_t gpuCount = 0;
				if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetGpuInfoCount(decoder, &gpuCount)) && gpuCount > 0) {
					std::vector<GFSDK_Aftermath_GpuCrashDump_GpuInfo> gpuInfos(gpuCount);
					if (GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetGpuInfo(decoder, gpuCount, gpuInfos.data()))) {
						for (const auto& gpu : gpuInfos) {
							CORI_CORE_FATAL_TAGGED(tags, "GPU: {} ({}).", gpu.adapterName, gpu.generationName);
						}
					}
				}

				LogPageFault(decoder);
				LogActiveShaders(decoder);
				LogEventMarkers(decoder);
			}

			void LogPageFault(const GFSDK_Aftermath_GpuCrashDump_Decoder decoder) const {
				const Cori::LogTagList tags = { Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath };

				GFSDK_Aftermath_GpuCrashDump_PageFaultInfo pageFault{};
				if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetPageFaultInfo(decoder, &pageFault))) {
					return;
				}

				CORI_CORE_FATAL_TAGGED(tags, "Page fault at GPU VA 0x{:016x}: {}, access {}, engine {}, client {}.", pageFault.faultingGpuVA, FaultTypeToString(pageFault.faultType), AccessTypeToString(pageFault.accessType), EngineToString(pageFault.engine), ClientToString(pageFault.client));

				if (pageFault.resourceInfoCount == 0) {
					CORI_CORE_FATAL_TAGGED(tags, "No resource is associated with the faulting address, it may belong to freed memory.");
					return;
				}

				std::vector<GFSDK_Aftermath_GpuCrashDump_ResourceInfo> resources(pageFault.resourceInfoCount);
				if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetPageFaultResourceInfo(decoder, pageFault.resourceInfoCount, resources.data()))) {
					return;
				}

				for (const auto& resource : resources) {
					CORI_CORE_FATAL_TAGGED(tags, "Faulting resource '{}': VkImage/VkBuffer handle 0x{:016x}, GPU VA 0x{:016x}, {} bytes, {}x{}x{}, {} mips, format {}, residency {}, destroyed: {}.",
						resource.debugName[0] ? resource.debugName : "<unnamed>",
						resource.apiResource,
						resource.gpuVa,
						resource.size,
						resource.width, resource.height, resource.depth,
						resource.mipLevels,
						vk::to_string(static_cast<vk::Format>(resource.format)),
						ResidencyToString(resource.residency),
						resource.bWasDestroyed);
				}
			}

			void LogActiveShaders(const GFSDK_Aftermath_GpuCrashDump_Decoder decoder) const {
				const Cori::LogTagList tags = { Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath };

				uint32_t shaderCount = 0;
				if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfoCount(decoder, &shaderCount)) || shaderCount == 0) {
					return;
				}

				std::vector<GFSDK_Aftermath_GpuCrashDump_ShaderInfo> shaders(shaderCount);
				if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetActiveShadersInfo(decoder, shaderCount, shaders.data()))) {
					return;
				}

				for (const auto& shader : shaders) {
					CORI_CORE_FATAL_TAGGED(tags, "Active shader: {} stage, hash 0x{:016x}, debug info uid 0x{:016x}, internal: {}.", ShaderTypeToString(shader.shaderType), shader.shaderHash, shader.shaderDebugInfoUid, shader.isInternal);
				}
			}

			void LogEventMarkers(const GFSDK_Aftermath_GpuCrashDump_Decoder decoder) const {
				const Cori::LogTagList tags = { Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath };

				uint32_t markerCount = 0;
				if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetEventMarkersInfoCount(decoder, &markerCount)) || markerCount == 0) {
					return;
				}

				std::vector<GFSDK_Aftermath_GpuCrashDump_EventMarkerInfo> markers(markerCount);
				if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GetEventMarkersInfo(decoder, markerCount, markers.data()))) {
					return;
				}

				CORI_CORE_FATAL_TAGGED(tags, "{} checkpoint marker(s) were in flight, 'executing' ones are the work the GPU never finished:", markerCount);

				for (const auto& marker : markers) {
					std::string name;
					if (marker.markerDataOwnership == GFSDK_Aftermath_EventMarkerDataOwnership_Decoder && marker.markerDataSize > 0) {
						name.assign(static_cast<const char*>(marker.markerData), marker.markerDataSize);
					} else if (!FindMarker(reinterpret_cast<uint64_t>(marker.markerData), name)) {
						name = "<automatic checkpoint>";
					}

					CORI_CORE_FATAL_TAGGED(tags, "    [{}] {}", ContextStatusToString(marker.contextStatus), name);
				}
			}

			void LogWarpInfo(const std::vector<char>& json) const {
				const Cori::LogTagList tags = { Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath };

				const nlohmann::json parsed = nlohmann::json::parse(json.begin(), json.end(), nullptr, false, true);
				if (parsed.is_discarded()) {
					CORI_CORE_ERROR_TAGGED(tags, "Could not parse the decoded crash dump JSON, skipping the warp state summary.");
					return;
				}

				auto forEachEntry = [&parsed](const char* key, auto&& callback) {
					for (const auto& element : parsed) {
						if (!element.is_object() || !element.contains(key)) {
							continue;
						}

						const auto& value = element[key];
						for (const auto& item : value.is_array() ? value : nlohmann::json::array({ value })) {
							callback(item);
						}
					}
				};

				forEachEntry("Faulted Warps", [&tags](const nlohmann::json& warp) {
					CORI_CORE_FATAL_TAGGED(tags, "Faulted warp: {} at {}, source {}.",
						warp.value("Fault Name", "unknown fault"),
						warp.value("Shader GPU PC Address", "unknown address"),
						warp.value("Shader mapping", "<no shader debug info, recompile the shaders with slangc -g>"));

					CORI_CORE_FATAL_TAGGED(tags, "    {}", warp.value("Fault Description", ""));
				});

				forEachEntry("Active Warps", [&tags](const nlohmann::json& warp) {
					CORI_CORE_FATAL_TAGGED(tags, "Active warp(s) x{}: {}, source {}.",
						warp.value("Warp count", 0),
						warp.value("GPU PC Address", "unknown address"),
						warp.value("Shader mapping", "<no shader debug info>"));
				});
			}

			void WriteDecodedJSON(const GFSDK_Aftermath_GpuCrashDump_Decoder decoder, const std::filesystem::path& dumpPath) {
				uint32_t jsonSize = 0;
				auto result = GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
					decoder,
					GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
					GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
					ShaderDebugInfoLookupCallback,
					ShaderLookupCallback,
					nullptr,
					this,
					&jsonSize);

				if (!GFSDK_Aftermath_SUCCEED(result)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to decode the crash dump to JSON, {}.", ResultToString(result));
					return;
				}

				std::vector<char> json(jsonSize);
				result = GFSDK_Aftermath_GpuCrashDump_GetJSON(decoder, static_cast<uint32_t>(json.size()), json.data());

				if (!GFSDK_Aftermath_SUCCEED(result)) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to fetch the decoded crash dump JSON, {}.", ResultToString(result));
					return;
				}

				std::filesystem::path jsonPath = dumpPath;
				jsonPath += ".json";

				WriteToFile(jsonPath, json.data(), json.empty() ? 0 : static_cast<uint32_t>(json.size() - 1));

				LogWarpInfo(json);
			}

			void WriteToFile(const std::filesystem::path& path, const void* data, const uint32_t size) const {
				std::error_code ec;
				std::filesystem::create_directories(m_DumpDirectory, ec);

				std::ofstream file(path, std::ios::out | std::ios::binary);
				if (!file) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to open '{}' for writing.", path.string());
					return;
				}

				file.write(static_cast<const char*>(data), size);
			}

			static void ShaderDebugInfoLookupCallback(const GFSDK_Aftermath_ShaderDebugInfoIdentifier* identifier, const PFN_GFSDK_Aftermath_SetData setShaderDebugInfo, void* userData) {
				static_cast<const AftermathState*>(userData)->OnShaderDebugInfoLookup(*identifier, setShaderDebugInfo);
			}

			static void ShaderLookupCallback(const GFSDK_Aftermath_ShaderBinaryHash* shaderHash, const PFN_GFSDK_Aftermath_SetData setShaderBinary, void* userData) {
				static_cast<const AftermathState*>(userData)->OnShaderLookup(*shaderHash, setShaderBinary);
			}

			std::filesystem::path m_DumpDirectory;

			std::mutex m_Mutex;
			std::map<GFSDK_Aftermath_ShaderDebugInfoIdentifier, std::vector<uint8_t>, ShaderDebugInfoIdentifierLess> m_ShaderDebugInfo;
			std::map<GFSDK_Aftermath_ShaderBinaryHash, std::vector<uint8_t>, ShaderBinaryHashLess> m_ShaderBinaries;
			std::vector<std::string> m_Descriptions;

			mutable std::mutex m_MarkerMutex;
			std::array<std::map<uint64_t, std::string>, AftermathCrashTracker::s_MarkerFrameHistory> m_MarkerMap;
			uint64_t m_MarkerFrameIndex{ 0 };

			uint32_t m_DumpCount{ 0 };
		};

		namespace {
			std::unique_ptr<AftermathState> s_State;
			bool s_DeviceInstrumented{ false };

			void GpuCrashDumpCallback(const void* gpuCrashDump, const uint32_t gpuCrashDumpSize, void* userData) {
				static_cast<AftermathState*>(userData)->OnCrashDump(gpuCrashDump, gpuCrashDumpSize);
			}

			void ShaderDebugInfoCallback(const void* shaderDebugInfo, const uint32_t shaderDebugInfoSize, void* userData) {
				static_cast<AftermathState*>(userData)->OnShaderDebugInfo(shaderDebugInfo, shaderDebugInfoSize);
			}

			void CrashDumpDescriptionCallback(const PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* userData) {
				static_cast<const AftermathState*>(userData)->OnDescription(addDescription);
			}

			void ResolveMarkerCallback(const void* markerData, const uint32_t, void* userData, const PFN_GFSDK_Aftermath_ResolveMarker resolveMarker) {
				static_cast<AftermathState*>(userData)->OnResolveMarker(markerData, resolveMarker);
			}

			std::filesystem::path ResolveDumpDirectory() {
				const std::filesystem::path userData = FileSystem::PathManager::GetAliasedPath("USER_DATA");
				const std::filesystem::path root = userData.empty() ? std::filesystem::current_path() : userData;

				std::error_code ec;
				const std::filesystem::path absolute = std::filesystem::absolute(root / "GpuCrashDumps", ec);
				return ec ? root / "GpuCrashDumps" : absolute.lexically_normal();
			}
		}

		void AftermathCrashTracker::Init() {
			CORI_CORE_ASSERT(!s_State, "AftermathCrashTracker::Init was called twice.");

			auto state = std::make_unique<AftermathState>(ResolveDumpDirectory());

			const auto result = GFSDK_Aftermath_EnableGpuCrashDumps(
				GFSDK_Aftermath_Version_API,
				GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
				GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
				GpuCrashDumpCallback,
				ShaderDebugInfoCallback,
				CrashDumpDescriptionCallback,
				ResolveMarkerCallback,
				state.get());

			if (!GFSDK_Aftermath_SUCCEED(result)) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to enable GPU crash dumps, {}. GPU crashes will not be captured.", ResultToString(result));
				return;
			}

			s_State = std::move(state);

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "GPU crash dump collection is enabled, dumps go to '{}'.", s_State->GetDumpDirectory().string());
		}

		void AftermathCrashTracker::Shutdown() {
			if (!s_State) {
				return;
			}

			GFSDK_Aftermath_DisableGpuCrashDumps();
			s_State.reset();
			s_DeviceInstrumented = false;
		}

		bool AftermathCrashTracker::IsActive() {
			return s_State != nullptr;
		}

		bool AftermathCrashTracker::IsDeviceInstrumented() {
			return s_DeviceInstrumented;
		}

		bool AftermathCrashTracker::IsSupportedByDevice(const vk::PhysicalDevice physicalDevice) {
			if (!s_State || !physicalDevice) {
				return false;
			}

			if (physicalDevice.getProperties().vendorID != NVIDIA_VENDOR_ID) {
				return false;
			}

			auto [result, availableExtensions] = physicalDevice.enumerateDeviceExtensionProperties();
			if (result != vk::Result::eSuccess) {
				return false;
			}

			const auto supports = [&availableExtensions](const char* extension) {
				return std::ranges::any_of(availableExtensions, [extension](const auto& available) {
					return strcmp(available.extensionName, extension) == 0;
				});
			};

			if (!supports(vk::NVDeviceDiagnosticCheckpointsExtensionName) || !supports(vk::NVDeviceDiagnosticsConfigExtensionName)) {
				CORI_CORE_WARN_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "The selected NVIDIA device does not expose '{}' and '{}', GPU crash dumps will lack markers and resource tracking.", vk::NVDeviceDiagnosticCheckpointsExtensionName, vk::NVDeviceDiagnosticsConfigExtensionName);
				return false;
			}

			const auto features = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceDiagnosticsConfigFeaturesNV>();
			return features.get<vk::PhysicalDeviceDiagnosticsConfigFeaturesNV>().diagnosticsConfig;
		}

		void AftermathCrashTracker::SetDeviceInstrumented(const bool instrumented) {
			s_DeviceInstrumented = instrumented && s_State != nullptr;
		}

		void AftermathCrashTracker::AddDumpDescription(std::string description) {
			if (!s_State) {
				return;
			}

			s_State->AddDescription(std::move(description));
		}

		void AftermathCrashTracker::RegisterShaderBinary(const void* spirvCode, const size_t codeSizeBytes, const std::string_view debugName) {
			if (!s_State || !spirvCode || codeSizeBytes == 0) {
				return;
			}

			s_State->RegisterShaderBinary(spirvCode, codeSizeBytes, debugName);
		}

		void AftermathCrashTracker::BeginFrame(const uint64_t frameIndex) {
			if (!s_DeviceInstrumented) {
				return;
			}

			s_State->BeginFrame(frameIndex);
		}

		void AftermathCrashTracker::SetCheckpoint(const vk::CommandBuffer cmb, const std::string_view markerData) {
			if (!s_DeviceInstrumented || !cmb) {
				return;
			}

			cmb.setCheckpointNV(reinterpret_cast<const void*>(s_State->StoreMarker(markerData)));
		}

		bool AftermathCrashTracker::ResolveMarker(const uint64_t markerID, std::string& outMarker) {
			return s_State && s_State->FindMarker(markerID, outMarker);
		}

		void AftermathCrashTracker::LogQueueCheckpoints(const vk::Queue queue, const std::string_view queueName) {
			if (!s_DeviceInstrumented || !queue) {
				return;
			}

			const Cori::LogTagList tags = { Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath };

			const std::vector<vk::CheckpointDataNV> checkpoints = queue.getCheckpointDataNV();

			if (checkpoints.empty()) {
				CORI_CORE_FATAL_TAGGED(tags, "No checkpoints were reported for the {} queue.", queueName);
				return;
			}

			CORI_CORE_FATAL_TAGGED(tags, "Last checkpoints reached on the {} queue ({}):", queueName, checkpoints.size());

			for (const auto& checkpoint : checkpoints) {
				std::string marker;
				if (!s_State->FindMarker(reinterpret_cast<uint64_t>(checkpoint.pCheckpointMarker), marker)) {
					marker = std::format("<unknown marker {}>", reinterpret_cast<uint64_t>(checkpoint.pCheckpointMarker));
				}

				CORI_CORE_FATAL_TAGGED(tags, "    [{}] {}", vk::to_string(checkpoint.stage), marker);
			}
		}

		bool AftermathCrashTracker::OnDeviceLost(const std::string_view context) {
			if (!s_State) {
				CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Device lost during {}, but Aftermath is not active so no crash dump was collected.", context);
				return false;
			}

			CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Device lost during {}, waiting for the Aftermath crash dump.", context);

			GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
			auto result = GFSDK_Aftermath_GetCrashDumpStatus(&status);

			const auto start = std::chrono::steady_clock::now();

			while (GFSDK_Aftermath_SUCCEED(result)
				&& status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed
				&& status != GFSDK_Aftermath_CrashDump_Status_Finished
				&& std::chrono::steady_clock::now() - start < CRASH_DUMP_TIMEOUT) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				result = GFSDK_Aftermath_GetCrashDumpStatus(&status);
			}

			if (!GFSDK_Aftermath_SUCCEED(result)) {
				CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Failed to query the crash dump status, {}.", ResultToString(result));
				return false;
			}

			if (status != GFSDK_Aftermath_CrashDump_Status_Finished) {
				CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Aftermath did not finish the crash dump, {}.", StatusToString(status));
				return false;
			}

			return true;
		}
	}
}

#else

namespace Cori {
	namespace Graphics {
		void AftermathCrashTracker::Init() {}

		void AftermathCrashTracker::Shutdown() {}

		bool AftermathCrashTracker::IsActive() { return false; }

		bool AftermathCrashTracker::IsDeviceInstrumented() { return false; }

		bool AftermathCrashTracker::IsSupportedByDevice(vk::PhysicalDevice) { return false; }

		void AftermathCrashTracker::SetDeviceInstrumented(bool) {}

		void AftermathCrashTracker::AddDumpDescription(std::string) {}

		void AftermathCrashTracker::RegisterShaderBinary(const void*, size_t, std::string_view) {}

		void AftermathCrashTracker::BeginFrame(uint64_t) {}

		void AftermathCrashTracker::SetCheckpoint(vk::CommandBuffer, std::string_view) {}

		bool AftermathCrashTracker::ResolveMarker(uint64_t, std::string&) { return false; }

		void AftermathCrashTracker::LogQueueCheckpoints(vk::Queue, std::string_view) {}

		bool AftermathCrashTracker::OnDeviceLost(const std::string_view context) {
			CORI_CORE_FATAL_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Aftermath }, "Device lost during {}. The engine was built without the Nsight Aftermath SDK, so no crash dump was collected.", context);
			return false;
		}
	}
}

#endif
