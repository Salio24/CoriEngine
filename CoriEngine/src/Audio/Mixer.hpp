#pragma once

// TODO: rename track argument to trackID

namespace Cori {
	class Application;

	namespace Audio {

		class Track;

		struct PlayParams {
			int Loops{ 0 };
			int MaxFrames{ -1 };
			int MaxMilliseconds{ -1 };
			int StartFrame{ 0 };
			int StartMillisecond{ 0 };
			int LoopStartFrame{ 0 };
			int LoopStartMillisecond{ 0 };
			int FadeInFrames{ 0 };
			int FadeInMilliseconds{ 0 };
			int AppendSilenceFrames{ 0 };
			int AppendSilenceMilliseconds{ 0 };
		};

		class Mixer {
		public:

			static bool PauseAllTracks();

			static bool ResumeAllTracks();

			static bool SetMasterGain(float gain);

			static float GetMasterGain();

			static bool PlayTag(const char* tag, const PlayParams& params = PlayParams{});

			static bool StopTag(const char* tag, uint32_t fadeOutMS);

			static bool PauseTag(const char* tag);

			static bool ResumeTag(const char* tag);

		protected:
			friend  Track;
			friend class Sound;
			friend Application;

			static void Init();
			static void Shutdown();

			static void LoadSound(const std::filesystem::path& path, bool preDecode, uint32_t index);

			static void UnloadSound(uint32_t index);

			static void CreateTrack(uint32_t index, Track* trackPtr);

			static void DestroyTrack(uint32_t index);

			static bool SetTrackSound(uint32_t track, uint32_t sound);

			static bool PlayTrack(uint32_t track, const PlayParams& params = {});

			static bool StopTrack(uint32_t track, uint32_t fadeOutMS);

			static bool PauseTrack(uint32_t track);

			static bool ResumeTrack(uint32_t track);

			static bool IsTrackPaused(uint32_t track);

			static bool IsTrackPlaying(uint32_t track);

			static bool SetTrackGain(uint32_t track, float gain);

			static float GetTrackGain(uint32_t track);

			static bool TagTrack(uint32_t track, const char* tag);

			static void UntagTrack(uint32_t track, const char* tag);
		private:
			struct Data;
			static Data* s_Data;

		};
	}
}



