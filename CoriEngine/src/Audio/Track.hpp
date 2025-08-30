#pragma once
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"
#include "Mixer.hpp"
#include "Sound.hpp"

struct MIX_Track;

namespace Cori {
	namespace Audio {
		using TrackStopCallbackFn = std::function<void()>;
		using SoundWithParams = std::pair<std::shared_ptr<Sound>, PlayParams>;

		template<typename T>
		concept IsSoundWithParams = std::is_same_v<T, SoundWithParams>;
		
		class Track : public Profiling::Trackable<Track>, public SharedSelfFactory<Track> {
		public:
			static bool PreCreateHook([[maybe_unused]] std::string name) {
				return true;
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			std::expected<void, CoriError<>> SetSound(const std::shared_ptr<Sound>& sound) {
				if (m_Valid) {
					if (!m_ActiveSequence) {
						if (sound->IsValid()) {
							CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Assigning Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'", sound->m_Name, sound->m_ID, m_Name, m_ID);
							if (sound->IsPlaceholder()) {
								CORI_CORE_WARN_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "A Sound '{} (SoundID: {})' to be assigned to Track '{} (TrackID: {})' is a placeholder, likely failed to load the original sound.", sound->m_Name, sound->m_ID, m_Name, m_ID);
							}

							return Mixer::SetTrackSound(m_ID, sound.get());
						}

						return std::unexpected(CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. Sound object is invalid.", sound->m_Name, sound->m_ID, m_Name, m_ID)));
					}

					return std::unexpected(CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. A sequence is currently playing on this track. Can't assign Sound when a sequence is playing.", sound->m_Name, sound->m_ID, m_Name, m_ID)));

				}


				return std::unexpected(CoriError(std::format("Failed to assign Sound '{} (SoundID: {})' to Track '{} (TrackID: {})'. Track object is invalid.", sound->m_Name, sound->m_ID, m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			std::expected<void, CoriError<>> Play(const PlayParams& params = PlayParams{}) {
				if (m_Valid) {
					if (!m_ActiveSequence) {
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Playing Track '{} (TrackID: {})'", m_Name, m_ID);
						return Mixer::PlayTrack(m_ID, params);
					}

					return std::unexpected(CoriError(std::format("Failed to play Track '{} (TrackID: {})'. A sequence is currently playing on this track. Can't play Sound when a sequence is playing.", m_Name, m_ID)));
				}

				return std::unexpected(CoriError(std::format("Failed to play Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}

			std::expected<void, CoriError<>> PlaySoundWithParams(SoundWithParams& object) {
				return SetSound(object.first).and_then([this, object] {
					return Play(object.second);
				});
			}

			std::expected<void, CoriError<>> StartSequence(const IsSoundWithParams auto&... sequence) {
				static_assert(sizeof...(sequence) > 2, "Sequence should contain at least 2 SoundWithParams objects.");
				if (m_Valid) {
					if (!m_ActiveSequence) {
						m_SoundSequence.clear();
						m_SoundSequence.reserve(sizeof...(sequence));
						(..., m_SoundSequence.push_back(sequence));
						m_EraseLastInSequence = false;
						m_SequenceIntroFinished = false;
						if (!m_CurrentTag.empty()) {
							RemoveTag(m_CurrentTag.c_str(), true);
						}
						m_CurrentLoopedSequenceIndex = 0;

						auto& initialPart = m_SoundSequence.back();
						const auto success = PlaySoundWithParams(initialPart);
						if (!success) {
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "An error occurred when trying to play a part of sequence. Details: {}", success.error().what());
						}
						m_ActiveSequence = true;

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
									const auto success = PlaySoundWithParams(*part);
									if (!success) {
										CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "An error occurred when trying to play a part of sequence. Details: {}", success.error().what());
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
								StopSequence(false);
							}
						});
						return {};
					}

					return std::unexpected(CoriError(std::format("Failed to play sequence on Track '{} (TrackID: {})'. A sequence is already playing on this track.", m_Name, m_ID)));
				}

				return std::unexpected(CoriError(std::format("Failed to play sequence on Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}

			// maybe add an ability to play outro seqeunce
			// fadeoutms are applide only when abruptStop is true!
			std::expected<void, CoriError<>> StopSequence(const bool abruptStop, const int64_t fadeOutMS = 0) {
				if (m_Valid) {
					if (m_ActiveSequence) {
						SetTrackStopCallbackInternal([this] {
							m_ActiveSequence = false;
							if (!m_CurrentTag.empty()) {
								const auto result = SetTag(m_CurrentTag.c_str());
								if (!result) {
									CORI_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Failed to reassign a tag '{}' after playing a sequence. Error: {}", m_CurrentTag, result.error().what());
								}
							}
						});
						if (abruptStop) {
							m_ActiveSequence = false;
							return Stop(fadeOutMS);
						}
					}

					return std::unexpected(CoriError(std::format("Failed to stop sequence on Track '{} (TrackID: {})'. No sequence is currently playing on this track.", m_Name, m_ID)));
				}

				return std::unexpected(CoriError(std::format("Failed to stop sequence on Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}


			// ReSharper disable once CppMemberFunctionMayBeConst
			std::expected<void, CoriError<>> Stop(const int64_t fadeOutMS) {
				if (m_Valid) {
					if (!m_ActiveSequence) {
						CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Stopping Track '{} (TrackID: {})'", m_Name, m_ID);
						return Mixer::StopTrack(m_ID, fadeOutMS);
					}

					return std::unexpected(CoriError(std::format("Failed to stop Track '{} (TrackID: {})'. A sequence is currently playing on this track. Can't stop when a sequence is playing.", m_Name, m_ID)));
				}

				return std::unexpected(CoriError(std::format("Failed to stop Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			std::expected<void, CoriError<>> Pause() {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Pausing Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::PauseTrack(m_ID);
				}

				return std::unexpected(CoriError(std::format("Failed to pause Track '{} (TrackID: {})'. Track object is invalid.", m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			std::expected<void, CoriError<>> Resume() {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Resuming Track '{} (TrackID: {})'", m_Name, m_ID);
					return Mixer::ResumeTrack(m_ID);
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
			std::expected<void, CoriError<>> SetGain(const float gain) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Setting Track '{} (TrackID: {})' gain to '{}'", m_Name, m_ID, gain);
					return Mixer::SetTrackGain(m_ID, gain);
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
			std::expected<void, CoriError<>> SetTag(const char* tag) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Assigning Tag '{}' to Track '{} (TrackID: {})'", tag, m_Name, m_ID);
					m_CurrentTag = std::string(tag);
					return Mixer::TagTrack(m_ID, tag);
				}

				return std::unexpected(CoriError(std::format("Failed to assign Tag '{}' to Track '{} (TrackID: {})'. Track object is invalid.", tag, m_Name, m_ID)));
			}

			// ReSharper disable once CppMemberFunctionMayBeConst
			void RemoveTag(const char* tag, const bool preserveCachedTag = false) {
				if (m_Valid) {
					CORI_CORE_TRACE_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Track }, "Removing Tag '{}' from Track '{} (TrackID: {})'", tag, m_Name, m_ID);
					Mixer::UntagTrack(m_ID, tag);
					if (!preserveCachedTag) {
						m_CurrentTag.clear();
						m_CurrentTag.shrink_to_fit();
					}
				}
			}

			const char* GetTag() const {
				if (m_Valid) {
					return m_CurrentTag.c_str();
				}

				return "";
			}

			[[nodiscard]] bool IsValid() const {
				return m_Valid;
			}

			void SetTrackStopCallback(TrackStopCallbackFn callback) {
				m_ClientCallBack = std::move(callback);
			}


			const std::string m_Name;
			const TrackID m_ID{ 0 };

			static void TrackStopCallback(void* userdata, MIX_Track* track);

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

			TrackStopCallbackFn m_EngineCallBack;
			TrackStopCallbackFn m_ClientCallBack;

		private:

			std::expected<void, CoriError<>> ProcessSequencePart(const SoundWithParams& part) {
				m_SoundSequence.push_back(part);

				return {};
			}

			void SetTrackStopCallbackInternal(TrackStopCallbackFn callback) {
				m_EngineCallBack = std::move(callback);
			}

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