#pragma once

namespace Cori {

	class AnimationDescriptor {
	public:
		constexpr AnimationDescriptor(const uint16_t index, const bool looped, const char* animatorName) : m_Index(index), m_Looped(looped), m_AnimatorName(animatorName) {}

		uint16_t m_Index;
		bool m_Looped;
		const char* m_AnimatorName;
	};

	template<typename T>
	concept IsAnimationDescriptor = std::is_same_v<T, AnimationDescriptor>;
}