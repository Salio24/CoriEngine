#include "Track.hpp"
#include <SDL3_mixer/SDL_mixer.h>

namespace Cori {
	namespace Audio {
		std::expected<void, Core::CoriError<>> Track::SetSound(const std::shared_ptr<Sound>& sound) {
			if (m_Valid) {
				if (!m_ActiveSequence) {
					if (sound->IsValid()) {
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Assigning Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'", sound->m_Name, sound->GetID(), m_Name, m_ID);
						if (sound->IsPlaceholder()) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "A Sound '{} (SoundID: {})' to be assigned to Track '{} (TrackID: {})' is a placeholder, likely failed to load the original sound.", sound->m_Name, sound->GetID(), m_Name, m_ID);
						}

						return Mixer::SetTrackSound(m_ID, sound.get());
					}

					return std::unexpected(Core::CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. Sound object is invalid.", sound->m_Name, sound->GetID(), m_Name, m_ID)));
				}

				return std::unexpected(Core::CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. A sequence is currently playing on this track. Can't assign Sound when a sequence is playing.", sound->m_Name, sound->GetID(), m_Name, m_ID)));

			}


			return std::unexpected(Core::CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. Track object is invalid.", sound->m_Name, sound->GetID(), m_Name, m_ID)));
		}

		std::expected<void, Core::CoriError<>> Track::Start(const PlayParams& params) {
			if (m_Valid) {
				if (!m_ActiveSequence) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Playing Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::PlayTrack(m_ID, params);
				}

				return std::unexpected(Core::CoriError(std::format("Failed to play Track '{} (TrackID: {})'. A sequence is currently playing on this track. Can't play Sound when a sequence is playing.", m_Name, m_ID)));
			}

			return std::unexpected(Core::CoriError(std::format("Failed to play Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
		}

		std::expected<void, Core::CoriError<>> Track::Stop(const bool abruptStop, const int64_t fadeOutMS) {
			if (m_Valid) {
				if (m_ActiveSequence) {
					SetTrackStopCallbackInternal([this] {
						m_ActiveSequence = false;
					});
					if (abruptStop) {
						m_ActiveSequence = false;
						return StopInternal(fadeOutMS);
					}
				}
				return {};
			}

			return std::unexpected(Core::CoriError(std::format("Failed to stop sequence on Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
		}

		std::expected<void, Core::CoriError<>> Track::Pause() {
			if (m_Valid) {
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Pausing Track '{} (TrackID: {})'", m_Name, m_ID);
				return Mixer::PauseTrack(m_ID);
			}

			return std::unexpected(Core::CoriError(std::format("Failed to pause Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
		}

		std::expected<void, Core::CoriError<>> Track::Resume() {
			if (m_Valid) {
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Resuming Track '{} (TrackID: {})'", m_Name, m_ID);
				return Mixer::ResumeTrack(m_ID);
			}

			return std::unexpected(Core::CoriError(std::format("Failed to resume Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
		}

		bool Track::IsPaused() const {
			if (m_Valid) {
				return Mixer::IsTrackPaused(m_ID);
			}

			return false;
		}

		bool Track::IsPlaying() const {
			if (m_Valid) {
				return Mixer::IsTrackPlaying(m_ID);
			}

			return false;
		}

		std::expected<void, Core::CoriError<>> Track::SetGain(const float gain) {
			if (m_Valid) {
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Setting Track '{} (TrackID: {})' gain to '{}'", m_Name, m_ID, gain);
				return Mixer::SetTrackGain(m_ID, gain);
			}

			return std::unexpected(Core::CoriError(std::format("Failed to set Track '{} (TrackID: {})' gain. Track object is invalid.", m_Name, m_ID)));
		}

		float Track::GetGain() const {
			if (m_Valid) {
				return Mixer::GetTrackGain(m_ID);
			}

			return 1.0f;
		}

		std::expected<void, Core::CoriError<>> Track::SetTag(const char* tag) {
			if (m_Valid) {
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Assigning Tag '{}' to Track '{} (TrackID: {})'", tag, m_Name, m_ID);
				m_CurrentTag = std::string(tag);
				return Mixer::TagTrack(m_ID, tag);
			}

			return std::unexpected(Core::CoriError(std::format("Failed to assign Tag '{}' to Track '{} (TrackID: {})'. Track object is invalid.", tag, m_Name, m_ID)));
		}

		void Track::RemoveTag(const char* tag, const bool preserveCachedTag) {
			if (m_Valid) {
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Removing Tag '{}' from Track '{} (TrackID: {})'", tag, m_Name, m_ID);
				Mixer::UntagTrack(m_ID, tag);
				if (!preserveCachedTag) {
					m_CurrentTag.clear();
					m_CurrentTag.shrink_to_fit();
				}
			}
		}

		std::string_view Track::GetTag() const {
			if (m_Valid) {
				return m_CurrentTag;
			}

			return "";
		}

		bool Track::IsValid() const {
			return m_Valid;
		}

		TrackID Track::GetID() const {
			return m_ID;
		}

		void Track::SetTrackStopCallback(TrackStopCallbackFn callback) {
			m_ClientCallBack = std::move(callback);
		}

		std::shared_ptr<Track> Track::Create(std::string name) {
			return std::shared_ptr<Track>(new Track(std::move(name)));
		}

		Track::~Track() {
			Mixer::DestroyTrack(m_ID);
		}

		void Track::TrackStopCallback(void* userdata, [[maybe_unused]] MIX_Track* track) {
			try {
				Track* coriTrack = static_cast<Track*>(userdata);

				if (coriTrack->m_EngineCallBack) {
					coriTrack->m_EngineCallBack();
				}

				if (coriTrack->m_ClientCallBack) {
					coriTrack->m_ClientCallBack();
				} else {
					CORI_CORE_DEBUG("Track '{}' (ID: {}) stopped.", coriTrack->m_Name, coriTrack->m_ID);
				}
			}
			catch (const std::exception& e) {
				CORI_CORE_ERROR("{}", e.what());
			}

		}

		Track::Track(std::string name): m_Name(std::move(name)), m_ID(s_NextIndex.fetch_add(1, std::memory_order_relaxed)) {
			auto result = Mixer::CreateTrack(this);
			if (!result) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Failed to create Track '{} (ID: {})'. Error: {}. Invalid Track object was created as a result, this should not crash as the engine prevents you from using an invalid Track object.", m_Name, m_ID, result.error().what());
			} else {
				m_Valid = true;
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Track '{} (ID: {})' created.", m_Name, m_ID);
			}
		}

		std::expected<void, Core::CoriError<>> Track::PlaySoundWithParams(SoundWithParams& object) {
			return SetSound(object.first).and_then([this, object] {
				return Start(object.second);
			});
		}

		std::expected<void, Core::CoriError<>> Track::StopInternal(const int64_t fadeOutMS) const {
			if (m_Valid) {
				if (!m_ActiveSequence) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Stopping Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::StopTrack(m_ID, fadeOutMS);
				}

				return std::unexpected(Core::CoriError(std::format("Failed to stop Track '{} (TrackID: {})'. A sequence is currently playing on this track. Can't stop when a sequence is playing.", m_Name, m_ID)));
			}

			return std::unexpected(Core::CoriError(std::format("Failed to stop Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
		}

		std::expected<void, Core::CoriError<>> Track::ProcessSequencePart(const SoundWithParams& part) {
			m_SoundSequence.push_back(part);

			return {};
		}

		void Track::SetTrackStopCallbackInternal(TrackStopCallbackFn callback) {
			m_EngineCallBack = std::move(callback);
		}
	}
}
