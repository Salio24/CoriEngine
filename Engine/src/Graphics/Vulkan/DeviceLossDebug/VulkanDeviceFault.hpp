#pragma once

#ifdef CORI_VK_DL_DEBUG_AMD

namespace Cori {
	namespace Graphics {
		class VulkanDeviceFault {
		public:
			static void SetSupport(bool faultSupported);

			static void SetAddressBindingTracking(bool enabled);

			[[nodiscard]] static bool IsSupported();

			[[nodiscard]] static bool IsAddressBindingTracked();

			static void RecordAddressBinding(uint64_t baseAddress, uint64_t size, bool unbind, bool internalObject, vk::ObjectType objectType, uint64_t objectHandle);

			static void RecordObjectName(uint64_t objectHandle, const char* name);

			[[nodiscard]] static std::string DescribeAddress(uint64_t address);

			[[nodiscard]] static uint64_t LiveAddressBindingCount();

			static void DumpAddressBindings(std::string_view context);

			[[nodiscard]] static bool ObjectNamesResolvable();

			static bool Report(std::string_view context);

		private:
			struct AddressBinding {
				uint64_t size;
				uint64_t objectHandle;
				vk::ObjectType objectType;
				bool internalObject;
			};

			static bool s_Supported;
			static bool s_AddressBindingTracked;
			static std::atomic_flag s_Reporting;

			static std::mutex s_TrackingMutex;
			static std::map<uint64_t, AddressBinding> s_AddressBindings;
			static std::unordered_map<uint64_t, std::string> s_ObjectNames;
		};
	}
}

#endif
