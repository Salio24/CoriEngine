#pragma once

namespace Cori {
	class Texture2DDescriptor {
	public:
		constexpr Texture2DDescriptor(std::string_view name, std::string_view imgPath) noexcept
			: imagePath(imgPath),
			debugName(name),
			runtimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		inline uint32_t GetRuntimeID() const { return runtimeID; }
		inline std::string_view GetDebugName() const { return debugName; }

		constexpr auto operator<=>(const Texture2DDescriptor& other) const noexcept {
			return runtimeID <=> other.runtimeID;
		}

		constexpr bool operator==(const Texture2DDescriptor& other) const noexcept {
			return runtimeID == other.runtimeID;
		}

		struct Hasher {
			std::size_t operator()(const Texture2DDescriptor& descriptor) const noexcept {
				return std::hash<uint32_t>{}(descriptor.runtimeID);
			}
		};

		const std::string_view imagePath;

	private:

		const std::string_view debugName;
		const uint32_t runtimeID{ 0 };
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
	};

}