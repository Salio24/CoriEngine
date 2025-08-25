#pragma once
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"
#include "Mixer.hpp"
#include "Sound.hpp"

namespace Cori {
	namespace Audio {
		
		class Track : public Profiling::Trackable<Track>, public SharedSelfFactory<Track> {
		public:
			static bool PreCreateHook([[maybe_unused]] std::string name) {
				return true;
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			[[nodiscard]] std::expected<void, CoriError<>> SetSound(const std::shared_ptr<Sound>& sound) {
				if (m_Valid) {
					if (sound->IsValid()) {
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Assigning Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'", sound->m_Name, sound->m_ID, m_Name, m_ID);
						if (sound->IsValid()) {
							CORI_CORE_WARN_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "A Sound '{} (SoundID: {})' to be assigned to Track '{} (TrackID: {})' is a placeholder, likely failed to load the original sound.", sound->m_Name, sound->m_ID, m_Name, m_ID);
						}
						return Mixer::SetTrackSound(this, sound.get());

					}
					return std::unexpected(CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. Sound object is invalid.", sound->m_Name, sound->m_ID, m_Name, m_ID)));
				}
				return std::unexpected(CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. Track object is invalid.", sound->m_Name, sound->m_ID, m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			[[nodiscard]] std::expected<void, CoriError<>> Play(const PlayParams& params = PlayParams{}) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Playing Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::PlayTrack(this, params);
				}
				return std::unexpected(CoriError(std::format("Failed to play Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			[[nodiscard]] std::expected<void, CoriError<>> Stop(const int64_t fadeOutMS) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Stopping Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::StopTrack(this, fadeOutMS);
				}
				return std::unexpected(CoriError(std::format("Failed to stop Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			[[nodiscard]] std::expected<void, CoriError<>> Pause() {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Pausing Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::PauseTrack(this);
				}
				return std::unexpected(CoriError(std::format("Failed to pause Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			[[nodiscard]] std::expected<void, CoriError<>> Resume() {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Resuming Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::ResumeTrack(this);
				}
				return std::unexpected(CoriError(std::format("Failed to resume Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}


			[[nodiscard]] bool IsPaused() const {
				if (m_Valid) {
					return Mixer::IsTrackPaused(m_ID);
				}
				return false;
			}

			[[nodiscard]] bool IsPlaying() const {
				if (m_Valid) {
					return Mixer::IsTrackPlaying(m_ID);
				}
				return false;
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			[[nodiscard]] std::expected<void, CoriError<>> SetGain(const float gain) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Setting Track '{} (TrackID: {})' gain to '{}'", m_Name, m_ID, gain);
					return Mixer::SetTrackGain(this, gain);
				}
				return std::unexpected(CoriError(std::format("Failed to set Track '{} (TrackID: {})' gain. Track object is invalid.", m_Name, m_ID)));
			}

			[[nodiscard]] float GetGain() const {
				if (m_Valid) {
					return Mixer::GetTrackGain(m_ID);
				}
				return 1.0f;
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			[[nodiscard]] std::expected<void, CoriError<>> SetTag(const char* tag) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Assigning Tag '{}' to Track '{} (TrackID: {})'", tag, m_Name, m_ID);
					return Mixer::TagTrack(this, tag);
				}
				return std::unexpected(CoriError(std::format("Failed to assign Tag '{}' to Track '{} (TrackID: {})'. Track object is invalid.", tag, m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			void RemoveTag(const char* tag) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Removing Tag '{}' from Track '{} (TrackID: {})'", tag, m_Name, m_ID);
					Mixer::UntagTrack(m_ID, tag);
				}
			}

			[[nodiscard]] bool IsValid() const {
				return m_Valid;
			}

			//static void TrackStopCallback(void* userdata, MIX_Track* track);

			const std::string m_Name;
			const TrackID m_ID{ 0 };

		protected:
			explicit Track(std::string name) : m_Name(std::move(name)), m_ID(s_NextIndex.fetch_add(1, std::memory_order_relaxed)) {
				auto result = Mixer::CreateTrack(this);
				if (!result) {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Failed to create Track '{} (ID: {})'. Error: {}. Invalid Track object was created as a result, this should not crash as the engine prevents you from using an invalid Track object.", m_Name, m_ID, result.error().what());
				} else {
					m_Valid = true;
					CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Track '{} (ID: {})' created.", m_Name, m_ID);
				}
			}

			~Track() {
				Mixer::DestroyTrack(m_ID);
			}

		private:
			bool m_Valid{ false };
			inline static std::atomic<TrackID> s_NextIndex{ 1 };
		};
	}
}