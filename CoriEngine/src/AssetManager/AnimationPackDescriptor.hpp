#pragma once
#include "Graphics/Animator/AnimationPack.hpp"

namespace Cori {
	class AnimationPackDescriptor {
	public:
		constexpr AnimationPackDescriptor(std::string name, const std::filesystem::path& jsonPath, const Graphics::AnimationPack::ConfigType type) noexcept
			: m_JsonPath(jsonPath),
			m_ConfigType(type),
			m_Name(std::move(name)),
			m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
		{ }

		[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

		constexpr bool operator==(const AnimationPackDescriptor& other) const noexcept {
			return m_RuntimeID == other.m_RuntimeID;
		}

		struct Hasher {
			std::size_t operator()(const AnimationPackDescriptor& descriptor) const noexcept {
				return std::hash<uint32_t>{}(descriptor.m_RuntimeID);
			}
		};

		const std::filesystem::path m_JsonPath;
		const Graphics::AnimationPack::ConfigType m_ConfigType;
		const std::string m_Name;

	private:
		const uint32_t m_RuntimeID{ 0 };
		inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };

	};
}