#include "VulkanDeviceFault.hpp"
#include "Graphics/Vulkan/VulkanEngine.hpp"

#ifdef CORI_VK_DL_DEBUG_AMD

namespace Cori {
	namespace Graphics {
		bool VulkanDeviceFault::s_Supported{ false };
		bool VulkanDeviceFault::s_AddressBindingTracked{ false };
		std::atomic_flag VulkanDeviceFault::s_Reporting{};

		std::mutex VulkanDeviceFault::s_TrackingMutex;
		std::map<uint64_t, VulkanDeviceFault::AddressBinding> VulkanDeviceFault::s_AddressBindings;
		std::unordered_map<uint64_t, std::string> VulkanDeviceFault::s_ObjectNames;

		void VulkanDeviceFault::SetSupport(const bool faultSupported) {
			s_Supported = faultSupported;
		}

		void VulkanDeviceFault::SetAddressBindingTracking(const bool enabled) {
			s_AddressBindingTracked = enabled;
		}

		bool VulkanDeviceFault::IsSupported() {
			return s_Supported;
		}

		bool VulkanDeviceFault::IsAddressBindingTracked() {
			return s_AddressBindingTracked;
		}

		void VulkanDeviceFault::RecordAddressBinding(const uint64_t baseAddress, const uint64_t size, const bool unbind, const bool internalObject, const vk::ObjectType objectType, const uint64_t objectHandle) {
			std::lock_guard lock(s_TrackingMutex);

			if (unbind) {
				s_AddressBindings.erase(baseAddress);
				return;
			}

			s_AddressBindings[baseAddress] = AddressBinding{
				.size = size,
				.objectHandle = objectHandle,
				.objectType = objectType,
				.internalObject = internalObject
			};
		}

		void VulkanDeviceFault::RecordObjectName(const uint64_t objectHandle, const char* name) {
			if (!s_AddressBindingTracked || name == nullptr || objectHandle == 0) {
				return;
			}

			std::lock_guard lock(s_TrackingMutex);
			s_ObjectNames[objectHandle] = name;
		}

		uint64_t VulkanDeviceFault::LiveAddressBindingCount() {
			std::lock_guard lock(s_TrackingMutex);
			return s_AddressBindings.size();
		}

		bool VulkanDeviceFault::ObjectNamesResolvable() {
			std::lock_guard lock(s_TrackingMutex);

			if (s_ObjectNames.empty() || s_AddressBindings.empty()) {
				return true;
			}

			for (const auto& [base, binding] : s_AddressBindings) {
				if (s_ObjectNames.contains(binding.objectHandle)) {
					return true;
				}
			}

			return false;
		}

		void VulkanDeviceFault::DumpAddressBindings(const std::string_view context) {
			if (!s_AddressBindingTracked) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "No address bindings to dump for '{}', VK_EXT_device_address_binding_report is not enabled.", context);
				return;
			}

