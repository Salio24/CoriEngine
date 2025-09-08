#include "Mixer.hpp"
#include <SDL3_mixer/SDL_mixer.h>
#include "Track.hpp"


#include "entt/core/type_traits.hpp"

namespace Cori {
	namespace Audio {
		Mixer::Data* Mixer::s_Data{ nullptr };

		struct Mixer::Data {
			MIX_Mixer* m_Mixer{ nullptr };
			std::unordered_map<SoundID, MIX_Audio*> m_SDLAudios;
			std::unordered_map<TrackID, std::pair<MIX_Track*, Track*>> m_TrackPool;
		};

		std::expected<void, Core::CoriError<>> Mixer::PauseAllTracks() {
			const bool success = MIX_PauseAllTracks(s_Data->m_Mixer);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to pause all tracks. SDL_Error: {}", SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Pausing all tracks.");
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::ResumeAllTracks() {
			const bool success = MIX_ResumeAllTracks(s_Data->m_Mixer);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to resume all tracks. SDL_Error: {}", SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Resuming all tracks.");
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::SetMasterGain(const float gain) {
			const bool success = MIX_SetMasterGain(s_Data->m_Mixer, gain);
			if (!success) {
				return std::unexpected(Core::CoriError<>(std::format("Failed to set master gain. SDL_Error: {}", SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Master gain set to: {}", gain);
			return {};
		}

		float Mixer::GetMasterGain() {
			return MIX_GetMasterGain(s_Data->m_Mixer);
		}

		std::expected<void, Core::CoriError<>> Mixer::PlayTag(const char* tag, const PlayParams& params) {
			const SDL_PropertiesID props = SDL_CreateProperties();
			SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, params.Loops);
			if (params.MaxMilliseconds != -1.0f) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER, params.MaxMilliseconds);
			}

			if (params.StartMillisecond != 0.0f) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, params.StartMillisecond);
			}

