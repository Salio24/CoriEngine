#pragma once

namespace Cori {
	class Texture2DDescriptor {
	public:
		constexpr Texture2DDescriptor(std::string name, const std::filesystem::path& imagePath) noexcept
			: m_ImagePath(imagePath),
			m_Name(std::move(name)),
			m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		uint32_t GetRuntimeID() const { return m_RuntimeID; }

		constexpr bool operator==(const Texture2DDescriptor& other) const noexcept {
			return m_RuntimeID == other.m_RuntimeID;
		}

		struct Hasher {
			std::size_t operator()(const Texture2DDescriptor& descriptor) const noexcept {
				return std::hash<uint32_t>{}(descriptor.m_RuntimeID);
			}
		};

		const std::filesystem::path m_ImagePath;
		const std::string m_Name;

	private:
		const uint32_t m_RuntimeID{ 0 };
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
	};
}