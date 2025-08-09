#pragma once
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"
#include "Mixer.hpp"
#include "Sound.hpp"

struct MIX_Track;

namespace Cori {
	namespace Audio {
		// use expected
		class Track : public Profiling::Trackable<Track>, public SharedSelfFactory<Track> {
		public:
			static bool PreCreateHook([[maybe_unused]] std::string name = "Unnamed") {
				return true;
			}

			bool SetSound(const std::shared_ptr<Sound>& sound) {
				return Mixer::SetTrackSound(m_Index, sound->m_Index);
			}

			bool Play(const PlayParams& params = PlayParams{}) {
				return Mixer::PlayTrack(m_Index, params);
			}

			bool Pause() {
				return Mixer::PauseTrack(m_Index);
			}

			bool Resume() {
				return Mixer::ResumeTrack(m_Index);
			}

			bool Stop(uint64_t fadeOutMS) {
				return Mixer::StopTrack(m_Index, fadeOutMS);
			}

			bool IsPaused() {
				return Mixer::IsTrackPaused(m_Index);
			}

			bool IsPlaying() {
				return Mixer::IsTrackPlaying(m_Index);
			}

			bool SetGain(float gain) {
				return Mixer::SetTrackGain(m_Index, gain);
			}

			float GetGain() {
				return Mixer::GetTrackGain(m_Index);
			}

			bool SetTag(const char *tag) {
				return Mixer::TagTrack(m_Index, tag);

			}

			void RemoveTag(const char *tag) {
				Mixer::UntagTrack(m_Index, tag);
			}

			static void TrackStopCallback(void* userdata, MIX_Track* track);

		protected:
			Track(std::string name = "Unnamed") : m_Name(std::move(name)), m_Index(s_NextIndex.fetch_add(1, std::memory_order_relaxed)) {
				Mixer::CreateTrack(m_Index, this);
			}

			~Track() {
				Mixer::DestroyTrack(m_Index);
			}

			const std::string m_Name;

		private:
			const uint32_t m_Index{ 0 };
			inline static std::atomic<uint32_t> s_NextIndex{ 1 };
		};
	}
}