#pragma once
#include "Profiling/Trackable.hpp"
#include "Mixer.hpp"
#include "Sound.hpp"

struct MIX_Track;

namespace Cori {
	namespace Audio {
		using TrackStopCallbackFn = std::function<void()>;
		using SoundWithParams = std::pair<std::shared_ptr<Sound>, PlayParams>;

		template<typename T>
		concept IsSoundWithParams = std::is_same_v<T, SoundWithParams>;

		/**
		 * @brief You use Track to play and mix Sound objects.
		 */
		class Track : public Profiling::Trackable<Track> {
		public:
			/**
			 * @brief Assigns the Sound asset to the Track.
			 * @param sound Sound asset.
			 * @note You generally want to use Play, this is only used when you have a Track that will not change its Sound asset ofter and will be started/restarted very-very often.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> SetSound(const std::shared_ptr<Sound>& sound);


			/**
			 * @brief Starts the Track that has a preassigned Sound asset.
			 * @param params A set of parameters to use when starting or restarting Tracks.
			 * @note You generally want to use Play, this is only used when you have a Track that will not change its Sound asset ofter and will be started/restarted very-very often.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> Start(const PlayParams& params = PlayParams{});


			/**
			 * @brief Plays a single or a sequence of SoundWithParams objects.
			 * @param sequence A sequence of object of type SoundWithParams.
			 * @details It creates a looped sequence of Sounds each with its own Mixer::PlayParams, in params there is a variable called LoopedInSequence, it defines the looping point of the sequence,
			 * the function will play all sequenced sounds once until it sees a LoopedInSequence=true in the sequence, object it belongs to becomes a looping point.
			 * Looping begins at the looping point and ends at the end if the sequence and then restarts, all LoopedInSequence variables in the SoundWithParams after the looping point has no affect on the looping behaviour.
			 * Looping will be indefinite, until the Stop method is called.
			 * \n If only one SoundWithParams object was passed, if LoopedInSequence=true it will start an infinite loop, otherwise it will just play it once.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> Play(const IsSoundWithParams auto&... sequence) {
				static_assert(sizeof...(sequence) > 2, "Sequence should contain at least 2 SoundWithParams objects.");
					if (m_Valid) {
						if (!m_ActiveSequence) {
							m_SoundSequence.clear();
							m_SoundSequence.reserve(sizeof...(sequence));
							(..., m_SoundSequence.push_back(sequence));
							m_EraseLastInSequence = false;
							m_SequenceIntroFinished = false;

							m_CurrentLoopedSequenceIndex = 0;
							auto& initialPart = m_SoundSequence.back();

							const auto success = PlaySoundWithParams(initialPart);
							if (!success) {
								CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "An error occurred when trying to play a part of sequence. Details: {}", success.error().what());
							}

							m_ActiveSequence = true;
							if constexpr (sizeof...(sequence) > 2) {
								if (!initialPart.second.LoopedInSequence) {
									m_EraseLastInSequence = true;
								} else {
									m_SequenceIntroFinished = true;
									m_CurrentLoopedSequenceIndex = m_SoundSequence.size() - 2;
								}

								SetTrackStopCallbackInternal([this] {
									if (m_EraseLastInSequence) {
										m_SoundSequence.pop_back();
										m_EraseLastInSequence = false;
									}
									if (!m_SoundSequence.empty()) {
											SoundWithParams* part;

											if (m_SequenceIntroFinished) {
												part = &m_SoundSequence.at(m_CurrentLoopedSequenceIndex);
											}
											else {
												part = &m_SoundSequence.back();
											}

											m_ActiveSequence = false;
											const auto success_ = PlaySoundWithParams(*part);
											if (!success_) {
												CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "An error occurred when trying to play a part of sequence. Details: {}", success_.error().what());
											}
											m_ActiveSequence = true;

											if (!part->second.LoopedInSequence && !m_SequenceIntroFinished) {
												m_EraseLastInSequence = true;
											}
											else {
												if (m_SequenceIntroFinished) {
													if (m_CurrentLoopedSequenceIndex == 0) {
														m_CurrentLoopedSequenceIndex = m_SoundSequence.size() - 1;
													}
													else {
														--m_CurrentLoopedSequenceIndex;
													}
												}
												else {
													m_SequenceIntroFinished = true;
													if (m_CurrentLoopedSequenceIndex == 0) {
														m_CurrentLoopedSequenceIndex = m_SoundSequence.size() - 2;
													} else {
														--m_CurrentLoopedSequenceIndex;
													}
												}
											}
									} else {
										const auto success_ = Stop(false);
										if (!success_) {
											CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "An error occurred when trying to stop a sequence inside of Engine side Callback. Details: {}", success_.error().what());
										}
									}
								});
								return {};
							}
							else {
								if (initialPart.second.LoopedInSequence) {
									SetTrackStopCallbackInternal([this] {
										SoundWithParams* part = &m_SoundSequence.at(0);
										m_ActiveSequence = false;
										const auto success_ = PlaySoundWithParams(*part);
										if (!success_) {
											CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "An error occurred when trying to play a part of sequence. Details: {}", success_.error().what());
										}
										m_ActiveSequence = true;
									});
								} else {
									m_ActiveSequence = false;
									return PlaySoundWithParams(initialPart);
								}
							}
						}

						return std::unexpected(Core::CoriError(std::format("Failed to play sequence on Track '{} (TrackID: {})'. A sequence is already playing on this track.", m_Name, m_ID)));
					}

					return std::unexpected(Core::CoriError(std::format("Failed to play sequence on Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));

			}

			/**
			 * @brief Stops the Track.
			 * @param abruptStop Stopping mode.
			 * @param fadeOutMS Number of milliseconds to spend fading out to silence before stopping. Has no effect if abruptStop is false. Optional, 0 by default.
			 * @details If abruptStop is false, the Track will stop any sequence its playing, but will let the already playing Sound asset to finish mixing.
			 * If abruptStop is true, the Track will stop any sequence its playing and stop mixing immediately.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> Stop(const bool abruptStop, const int64_t fadeOutMS = 0);


			/**
			 * @brief Pauses the Track.
			 * @note Pausing a Track will not fire its stop callback.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> Pause();


			/**
			 * @brief Resumes the Track.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> Resume();

			/**
			 * @brief Checks if the Track is paused.
			 * @return Paused state.
			 */
			[[nodiscard]] bool IsPaused() const;


