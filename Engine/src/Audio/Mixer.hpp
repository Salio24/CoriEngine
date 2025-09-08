#pragma once
#include "IDDefs.hpp"

namespace Cori {
	namespace Core {
		class Application;
	}

	namespace Audio {
		class Track;
		class Sound;

		// Note: sub millisecond data will be discarded when params are passed to Mixer::PlayTag, it only works with Mixer::PlayTrack
		struct PlayParams {
			int32_t Loops{ 0 };
			float MaxMilliseconds{ -1.0f };
			float StartMillisecond{ 0.0f };
			float LoopStartMillisecond{ 0.0f };
			float FadeInMilliseconds{ 0.0f };
			float AppendSilenceMilliseconds{ 0.0f };
			bool LoopedInSequence{ true };

			friend std::ostream& operator<<(std::ostream& os, const PlayParams& params) {
				os << "\n" <<
					"| Loops                    : " << params.Loops << " |" <<
					"| MaxMilliseconds          : " << params.MaxMilliseconds << " |" <<
					"| StartMillisecond         : " << params.StartMillisecond << " |" <<
					"| LoopStartMillisecond     : " << params.LoopStartMillisecond << " |" <<
					"| FadeInMilliseconds       : " << params.FadeInMilliseconds << " |" <<
					"| AppendSilenceMilliseconds: " << params.AppendSilenceMilliseconds << " |";
				return os;
			}

			[[nodiscard]] std::string Stringify() const {
				std::string out = "\n" +
					std::string("| Loops                    : ") + std::to_string(Loops) + " |" +
					std::string("| MaxMilliseconds          : ") + std::to_string(MaxMilliseconds) + " |" +
					std::string("| StartMillisecond         : ") + std::to_string(StartMillisecond) + " |" +
					std::string("| LoopStartMillisecond     : ") + std::to_string(LoopStartMillisecond) + " |" +
					std::string("| FadeInMilliseconds       : ") + std::to_string(FadeInMilliseconds) + " |" +
					std::string("| AppendSilenceMilliseconds: ") + std::to_string(AppendSilenceMilliseconds) + " |";
				return out;
			}
		};

		class Mixer {
		public:

			[[nodiscard]] static std::expected<void, Core::CoriError<>> PauseAllTracks();
			[[nodiscard]] static std::expected<void, Core::CoriError<>> ResumeAllTracks();

			[[nodiscard]] static std::expected<void, Core::CoriError<>> SetMasterGain(const float gain);
			[[nodiscard]] static float GetMasterGain();

			[[nodiscard]] static std::expected<void, Core::CoriError<>> PlayTag(const char* tag, const PlayParams& params = PlayParams{});
			[[nodiscard]] static std::expected<void, Core::CoriError<>> StopTag(const char* tag, const int64_t fadeOutMS = 0);
			[[nodiscard]] static std::expected<void, Core::CoriError<>> PauseTag(const char* tag);
			[[nodiscard]] static std::expected<void, Core::CoriError<>> ResumeTag(const char* tag);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> SetTagGain(const char* tag, const float gain);

		protected:
			friend Track;
			friend Sound;
			friend Core::Application;

			static void Init();
			static void Shutdown();

			[[nodiscard]] static std::expected<void, Core::CoriError<std::filesystem::path>> LoadSound(const std::filesystem::path& path, const bool preDecode, const SoundID soundID);

			static void UnloadSound(const SoundID soundID);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> CreateTrack(Track* track);

			static void DestroyTrack(const TrackID trackID);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> SetTrackSound(const TrackID trackID, const Sound* sound);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> PlayTrack(const TrackID trackID, const PlayParams& params = {});

			[[nodiscard]] static std::expected<void, Core::CoriError<>> StopTrack(const TrackID trackID, const int64_t fadeOutMS = 0);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> PauseTrack(const TrackID trackID);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> ResumeTrack(const TrackID trackID);

			[[nodiscard]] static bool IsTrackPaused(const TrackID trackID);

			[[nodiscard]] static bool IsTrackPlaying(const TrackID trackID);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> SetTrackGain(const TrackID trackID, const float gain);

			[[nodiscard]] static float GetTrackGain(const TrackID trackID);

			[[nodiscard]] static std::expected<void, Core::CoriError<>> TagTrack(const TrackID trackID, const char* tag);

			static void UntagTrack(const TrackID trackID, const char* tag);
		private:
			[[nodiscard]] static std::expected<int64_t, Core::CoriError<>> MillisecondsToFrames(const TrackID trackID, const float milliseconds);

			struct Data;
			static Data* s_Data;

		};
	}
}



