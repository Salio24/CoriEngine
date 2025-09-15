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
		/**
		 * @brief Parameters to be used when playing sound, you can mix your audio playback however you want with these.
		 * @note Sub millisecond data will be discarded when params are passed to Mixer::PlayTag, it only works with Mixer::PlayTrack
		 */
		struct PlayParams {
			/**
			 * @brief Number of times to loop the track when it reaches the end. A value of -1 will result in an infinite loop.
			 */
			int32_t Loops{ 0 };

			/**
			 * @brief Mix at most n milliseconds. Default -1.0f, mixes all available audio.
			 */
			float MaxMilliseconds{ -1.0f };

			/**
			 * @brief Start mixing at n'th millisecond. Values <=0 will result in mixing from the very beginning of the tracks input.
			 */
			float StartMillisecond{ 0.0f };

			/**
			 * @brief Start looping at n'th millisecond. Values <=0 will result in looping from the very beginning of the tracks input.
			 */
			float LoopStartMillisecond{ 0.0f };

			/**
			 * @brief Fade in the audio over n milliseconds. Will start from silence and reach full volume smoothly. A value <= 0 disables fade in.
			 */
			float FadeInMilliseconds{ 0.0f };

			/**
			 * @brief Append n milliseconds to the end of a track after all loops (specified in PlayParams::Loops) are complete. A value <= 0 appends nothing.
			 */
			float AppendSilenceMilliseconds{ 0.0f };

			/**
			 * @brief This value is used to define a sequence looping point when passing a sequence to Track::Play, this value is ignored everywhere else.
			 */
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

			[[nodiscard]] std::string ToString() const {
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

		/**
		 * @brief Mixer is responsible for mixing all the sounds, it is a global object and there can only be one Mixer.
		 */
		class Mixer {
		public:

			/**
			 * @brief Pauses all Track that are currently playing on the Mixer.
			 * @note Pausing a Track will not fire its stop callback.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			static std::expected<void, Core::CoriError<>> PauseAllTracks();

			/**
			 * @brief Resumes all Track that are currently paused on the Mixer.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			static std::expected<void, Core::CoriError<>> ResumeAllTracks();


			/**
			 * @brief Sets the master gain of a Mixer.
			 * @param gain Specified gain, negative values are illegal.
			 * @details Gain of 0.0f will completely silence the Mixer, value of 1.0f will not change the Mixer volume, values higher than 1.0f will increase the volume. There is no gain limit specified.
			 * @return Expected object with void on success or CoriError<> on failure.
			 * @note Because there is no limit, this can get very load very quickly, be carefully.
			 */
			static std::expected<void, Core::CoriError<>> SetMasterGain(const float gain);


			/**
			 * @brief Returns the current master gain specified for the Mixer. The default gain for a Mixer is 1.0f.
			 * @return Current Mixer gain.
			 */
			static float GetMasterGain();

			/**
			 * @brief Start (ot restart) mixing all Tracks with a specific tag.
			 * @param tag Specific Track tag.
			 * @param params A set of parameters to use when starting or restarting Tracks.
			 * @details This function behaves very similarly to Track::Start, parameters when passed to this function have a 1ms accuracy,
			 * no sub millisecond when using this unfortunately.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			static std::expected<void, Core::CoriError<>> StartTag(const char* tag, const PlayParams& params = PlayParams{});


			/**
			 * @brief Stops all Track with a specific tab, possibly fading out over time.
			 * @param tag Specific Track tag.
			 * @param fadeOutMS Number of milliseconds to spend fading out to silence before stopping. Optional, 0 by default.
			 * @details If the Track ends normally while the fade-out is still in progress, the audio stops there. The fade is not adjusted to be shorter if it will last longer than the audio remaining.
			 * @note If a Track with the specific tag is currently playing a looped sequence, it will not be stoped, only when current Sound that is playing will be.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			static std::expected<void, Core::CoriError<>> StopTag(const char* tag, const int64_t fadeOutMS = 0);


			/**
			 * @brief Pauses all tracks with a specific tag.
			 * @param tag Specific Track tag.
			 * @note Pausing a Track will not fire its stop callback.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			static std::expected<void, Core::CoriError<>> PauseTag(const char* tag);

			/**
			 * @brief Resumes all Tracks with a specific tag.
			 * @param tag Specific Track tag.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			static std::expected<void, Core::CoriError<>> ResumeTag(const char* tag);


			/**
			 * @brief Sets the gain of all tracks with a specific tag.
			 * @param tag Specific Track tag.
			 * @param gain Specified gain, negative values are illegal.
			 * @details Gain of 0.0f will completely silence the Tracks, value of 1.0f will not change the Tracks volume, values higher than 1.0f will increase the volume. There is no gain limit specified.
			 * @return Expected object with void on success or CoriError<> on failure.
			 * @note Because there is no limit, this can get very load very quickly, be carefully.
			 */
			static std::expected<void, Core::CoriError<>> SetTagGain(const char* tag, const float gain);

		private:
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

			[[nodiscard]] static std::expected<int64_t, Core::CoriError<>> MillisecondsToFrames(const TrackID trackID, const float milliseconds);

			struct Data;
			static Data* s_Data;

		};
	}
}



