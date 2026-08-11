#include "VmaLeakLog.hpp"
#include "VulkanEngine.hpp"
#include "Core/Console/Console.hpp"

#include <cstdarg>
#include <cstdio>

namespace Cori {
	namespace Graphics {
		std::atomic<uint32_t> VmaLeakLog::s_LeakedAllocations{ 0 };
		std::atomic<uint32_t> VmaLeakLog::s_LeakAsserts{ 0 };
		std::atomic<uint32_t> VmaLeakLog::s_DumpCounter{ 0 };

		void VmaLeakLog::BumpLeakedAllocationCounter() {
			s_LeakedAllocations.fetch_add(1, std::memory_order_relaxed);
		}

		void VmaLeakLog::BumpLeakAssertCounter() {
			s_LeakAsserts.fetch_add(1, std::memory_order_relaxed);
		}

		std::expected<std::filesystem::path, std::string> VmaLeakLog::WriteLiveAllocationReport(const std::filesystem::path& destination) {
			const vma::Allocator allocator = VulkanEngine::GetAllocator();
			if (!allocator) {
				return std::unexpected("There is no VMA allocator, the Vulkan engine is not up.");
			}

			char* statsString = allocator.buildStatsString(vk::True);
			if (!statsString) {
				return std::unexpected("VMA did not hand back a statistics string.");
			}

			std::error_code errorCode;

			std::filesystem::path reportPath = destination;
			if (reportPath.empty()) {
				std::filesystem::create_directories("logs", errorCode);
				reportPath = std::filesystem::path("logs") / fmt::format("vma_leak_report_{}.json", s_DumpCounter.fetch_add(1, std::memory_order_relaxed));
			} else if (reportPath.has_parent_path()) {
				std::filesystem::create_directories(reportPath.parent_path(), errorCode);
			}

			std::ofstream file(reportPath);
			if (!file) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Vma }, "Could not open '{}' for the VMA dump, writing it to the log instead:\n{}", reportPath.string(), statsString);
				allocator.freeStatsString(statsString);
				return std::unexpected(std::format("Could not open '{}' for the VMA dump, it went to the log instead.", reportPath.string()));
			}

			file << statsString;
			allocator.freeStatsString(statsString);

			return std::filesystem::absolute(reportPath, errorCode);
		}

		bool VmaLeakLog::DumpLiveAllocations(const std::string_view label) {
			const vma::Allocator allocator = VulkanEngine::GetAllocator();
			if (!allocator) {
				return false;
			}

			const vma::TotalStatistics statistics = allocator.calculateStatistics();
			const vma::Statistics& total = statistics.total.statistics;

			if (total.allocationCount == 0) {
				CORI_CORE_INFO_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Vma }, "No live VMA allocations at '{}'.", label);
				return false;
			}

			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Vma }, "{} live VMA allocation(s) holding {} bytes across {} device memory block(s) holding {} bytes at '{}'.", total.allocationCount, static_cast<uint64_t>(total.allocationBytes), total.blockCount, static_cast<uint64_t>(total.blockBytes), label);

			if (const std::expected<std::filesystem::path, std::string> report = WriteLiveAllocationReport(); report) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Vma }, "Detailed VMA dump written to '{}'. Every allocation is listed with the name given to VulkanBuffer/VulkanImage CreateInfo::name.", report->string());
			}

			return true;
		}

		bool VmaLeakLog::LogCollectedLeaks(const std::string_view label) {
			const uint64_t leaked = s_LeakedAllocations.load(std::memory_order_relaxed);
			const uint64_t asserts = s_LeakAsserts.load(std::memory_order_relaxed);

			if (leaked == 0 && asserts == 0) {
				return false;
			}

			s_LeakedAllocations.store(0, std::memory_order_relaxed);
			s_LeakAsserts.store(0, std::memory_order_relaxed);

			CORI_CORE_ERROR_TAGGED({ Logger::Tags::Graphics::Self, Logger::Tags::Graphics::Vulkan::Self, Logger::Tags::Graphics::Vulkan::Vma }, "VMA reported {} unfreed allocation(s) and {} leak assertion(s) during '{}'. The 'UNFREED ALLOCATION' lines above carry the offset, size and name of each one.", leaked, asserts, label);

			return true;
		}
	}
}

namespace {
	using namespace Cori;
	using namespace Cori::Core;

	CORI_CONSOLE_COMMAND(VmaDump, "vma.dump", "vma.dump [file] - writes every live VMA allocation to a JSON report, by default a numbered one under logs/") {
		const vma::Allocator allocator = Graphics::VulkanEngine::GetAllocator();
		if (!allocator) {
			return std::unexpected("There is no VMA allocator, the Vulkan engine is not up.");
		}

		const vma::TotalStatistics statistics = allocator.calculateStatistics();
		const vma::Statistics& total = statistics.total.statistics;

		const std::expected<std::filesystem::path, std::string> report = Graphics::VmaLeakLog::WriteLiveAllocationReport(args.empty() ? std::filesystem::path{} : std::filesystem::path(args[0]));
		if (!report) {
			return std::unexpected(report.error());
		}

		return std::format("{} live VMA allocation(s) holding {} bytes across {} device memory block(s) holding {} bytes written to '{}'.", total.allocationCount, static_cast<uint64_t>(total.allocationBytes), total.blockCount, static_cast<uint64_t>(total.blockBytes), report->string());
	}
}