			std::lock_guard lock(s_TrackingMutex);

			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "Live address bindings for '{}' ({} total, sorted by address, {} name(s) recorded):", context, s_AddressBindings.size(), s_ObjectNames.size());

			for (const auto& [base, binding] : s_AddressBindings) {
				std::string name = "<unnamed>";
				if (const auto nameIt = s_ObjectNames.find(binding.objectHandle); nameIt != s_ObjectNames.end()) {
					name = nameIt->second;
				}

				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\t[{:#x}, {:#x}) {} byte(s), {}{} '{}' (handle {:#x})", base, base + binding.size, binding.size, binding.internalObject ? "driver internal " : "", vk::to_string(binding.objectType), name, binding.objectHandle);
			}

			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "Recorded object names ({} total):", s_ObjectNames.size());

			for (const auto& [handle, name] : s_ObjectNames) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\thandle {:#x} -> '{}'", handle, name);
			}
		}

		std::string VulkanDeviceFault::DescribeAddress(const uint64_t address) {
			if (!s_AddressBindingTracked) {
				return "no owner info, VK_EXT_device_address_binding_report is not enabled";
			}

			std::lock_guard lock(s_TrackingMutex);

			auto it = s_AddressBindings.upper_bound(address);
			if (it == s_AddressBindings.begin()) {
				return "no live allocation covers this address, it is below every tracked binding";
			}

			--it;

			const uint64_t base = it->first;
			const AddressBinding& binding = it->second;
			const uint64_t end = base + binding.size;

			if (address >= end) {
				return fmt::format("no live allocation covers this address, it is {} byte(s) past the end of the nearest one below it, [{:#x}, {:#x})", address - end, base, end);
			}

			if (binding.internalObject) {
				return fmt::format("inside a driver internal allocation at [{:#x}, {:#x}), offset {:#x}", base, end, address - base);
			}

			std::string name = "<unnamed>";
			if (const auto nameIt = s_ObjectNames.find(binding.objectHandle); nameIt != s_ObjectNames.end()) {
				name = nameIt->second;
			}

			return fmt::format("inside {} '{}' (handle {:#x}) at [{:#x}, {:#x}), offset {:#x} into it", vk::to_string(binding.objectType), name, binding.objectHandle, base, end, address - base);
		}

		bool VulkanDeviceFault::Report(const std::string_view context) {
			if (s_Reporting.test_and_set(std::memory_order_acquire)) {
				return false;
			}

			struct ReportGuard {
				~ReportGuard() {
					s_Reporting.clear(std::memory_order_release);
				}
			} reportGuard;

			if (!s_Supported) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "No device fault info available for '{}': VK_EXT_device_fault is not enabled on this device.", context);
				return false;
			}

			const vk::Device device = VulkanEngine::GetLogicalDevice();
			if (!device) {
				return false;
			}

			vk::DeviceFaultCountsEXT counts{};

			vk::Result result = device.getFaultInfoEXT(&counts, nullptr);
			if (result != vk::Result::eSuccess && result != vk::Result::eIncomplete) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "vkGetDeviceFaultInfoEXT failed while querying the fault counts for '{}'. Error: {}", context, vk::to_string(result));
				return false;
			}

			counts.vendorBinarySize = 0;

			std::vector<vk::DeviceFaultAddressInfoEXT> addressInfos(counts.addressInfoCount);
			std::vector<vk::DeviceFaultVendorInfoEXT> vendorInfos(counts.vendorInfoCount);

			vk::DeviceFaultInfoEXT info{
				.pAddressInfos = addressInfos.empty() ? nullptr : addressInfos.data(),
				.pVendorInfos = vendorInfos.empty() ? nullptr : vendorInfos.data()
			};

			result = device.getFaultInfoEXT(&counts, &info);
			if (result != vk::Result::eSuccess && result != vk::Result::eIncomplete) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "vkGetDeviceFaultInfoEXT failed while fetching the fault info for '{}'. Error: {}", context, vk::to_string(result));
				return false;
			}

			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "Device fault report for '{}': \"{}\". {} address info(s), {} vendor info(s). {} live address binding(s) tracked.", context, info.description.data(), counts.addressInfoCount, counts.vendorInfoCount, LiveAddressBindingCount());

			if (s_AddressBindingTracked && !ObjectNamesResolvable()) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "Not one tracked binding matches a recorded object name, so everything below will resolve as '<unnamed>'. VK_LAYER_KHRONOS_validation wraps non-dispatchable handles by default, so the handles the engine names are not the handles the driver reports. Turn off 'Handle Wrapping' (unique_handles) in vkconfig, or run without the validation layer, to get named objects here.");
			}

			for (uint32_t i = 0; i < counts.addressInfoCount; i++) {
				const vk::DeviceFaultAddressInfoEXT& addressInfo = addressInfos[i];

				const uint64_t precision = static_cast<uint64_t>(addressInfo.addressPrecision);
				const uint64_t lowerBound = precision == 0 ? addressInfo.reportedAddress : addressInfo.reportedAddress & ~(precision - 1);
				const uint64_t upperBound = lowerBound + precision;

				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\tAddress fault [{}]: type {}, reported address {:#x}, precision {:#x}, faulting address lies in [{:#x}, {:#x}).", i, vk::to_string(addressInfo.addressType), addressInfo.reportedAddress, precision, lowerBound, upperBound);
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\t\tReported address: {}", DescribeAddress(addressInfo.reportedAddress));

				if (lowerBound != addressInfo.reportedAddress) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\t\tRange start {:#x}: {}", lowerBound, DescribeAddress(lowerBound));
				}
			}

			for (uint32_t i = 0; i < counts.vendorInfoCount; i++) {
				const vk::DeviceFaultVendorInfoEXT& vendorInfo = vendorInfos[i];

				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::DeviceFault }, "\tVendor fault [{}]: \"{}\", fault code {:#x}, fault data {:#x}.", i, vendorInfo.description.data(), vendorInfo.vendorFaultCode, vendorInfo.vendorFaultData);
			}

			if (counts.addressInfoCount > 0) {
				DumpAddressBindings(context);
			}

			return true;
		}

	}
}

#endif
