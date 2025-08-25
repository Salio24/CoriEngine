#pragma once
#include "IDDefs.hpp"

namespace Cori {
	class Application;

	namespace Audio {
		class Track;
		class Sound;

		struct PlayParams {
			int32_t Loops{ 0 };
			int32_t MaxFrames{ -1 };
			int32_t MaxMilliseconds{ -1 };
			int32_t StartFrame{ 0 };
			int32_t StartMillisecond{ 0 };
			int32_t LoopStartFrame{ 0 };
			int32_t LoopStartMillisecond{ 0 };
			int32_t FadeInFrames{ 0 };
			int32_t FadeInMilliseconds{ 0 };
			int32_t AppendSilenceFrames{ 0 };
			int32_t AppendSilenceMilliseconds{ 0 };

			friend std::ostream& operator<<(std::ostream& os, const PlayParams& params) {
				os << "\n" <<
					"| Loops                    : " << params.Loops << " |" <<
					"| MaxFrames                : " << params.MaxFrames << " |" <<
					"| MaxMilliseconds          : " << params.MaxMilliseconds << " |" <<
					"| StartFrame               : " << params.StartFrame << " |" <<
					"| StartMillisecond         : " << params.StartMillisecond << " |" <<
					"| LoopStartFrame           : " << params.LoopStartFrame << " |" <<
					"| LoopStartMillisecond     : " << params.LoopStartMillisecond << " |" <<
					"| FadeInFrames:            : " << params.FadeInFrames << " |" <<
					"| FadeInMilliseconds       : " << params.FadeInMilliseconds << " |" <<
					"| AppendSilenceFrames      : " << params.AppendSilenceFrames << " |" <<
					"| AppendSilenceMilliseconds: " << params.AppendSilenceMilliseconds << " |";
				return os;
			}

			std::string Stringify() const {
				std::string out = "\n" +
					std::string("| Loops                    : ") + std::to_string(Loops) + " |" +
					std::string("| MaxFrames                : ") + std::to_string(MaxFrames) + " |" +
					std::string("| MaxMilliseconds          : ") + std::to_string(MaxMilliseconds) + " |" +
					std::string("| StartFrame               : ") + std::to_string(StartFrame) + " |" +
					std::string("| StartMillisecond         : ") + std::to_string(StartMillisecond) + " |" +
					std::string("| LoopStartFrame           : ") + std::to_string(LoopStartFrame) + " |" +
					std::string("| LoopStartMillisecond     : ") + std::to_string(LoopStartMillisecond) + " |" +
					std::string("| FadeInFrames:            : ") + std::to_string(FadeInFrames) + " |" +
					std::string("| FadeInMilliseconds       : ") + std::to_string(FadeInMilliseconds) + " |" +
					std::string("| AppendSilenceFrames      : ") + std::to_string(AppendSilenceFrames) + " |" +
					std::string("| AppendSilenceMilliseconds: ") + std::to_string(AppendSilenceMilliseconds) + " |";
				return out;
			}
		};

		class Mixer {
		public:

			[[nodiscard]] static std::expected<void, CoriError<>> PauseAllTracks();
			[[nodiscard]] static std::expected<void, CoriError<>> ResumeAllTracks();

			[[nodiscard]] static std::expected<void, CoriError<>> SetMasterGain(const float gain);
			[[nodiscard]] static float GetMasterGain();

			[[nodiscard]] static std::expected<void, CoriError<>> PlayTag(const char* tag, const PlayParams& params = PlayParams{});
			[[nodiscard]] static std::expected<void, CoriError<>> StopTag(const char* tag, const int64_t fadeOutMS = 0);
			[[nodiscard]] static std::expected<void, CoriError<>> PauseTag(const char* tag);
			[[nodiscard]] static std::expected<void, CoriError<>> ResumeTag(const char* tag);

			[[nodiscard]] static std::expected<void, CoriError<>> SetTagGain(const char* tag, const float gain);



		protected:
			friend Track;
			friend Sound;
			friend Application;

			static void Init();
			static void Shutdown();

			[[nodiscard]] static std::expected<void, CoriError<std::filesystem::path>> LoadSound(const std::filesystem::path& path, const bool preDecode, const SoundID soundID);

			static void UnloadSound(const SoundID soundID);

			[[nodiscard]] static std::expected<void, CoriError<>> CreateTrack(Track* track);

			static void DestroyTrack(const TrackID trackID);

			[[nodiscard]] static std::expected<void, CoriError<>> SetTrackSound(const Track* track, const Sound* sound);

			[[nodiscard]] static std::expected<void, CoriError<>> PlayTrack(const Track* track, const PlayParams& params = {});

			[[nodiscard]] static std::expected<void, CoriError<>> StopTrack(const Track* track, const int64_t fadeOutMS = 0);

			[[nodiscard]] static std::expected<void, CoriError<>> PauseTrack(const Track* track);

			[[nodiscard]] static std::expected<void, CoriError<>> ResumeTrack(const Track* track);

			[[nodiscard]] static bool IsTrackPaused(const TrackID trackID);

			[[nodiscard]] static bool IsTrackPlaying(const TrackID trackID);

			[[nodiscard]] static std::expected<void, CoriError<>> SetTrackGain(const Track* track, const float gain);

			[[nodiscard]] static float GetTrackGain(const TrackID trackID);

			[[nodiscard]] static std::expected<void, CoriError<>> TagTrack(const Track* track, const char* tag);

			static void UntagTrack(const TrackID trackID, const char* tag);
		private:
			struct Data;
			static Data* s_Data;

		};
	}
}