			/**
			 * @brief Checks if the Track is playing.
			 * @return Playing state.
			 */
			[[nodiscard]] bool IsPlaying() const;

			/**
			 * @brief Sets the Track gain.
			 * @param gain Specified gain, negative values are illegal.
			 * @details Gain of 0.0f will completely silence the Track, value of 1.0f will not change the Track volume, values higher than 1.0f will increase the volume. There is no gain limit specified.
			 * @return Expected object with void on success or CoriError<> on failure.
			 * @note Because there is no limit, this can get very load very quickly, be carefully.
			 */
			std::expected<void, Core::CoriError<>> SetGain(const float gain);


			/**
			 * @brief Returns the current Track gain. The default gain for a Track is 1.0f.
			 * @return Current Track gain.
			 */
			[[nodiscard]] float GetGain() const;

			/**
			 * @brief Assigns the tag to the Track.
			 * @param tag Tag to be assigned.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			std::expected<void, Core::CoriError<>> SetTag(const char* tag);

			/**
			 * @brief Removes the tag from the Track.
			 * @param tag Tag to be removed.
			 * @param preserveCachedTag Whether to leave the removed cache tag.
			 * @details With preserveCachedTag=true it will leave the tag cached, it will be still available via GetTag(), otherwise it will erase a tag from cache.
			 */
			void RemoveTag(const char* tag, const bool preserveCachedTag = false);

			/**
			 * @brief Gets the active/cached Track tag.
			 * @return String View of the Tag cache.
			 */
			[[nodiscard]] std::string_view GetTag() const;

			/**
			 * @brief Check if the Track is valid.
			 * @return Validity state.
			 * @note Generally there is no need to explicitly check for sound validity, because Track already does so.
			 */
			[[nodiscard]] bool IsValid() const;

			/**
			 * @brief Returns the TrackID associated with Track.
			 */
			[[nodiscard]] TrackID GetID() const;

			/**
			 * @brief Sets a callback to be run when the Track stops playing.
			 * @param callback Lambda ot function bind.
			 * @note When a sequence is playing this callback will be called for every part of the sequence.
			 */
			void SetTrackStopCallback(TrackStopCallbackFn callback);

			/**
			 * @brief Creates a Track object.
			 * @param name Name to be assigned to the Track.
			 * @return Shared pointer to the created Track object.
			 */
			[[nodiscard]] static std::shared_ptr<Track> Create(std::string name);

			~Track();

			const std::string m_Name;

			/**
			 * @brief For internal use only!
			 */
			static void TrackStopCallback(void* userdata, MIX_Track* track);

		private:
			explicit Track(std::string name);

			std::expected<void, Core::CoriError<>> PlaySoundWithParams(SoundWithParams& object);

			std::expected<void, Core::CoriError<>> StopInternal(const int64_t fadeOutMS) const;

			TrackStopCallbackFn m_EngineCallBack;
			TrackStopCallbackFn m_ClientCallBack;

			std::expected<void, Core::CoriError<>> ProcessSequencePart(const SoundWithParams& part);

			void SetTrackStopCallbackInternal(TrackStopCallbackFn callback);

			const TrackID m_ID{ 0 };
			bool m_Valid{ false };
			bool m_ActiveSequence{ false };
			bool m_EraseLastInSequence{ false };
			bool m_SequenceIntroFinished{ false };
			uint32_t m_CurrentLoopedSequenceIndex{ 0 };
			std::string m_CurrentTag{};
			std::vector<SoundWithParams> m_SoundSequence;
			inline static std::atomic<TrackID> s_NextIndex{ 1 };
		};
	}
}