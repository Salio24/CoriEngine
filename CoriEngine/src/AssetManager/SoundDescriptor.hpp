#pragma once

namespace Cori {
	class SoundDescriptor {
	public:
		constexpr SoundDescriptor(std::string name, std::filesystem::path path, const bool preDecode = true) noexcept
			: m_Path(std::move(path)),
			m_PreDecode(preDecode),
			m_Name(std::move(name)),
			m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

		constexpr bool operator==(const SoundDescriptor& other) const noexcept {
			return m_RuntimeID == other.m_RuntimeID;
		}

		struct Hasher {
			std::size_t operator()(const SoundDescriptor& handle) const noexcept {
				return std::hash<uint32_t>{}(handle.m_RuntimeID);
			}
		};

		const std::filesystem::path m_Path;
		const bool m_PreDecode;
		const std::string m_Name;

	private:
		const uint32_t m_RuntimeID{ 0 };
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
	};
}