			if (params.LoopStartMillisecond != 0.0f) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER, params.LoopStartMillisecond);
			}

			if (params.FadeInMilliseconds != 0.0f) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, params.FadeInMilliseconds);
			}

			if (params.AppendSilenceMilliseconds != 0.0f) {
				SDL_SetNumberProperty(props, MIX_PROP_PLAY_APPEND_SILENCE_MILLISECONDS_NUMBER, params.AppendSilenceMilliseconds);
			}

			const bool success = MIX_PlayTag(s_Data->m_Mixer, tag, props);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to play mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' is set to play.", tag);
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::StopTag(const char* tag, const int64_t fadeOutMS) {
			const bool success = MIX_StopTag(s_Data->m_Mixer, tag, fadeOutMS);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to stop mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' stopped.", tag);
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::PauseTag(const char* tag) {
			const bool success = MIX_PauseTag(s_Data->m_Mixer, tag);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to pause mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' paused.", tag);
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::ResumeTag(const char* tag) {
			const bool success = MIX_ResumeTag(s_Data->m_Mixer, tag);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to resume mixer Tag '{}'. SDL_Error: {}", tag, SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' resumed.", tag);
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::SetTagGain(const char* tag, const float gain) {
			const bool success = MIX_SetTagGain(s_Data->m_Mixer, tag, gain);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to set Tag '{}' gain. SDL_Error: {}", tag,  SDL_GetError())));
			}
			CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Mixer }, "Tag '{}' gain set to: {}", tag, gain);
			return {};
		}

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

		std::expected<void, Core::CoriError<std::filesystem::path>> Mixer::LoadSound(const std::filesystem::path& path, const bool preDecode, const SoundID soundID) {
			const auto result = MIX_LoadAudio(s_Data->m_Mixer, path.c_str(), preDecode);
			if (!result) {
				return std::unexpected(Core::CoriError<std::filesystem::path>(std::format("Failed to load Sound. SDL_Error: {}", SDL_GetError()), "Path", path));
			}

			s_Data->m_SDLAudios.insert({ soundID, result });
			return {};
		}

		void Mixer::UnloadSound(const SoundID soundID) {
			MIX_DestroyAudio(s_Data->m_SDLAudios.at(soundID));
			s_Data->m_SDLAudios.erase(soundID);
		}

		std::expected<void, Core::CoriError<>> Mixer::CreateTrack(Track* track) {
			const auto result = MIX_CreateTrack(s_Data->m_Mixer);
			if (!result) {
				return std::unexpected(Core::CoriError(std::format("Failed to create Track. SDL_Error: {}", SDL_GetError())));
			}

			MIX_SetTrackStoppedCallback(result, Track::TrackStopCallback, track);
			s_Data->m_TrackPool.insert({ track->m_ID, std::make_pair(result, track) });
			return {};
		}

		void Mixer::DestroyTrack(const TrackID trackID) {
			MIX_DestroyTrack(s_Data->m_TrackPool.at(trackID).first);
			s_Data->m_TrackPool.erase(trackID);
		}

		std::expected<void, Core::CoriError<>> Mixer::SetTrackSound(const TrackID trackID, const Sound* sound) {
			const bool success = MIX_SetTrackAudio(s_Data->m_TrackPool.at(trackID).first, s_Data->m_SDLAudios.at(sound->m_ID));
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. SDL_Error: {}", sound->m_Name, sound->m_ID, s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}

			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::PlayTrack(const TrackID trackID, const PlayParams& params) {
			const SDL_PropertiesID props = SDL_CreateProperties();
			SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, params.Loops);
			if (params.MaxMilliseconds != -1.0f) {
				auto samples = MillisecondsToFrames(trackID, params.MaxMilliseconds);
				if (samples) {
					SDL_SetNumberProperty(props, MIX_PROP_PLAY_MAX_FRAME_NUMBER, samples.value());
				} else {
					return std::unexpected(samples.error());
				}
			}

			if (params.StartMillisecond != 0.0f) {
				auto samples = MillisecondsToFrames(trackID, params.StartMillisecond);
				if (samples) {
					SDL_SetNumberProperty(props, MIX_PROP_PLAY_START_FRAME_NUMBER, samples.value());
				} else {
					return std::unexpected(samples.error());
				}
			}

			if (params.LoopStartMillisecond != 0.0f) {
				auto samples = MillisecondsToFrames(trackID, params.LoopStartMillisecond);
				if (samples) {
					SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOP_START_FRAME_NUMBER, samples.value());
				} else {
					return std::unexpected(samples.error());
				}
			}

			if (params.FadeInMilliseconds != 0.0f) {
				auto samples = MillisecondsToFrames(trackID, params.FadeInMilliseconds);
				if (samples) {
					SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER, samples.value());
				} else {
					return std::unexpected(samples.error());
				}
			}

			if (params.AppendSilenceMilliseconds != 0.0f) {
				auto samples = MillisecondsToFrames(trackID, params.AppendSilenceMilliseconds);
				if (samples) {
					SDL_SetNumberProperty(props, MIX_PROP_PLAY_APPEND_SILENCE_FRAMES_NUMBER, samples.value());
				} else {
					return std::unexpected(samples.error());
				}
			}

			const bool success = MIX_PlayTrack(s_Data->m_TrackPool.at(trackID).first, props);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to play Track '{} (TrackID: {})'. SDL_Error: {}", s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::StopTrack(const TrackID trackID, const int64_t fadeOutMS) {
			const bool success = MIX_StopTrack(s_Data->m_TrackPool.at(trackID).first, MIX_TrackMSToFrames(s_Data->m_TrackPool.at(trackID).first, fadeOutMS));
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to stop Track '{} (TrackID: {})'. SDL_Error: '{}'", s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::PauseTrack(const TrackID trackID) {
			const bool success =  MIX_PauseTrack(s_Data->m_TrackPool.at(trackID).first);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to pause Track '{} (TrackID: {})'. SDL_Error: {}", s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}
			return {};
		}

		std::expected<void, Core::CoriError<>> Mixer::ResumeTrack(const TrackID trackID) {
			const bool success = MIX_ResumeTrack(s_Data->m_TrackPool.at(trackID).first);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to pause Track '{} (TrackID: {})'. SDL_Error: {}", s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}
			return {};
		}

		bool Mixer::IsTrackPaused(const TrackID trackID) {
			return MIX_TrackPaused(s_Data->m_TrackPool.at(trackID).first);
		}

		bool Mixer::IsTrackPlaying(const TrackID trackID) {
			return MIX_TrackPlaying(s_Data->m_TrackPool.at(trackID).first);
		}

		std::expected<void, Core::CoriError<>> Mixer::SetTrackGain(const TrackID trackID, const float gain) {
			const bool success = MIX_SetTrackGain(s_Data->m_TrackPool.at(trackID).first, gain);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to set Track '{} (TrackID: {})' gain. SDL_Error: {}", s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}

			return {};
		}

		float Mixer::GetTrackGain(const TrackID trackID) {
			return MIX_GetTrackGain(s_Data->m_TrackPool.at(trackID).first);
		}

		std::expected<void, Core::CoriError<>> Mixer::TagTrack(const TrackID trackID, const char* tag) {
			const bool success = MIX_TagTrack(s_Data->m_TrackPool.at(trackID).first, tag);
			if (!success) {
				return std::unexpected(Core::CoriError(std::format("Failed to assign the tag the Track '{} (TrackID: {}'. SDL_Error: {}", s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}

			return {};
		}

		void Mixer::UntagTrack(const TrackID trackID, const char* tag) {
			MIX_UntagTrack(s_Data->m_TrackPool.at(trackID).first, tag);
		}

		std::expected<int64_t, Core::CoriError<>> Mixer::MillisecondsToFrames(const TrackID trackID, const float milliseconds) {
			const int64_t sampleRate = MIX_TrackMSToFrames(s_Data->m_TrackPool.at(trackID).first, 1000);

			if (sampleRate == -1) {
				return std::unexpected(Core::CoriError(std::format("Failed to convert milliseconds to samples for Track '{} (TrackID: {}'. SDL_Error: {}", s_Data->m_TrackPool.at(trackID).second->m_Name, trackID, SDL_GetError())));
			}

			return milliseconds / 1000.0f * static_cast<float>(sampleRate);



		}
	}
}
