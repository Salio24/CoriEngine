#pragma once
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"
#include "Mixer.hpp"
#include "Core/Logger.hpp"

namespace Cori {
	namespace Audio {

		class Sound : public Profiling::Trackable<Sound>, public SharedSelfFactory<Sound> {
		public:
			static bool PreCreateHook([[maybe_unused]] std::string name, [[maybe_unused]] const std::filesystem::path& path, [[maybe_unused]] const bool preDecode = true) {
				return true;
			}

			bool IsValid() const {
				return m_Valid;
			}

			bool IsPlaceholder() const {
				return m_Placeholder;
			}

			std::string m_Name;
			const SoundID m_ID{ 0 };

		protected:
			Sound(std::string name, const std::filesystem::path& path, const bool preDecode = true) : m_Name(std::move(name)), m_ID(s_NextIndex.fetch_add(1, std::memory_order_relaxed)) {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Creating Sound '{} (SoundID: {})' from: '{}'", m_Name, m_ID, path.string());
				if (std::filesystem::exists(path)) {
					auto result = Mixer::LoadSound(path, preDecode, m_ID);
					if (result) {
						m_Valid = true;
						CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Sound '{} (SoundID: {})' was created from: '{}'", m_Name, m_ID, path.string());
					} else {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Failed to create Sound '{} (SoundID: {})'. Error: {}. Trying to load a placeholder.", m_Name, m_ID, result.error().what());

						auto result_ = Mixer::LoadSound("assets/engine/sounds/placeholder.ogg", preDecode, m_ID);
						if (result_) {
							m_Valid = true;
							m_Placeholder = true;
							CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Placeholder loaded for Sound '{} (SoundID: {})'",m_Name, m_ID);
						} else {
							m_Valid = false;
							CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Failed to load the placeholder for Sound '{} (SoundID: {})'. Error: {}. Invalid Sound object was created as a result, this should not crash as the engine prevents you from using an invalid Sound object.", m_Name, m_ID, result_.error().what());
						}
					}
				} else {
					CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Failed to create a Sound '{} (SoundID: {})' from: '{}', specified path does not exist. Trying to load a placeholder.", m_Name, m_ID, path.string());
					auto result_ = Mixer::LoadSound("assets/engine/sounds/placeholder.ogg", preDecode, m_ID);
					if (result_) {
						m_Valid = true;
						m_Placeholder = true;
						CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Placeholder loaded for Sound '{} (SoundID: {})'", m_Name, m_ID);
					} else {
						m_Valid = false;
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Failed to load the placeholder for Sound '{} (SoundID: {})'. Error: {}. Invalid Sound object was created as a result, this should not crash as the engine prevents you from using an invalid Sound object.", m_Name, m_ID, result_.error().what());
					}
				}
			}

			~Sound() {
				Mixer::UnloadSound(m_ID);
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Sound '{} (SoundID: {})' destroyed." , m_Name, m_ID);
			}

		private:
			bool m_Valid{ false };
			bool m_Placeholder{ false };
			inline static std::atomic<SoundID> s_NextIndex{ 1 };
		};
	}
}
