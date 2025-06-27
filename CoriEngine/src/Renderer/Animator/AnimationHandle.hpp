#pragma once

namespace Cori {
	template<typename T>
	concept IsAnimationHandle = requires {
		{ T::m_Index } -> std::convertible_to<size_t>;
		{ T::m_Looped } -> std::convertible_to<bool>;
		{ T::m_AnimatorName } -> std::convertible_to<const char*>;
	};

	template<size_t Index, bool Looped, const char* AnimatorName>
	class AnimationHandle {
	public:
		static constexpr size_t m_Index = Index;
		static constexpr bool m_Looped = Looped;
		static constexpr const char* m_AnimatorName = AnimatorName;
	};
}