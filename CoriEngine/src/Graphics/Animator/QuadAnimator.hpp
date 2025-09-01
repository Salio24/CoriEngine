#pragma once
#include <nlohmann/json.hpp>
#include "Animation.hpp"
#include "AnimationDescriptor.hpp"
#include "WorldSystem/Entity.hpp"
#include "WorldSystem/Components.hpp"
using json = nlohmann::json;

/*
 * add a separate animationpack class that will load the animation atlas and hold the relevant data
 *
 * remove start single and move update calls to engine side
 * add stop
 *
 */


namespace Cori {
	namespace Components {
		namespace Entity {
			class QuadAnimator {
			public:
				QuadAnimator(std::filesystem::path jsonPath, const Cori::Entity& entity, const float timeStep, const char* animatorName);
				~QuadAnimator();

				void StartSingle(const AnimationDescriptor& descriptor);

				bool UpdateSingle(const AnimationDescriptor& descriptor);

				void StartSequence(const IsAnimationDescriptor auto&... args) {
					if (!m_Valid) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "Trying to use an invalid Quad Animator, name: {}", m_AnimatorName);
					}

					if (!(CheckDescriptorValidity(args) && ...)) {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::World::Self, Logger::Tags::World::Entity::Self, Logger::Tags::World::Entity::QuadAnimator }, "StartSequence failed, Quad Animator for Entity: '{}'", m_Entity.GetDebugData());
						return;
					}

					m_AnimationQueue.clear();
					m_CurrentAnimationQueueIndex = 0;

					(ResetAnimationState(args), ...);

					bool loopedFlags[] = { args.m_Looped... };
					m_LoopStartIndex = 0xFFFF;
					for (uint16_t i = 0; i < sizeof...(args); ++i) {
						if (loopedFlags[i]) {
							m_LoopStartIndex = i;
							break;
						}
					}

					(m_AnimationQueue.push_back({ GetAnimationPlayer(), args.m_Index }), ...);
				}

				void UpdateSequence();

				uint16_t m_CurrentAnimationQueueIndex = 0;
				uint16_t m_LoopStartIndex = 0xFFFF;
				glm::vec2 m_FrameSize{ 0.0f, 0.0f };

			private:
				std::function<void()> GetAnimationPlayer();

				void ResetAnimationState(const AnimationDescriptor& descriptor);

				[[nodiscard]] bool CheckDescriptorValidity(const AnimationDescriptor& descriptor) const;

				static int32_t ExtractFrameNumber(const std::string& keyStr);

				struct QueuedAnimation {
					std::function<void()> Player;
					uint16_t Index;
				};

				std::vector<QueuedAnimation> m_AnimationQueue;

				const char* m_AnimatorName;
				std::shared_ptr<Texture2D> m_Atlas;
				std::vector<AnimationData> m_Animations;
				Cori::Entity m_Entity{};
				bool m_Valid = false;
			};
		}
	}

}