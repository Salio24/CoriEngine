#pragma once

namespace Cori {
	class ShaderProgramDescriptor {
	public:

		constexpr ShaderProgramDescriptor(std::string_view name, std::string_view vertPath, std::string_view fragPath, std::string_view geomPath = "") noexcept
			: m_VertexPath(vertPath),
			m_FragmentPath(fragPath),
			m_GeometryPath(geomPath),
			m_DebugName(name),
			m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		inline uint32_t GetRuntimeID() const { return m_RuntimeID; }
		inline std::string_view GetDebugName() const { return m_DebugName; }

		constexpr auto operator<=>(const ShaderProgramDescriptor& other) const noexcept {
			return m_RuntimeID <=> other.m_RuntimeID;
		}

		constexpr bool operator==(const ShaderProgramDescriptor& other) const noexcept {
			return m_RuntimeID == other.m_RuntimeID;
		}

		struct Hasher {
			std::size_t operator()(const ShaderProgramDescriptor& handle) const noexcept {
				return std::hash<uint32_t>{}(handle.m_RuntimeID);
			}
		};

		const std::string_view m_VertexPath;
		const std::string_view m_FragmentPath;
		const std::string_view m_GeometryPath;

	private:

		const std::string_view m_DebugName;
		const uint32_t m_RuntimeID;
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
	};
}