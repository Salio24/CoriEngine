#include "Mixer.hpp"
#include <SDL3_mixer/SDL_mixer.h>

#include "Track.hpp"

namespace Cori {
	namespace {
		struct Data {
			static Data& Get() {
				static Data instance;
				return instance;
			}

			MIX_Mixer* m_Mixer{ nullptr };
			std::unordered_map<uint32_t, MIX_Audio*> m_SDLAudios;
			std::unordered_map<uint32_t, MIX_Track*> m_SDLTracks;
		};
	}


	namespace Audio {
		// use expected
		bool Mixer::PauseAllTracks() {
			return MIX_PauseAllTracks(Data::Get().m_Mixer);
		}

		// use expected
		bool Mixer::ResumeAllTracks() {
			return MIX_ResumeAllTracks(Data::Get().m_Mixer);
		}

		// use expected
		bool Mixer::SetMasterGain(float gain) {
			return MIX_SetMasterGain(Data::Get().m_Mixer, gain);
		}

		// use expected
		float Mixer::GetMasterGain() {
			return MIX_GetMasterGain(Data::Get().m_Mixer);
		}

		// use expected
		bool Mixer::PlayTag(const char* tag, const PlayParams& params) {
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

			bool success = MIX_PlayTag(Data::Get().m_Mixer, tag, props);
			if (!success) {
				CORI_CORE_ERROR("Failed to play tag: {} {}", SDL_GetError(), tag);
			}
			return success;
		}

		// use expected
		bool Mixer::StopTag(const char* tag, uint32_t fadeOutMS) {
			return MIX_StopTag(Data::Get().m_Mixer, tag, fadeOutMS);
		}

		// use expected
		bool Mixer::PauseTag(const char* tag) {
			return MIX_PauseTag(Data::Get().m_Mixer, tag);
		}

		// use expected
		bool Mixer::ResumeTag(const char* tag) {
			return MIX_ResumeTag(Data::Get().m_Mixer, tag);
		}

		// use expected in protected methods as well and check it in track/sound class
		void Mixer::Init() {
			if (CORI_CORE_VERIFY(MIX_Init(), "Failed to initialize SDL_Mixer! Error: {}", SDL_GetError())) {
				CORI_CORE_INFO_TAGGED({"Sound"}, "SDL_Mixer initialized successfully.");
			}

			Data::Get().m_Mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		}
		void Mixer::Shutdown() {
			MIX_Quit();
		}

		void Mixer::LoadSound(const std::filesystem::path& path, bool preDecode, uint32_t index) {
			auto result = MIX_LoadAudio(Data::Get().m_Mixer, path.c_str(), preDecode);
			if (!result) {
				CORI_CORE_ERROR("loadsound error {}", SDL_GetError());
				return;

			}
			Data::Get().m_SDLAudios.insert({index, MIX_LoadAudio(Data::Get().m_Mixer, path.c_str(), preDecode)});
		}

		void Mixer::UnloadSound(uint32_t index) {
			MIX_DestroyAudio(Data::Get().m_SDLAudios.at(index));
		}

		void Mixer::CreateTrack(uint32_t index, Track* trackPtr) {
			Data::Get().m_SDLTracks.insert({index, MIX_CreateTrack(Data::Get().m_Mixer)});
			MIX_SetTrackStoppedCallback(Data::Get().m_SDLTracks.at(index), Track::TrackStopCallback, trackPtr);
		}

		void Mixer::DestroyTrack(uint32_t index) {
			MIX_DestroyTrack(Data::Get().m_SDLTracks.at(index));
		}

		bool Mixer::SetTrackSound(uint32_t track, uint32_t sound) {
			bool success = MIX_SetTrackAudio(Data::Get().m_SDLTracks.at(track), Data::Get().m_SDLAudios.at(sound));
			if (!success) {
				CORI_CORE_ERROR("Failed to set track: {} {}", SDL_GetError(), track, sound);
			}
			CORI_CORE_TRACE("settrack {} {}", track, sound);

			return success;
		}

		bool Mixer::PlayTrack(uint32_t track, const PlayParams& params) {
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

			bool success = MIX_PlayTrack(Data::Get().m_SDLTracks.at(track), props);
			if (!success) {
				CORI_CORE_ERROR("Failed to play track: {} {}", SDL_GetError(), track);
			}
			return success;
		}

		bool Mixer::StopTrack(uint32_t track, uint32_t fadeOutMS) {
			return MIX_StopTrack(Data::Get().m_SDLTracks.at(track), MIX_TrackMSToFrames(Data::Get().m_SDLTracks.at(track), fadeOutMS));
		}

		bool Mixer::PauseTrack(uint32_t track) {
			return MIX_PauseTrack(Data::Get().m_SDLTracks.at(track));
		}

		bool Mixer::ResumeTrack(uint32_t track) {
			return MIX_ResumeTrack(Data::Get().m_SDLTracks.at(track));
		}
		bool Mixer::IsTrackPaused(uint32_t track) {
			return MIX_TrackPaused(Data::Get().m_SDLTracks.at(track));
		}

		bool Mixer::IsTrackPlaying(uint32_t track) {
			return MIX_TrackPlaying(Data::Get().m_SDLTracks.at(track));
		}

		bool Mixer::SetTrackGain(uint32_t track, float gain) {
			return MIX_SetTrackGain(Data::Get().m_SDLTracks.at(track), gain);
		}

		float Mixer::GetTrackGain(uint32_t track) {
			return MIX_GetTrackGain(Data::Get().m_SDLTracks.at(track));
		}

		bool Mixer::TagTrack(uint32_t track, const char* tag) {
			return MIX_TagTrack(Data::Get().m_SDLTracks.at(track), tag);
		}

		void Mixer::UntagTrack(uint32_t track, const char* tag) {
			MIX_UntagTrack(Data::Get().m_SDLTracks.at(track), tag);
		}
	}
}
