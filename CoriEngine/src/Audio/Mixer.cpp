#include "Mixer.hpp"
#include <SDL3_mixer/SDL_mixer.h>
#include "Track.hpp"

#include "entt/core/type_traits.hpp"

namespace Cori {
	namespace Audio {
		namespace {
			void TrackStopCallback(void* userdata, [[maybe_unused]] MIX_Track* track) {
				const auto* coriTrack = static_cast<Track*>(userdata);
				CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Track '{} (ID: {})' stopped playing.", coriTrack->m_Name, coriTrack->m_ID);
			}
		}
		Mixer::Data* Mixer::s_Data{ nullptr };

		struct Mixer::Data {
			MIX_Mixer* m_Mixer{ nullptr };
			std::unordered_map<SoundID, MIX_Audio*> m_SDLAudios;
			std::unordered_map<TrackID, MIX_Track*> m_SDLTracks;
		};

		std::expected<void, CoriError<>> Mixer::PauseAllTracks() {
			const bool success = MIX_PauseAllTracks(s_Data->m_Mixer);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to pause all tracks. SDL_Error: {}", SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Pausing all tracks.");
			return {};
		}

		std::expected<void, CoriError<>> Mixer::ResumeAllTracks() {
			const bool success = MIX_ResumeAllTracks(s_Data->m_Mixer);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to resume all tracks. SDL_Error: {}", SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Resuming all tracks.");
			return {};
		}

		std::expected<void, CoriError<>> Mixer::SetMasterGain(const float gain) {
			const bool success = MIX_SetMasterGain(s_Data->m_Mixer, gain);
			if (!success) {
				return std::unexpected(CoriError<>(std::format("Failed to set master gain. SDL_Error: {}", SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Master gain set to: {}", gain);
			return {};
		}

		float Mixer::GetMasterGain() {
			return MIX_GetMasterGain(s_Data->m_Mixer);
		}

		// use expected
		std::expected<void, CoriError<>> Mixer::PlayTag(const char* tag, const PlayParams& params) {
			// ReSharper disable once CppLocalVariableMayBeConst
			SDL_PropertiesID props = SDL_CreateProperties();
			SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, params.Loops);
			if (params.MaxFrames != -1) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_MAX_FRAME_NUMBER, params.MaxFrames);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER, params.MaxMilliseconds);
			}

			if (params.StartFrame != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_FRAME_NUMBER, params.StartFrame);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, params.StartMillisecond);
			}

			if (params.LoopStartFrame != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOP_START_FRAME_NUMBER, params.LoopStartFrame);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER, params.LoopStartMillisecond);
			}

			if (params.FadeInFrames != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER, params.FadeInFrames);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, params.FadeInMilliseconds);
			}

			if (params.AppendSilenceFrames != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_APPEND_SILENCE_FRAMES_NUMBER, params.AppendSilenceFrames);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_APPEND_SILENCE_MILLISECONDS_NUMBER, params.AppendSilenceMilliseconds);
			}

			const bool success = MIX_PlayTag(s_Data->m_Mixer, tag, props);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to play mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' is set to play.", tag);
			return {};
		}

		std::expected<void, CoriError<>> Mixer::StopTag(const char* tag, const int64_t fadeOutMS) {
			const bool success = MIX_StopTag(s_Data->m_Mixer, tag, fadeOutMS);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to stop mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' stopped.", tag);
			return {};
		}

		std::expected<void, CoriError<>> Mixer::PauseTag(const char* tag) {
			const bool success = MIX_PauseTag(s_Data->m_Mixer, tag);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to pause mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' paused.", tag);
			return {};
		}

		std::expected<void, CoriError<>> Mixer::ResumeTag(const char* tag) {
			const bool success = MIX_ResumeTag(s_Data->m_Mixer, tag);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to resume mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' resumed.", tag);
			return {};
		}

		std::expected<void, CoriError<>> Mixer::SetTagGain(const char* tag, const float gain) {
			const bool success = MIX_SetTagGain(s_Data->m_Mixer, tag, gain);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to set Tag '{}' gain. SDL_Error: {}", tag,  SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' gain set to: {}", tag, gain);
			return {};
		}


		// use expected in protected methods as well and check it in track/sound class
		void Mixer::Init() {
			if (CORI_CORE_VERIFY(MIX_Init(), "Failed to initialize SDL_Mixer! SDL_Error: {}", SDL_GetError())) {}

			CORI_CORE_INFO_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "SDL3_Mixer initialized successfully.");

			s_Data = new Data();

			s_Data->m_Mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		}

		void Mixer::Shutdown() {
			delete s_Data;
			MIX_Quit();
		}

		std::expected<void, CoriError<std::filesystem::path>> Mixer::LoadSound(const std::filesystem::path& path, const bool preDecode, const SoundID soundID) {
			const auto result = MIX_LoadAudio(s_Data->m_Mixer, path.c_str(), preDecode);
			if (!result) {
				return std::unexpected(CoriError<std::filesystem::path>(std::format("Failed to load Sound. SDL_Error: {}", SDL_GetError()), "Path", path));
			}

			s_Data->m_SDLAudios.insert({ soundID, result });
			return {};
		}

		void Mixer::UnloadSound(const SoundID soundID) {
			MIX_DestroyAudio(s_Data->m_SDLAudios.at(soundID));
		}

		std::expected<void, CoriError<>> Mixer::CreateTrack(Track* track) {
			const auto result = MIX_CreateTrack(s_Data->m_Mixer);
			if (!result) {
				return std::unexpected(CoriError(std::format("Failed to create Track. SDL_Error: {}", SDL_GetError())));
			}

			MIX_SetTrackStoppedCallback(result, TrackStopCallback, track);
			s_Data->m_SDLTracks.insert({ track->m_ID, result });
			return {};
		}

		void Mixer::DestroyTrack(const TrackID trackID) {
			MIX_DestroyTrack(s_Data->m_SDLTracks.at(trackID));
		}

		std::expected<void, CoriError<>> Mixer::SetTrackSound(const Track* track, const Sound* sound) {
			const bool success = MIX_SetTrackAudio(s_Data->m_SDLTracks.at(track->m_ID), s_Data->m_SDLAudios.at(sound->m_ID));
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. SDL_Error: {}", sound->m_Name, sound->m_ID, track->m_Name, track->m_ID, SDL_GetError())));
			}

			return {};
		}

		std::expected<void, CoriError<>> Mixer::PlayTrack(const Track* track, const PlayParams& params) {
			// ReSharper disable once CppLocalVariableMayBeConst
			SDL_PropertiesID props = SDL_CreateProperties();
			SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, params.Loops);
			if (params.MaxFrames != -1) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_MAX_FRAME_NUMBER, params.MaxFrames);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER, params.MaxMilliseconds);
			}

			if (params.StartFrame != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_FRAME_NUMBER, params.StartFrame);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, params.StartMillisecond);
			}

			if (params.LoopStartFrame != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOP_START_FRAME_NUMBER, params.LoopStartFrame);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER, params.LoopStartMillisecond);
			}

			if (params.FadeInFrames != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER, params.FadeInFrames);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, params.FadeInMilliseconds);
			}

			if (params.AppendSilenceFrames != 0) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_APPEND_SILENCE_FRAMES_NUMBER, params.AppendSilenceFrames);
			} else {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_APPEND_SILENCE_MILLISECONDS_NUMBER, params.AppendSilenceMilliseconds);
			}
			const bool success = MIX_PlayTrack(s_Data->m_SDLTracks.at(track->m_ID), props);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to play Track '{} (TrackID: {})'. SDL_Error: {}", track->m_Name, track->m_ID, SDL_GetError())));
			}
			return {};
		}

		std::expected<void, CoriError<>> Mixer::StopTrack(const Track* track, const int64_t fadeOutMS) {
			const bool success = MIX_StopTrack(s_Data->m_SDLTracks.at(track->m_ID), MIX_TrackMSToFrames(s_Data->m_SDLTracks.at(track->m_ID), fadeOutMS));
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to stop Track '{} (TrackID: {})'. SDL_Error: '{}'", track->m_Name, track->m_ID, SDL_GetError())));
			}
			return {};
		}

		std::expected<void, CoriError<>> Mixer::PauseTrack(const Track* track) {
			const bool success =  MIX_PauseTrack(s_Data->m_SDLTracks.at(track->m_ID));
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to pause Track '{} (TrackID: {})'. SDL_Error: {}", track->m_Name, track->m_ID, SDL_GetError())));
			}
			return {};
		}

		std::expected<void, CoriError<>> Mixer::ResumeTrack(const Track* track) {
			const bool success = MIX_ResumeTrack(s_Data->m_SDLTracks.at(track->m_ID));
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to pause Track '{} (TrackID: {})'. SDL_Error: {}", track->m_Name, track->m_ID, SDL_GetError())));
			}
			return {};
		}
		bool Mixer::IsTrackPaused(const TrackID trackID) {
			return MIX_TrackPaused(s_Data->m_SDLTracks.at(trackID));
		}

		bool Mixer::IsTrackPlaying(const TrackID trackID) {
			return MIX_TrackPlaying(s_Data->m_SDLTracks.at(trackID));
		}

		std::expected<void, CoriError<>> Mixer::SetTrackGain(const Track* track, const float gain) {
			const bool success = MIX_SetTrackGain(s_Data->m_SDLTracks.at(track->m_ID), gain);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to set Track '{} (TrackID: {})' gain. SDL_Error: {}", track->m_Name, track->m_ID, SDL_GetError())));
			}

			return {};
		}

		float Mixer::GetTrackGain(const TrackID trackID) {
			return MIX_GetTrackGain(s_Data->m_SDLTracks.at(trackID));
		}

		std::expected<void, CoriError<>> Mixer::TagTrack(const Track* track, const char* tag) {
			const bool success = MIX_TagTrack(s_Data->m_SDLTracks.at(track->m_ID), tag);
			if (!success) {
				return std::unexpected(CoriError(std::format("Failed to assign the tag the Track '{} (TrackID: {}'. SDL_Error: {}", track->m_Name, track->m_ID, SDL_GetError())));
			}

			return {};
		}

		void Mixer::UntagTrack(const TrackID trackID, const char* tag) {
			MIX_UntagTrack(s_Data->m_SDLTracks.at(trackID), tag);
		}
	}
}
