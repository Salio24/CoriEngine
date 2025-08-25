#pragma once

namespace Cori {
	class ShaderProgramDescriptor {
	public:

		constexpr ShaderProgramDescriptor(std::string name, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const std::filesystem::path& geometryPath = {}) noexcept
			: m_VertexPath(vertexPath),
			m_FragmentPath(fragmentPath),
			m_GeometryPath(geometryPath),
			m_Name(std::move(name)),
			m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		uint32_t GetRuntimeID() const { return m_RuntimeID; }

		constexpr bool operator==(const ShaderProgramDescriptor& other) const noexcept {
			return m_RuntimeID == other.m_RuntimeID;
		}

		struct Hasher {
			std::size_t operator()(const ShaderProgramDescriptor& handle) const noexcept {
				return std::hash<uint32_t>{}(handle.m_RuntimeID);
			}
		};

		const std::filesystem::path m_VertexPath;
		const std::filesystem::path m_FragmentPath;
		const std::filesystem::path m_GeometryPath;
		const std::string m_Name;

	private:
		const uint32_t m_RuntimeID{ 0 };
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
	};
}