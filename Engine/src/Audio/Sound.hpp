#pragma once
#include "Profiling/Trackable.hpp"
#include "Mixer.hpp"
#include "Utility/PathDefines.hpp"

namespace Cori {
	namespace Audio {

		class Sound : public Profiling::Trackable<Sound> {
		public:
			class Descriptor {
			public:
				constexpr Descriptor(std::string name, std::filesystem::path path, const bool preDecode = true) noexcept
					: m_Path(std::move(path)),
					m_PreDecode(preDecode),
					m_Name(std::move(name)),
					m_RuntimeID(s_NextRuntimeID.fetch_add(1, std::memory_order_relaxed))
				{ }

				using AssetType = Sound;

				[[nodiscard]] uint32_t GetRuntimeID() const { return m_RuntimeID; }

				constexpr bool operator==(const Descriptor& other) const noexcept {
					return m_RuntimeID == other.m_RuntimeID;
				}

				struct Hasher {
					std::size_t operator()(const Descriptor& handle) const noexcept {
						return std::hash<uint32_t>{}(handle.m_RuntimeID);
					}
				};

				const std::filesystem::path m_Path;
				const bool m_PreDecode;
				const std::string m_Name;

			private:
				const uint32_t m_RuntimeID{ 0 };
				inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
			};

			~Sound() {
				Mixer::UnloadSound(m_ID);
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Sound '{} (SoundID: {})' destroyed." , m_Name, m_ID);
			}

			[[nodiscard]] bool IsValid() const {
				return m_Valid;
			}

			[[nodiscard]] bool IsPlaceholder() const {
				return m_Placeholder;
			}

			std::string m_Name;
			const SoundID m_ID{ 0 };

			static std::shared_ptr<Sound> Create(const std::string& name, const std::filesystem::path& path, const bool preDecode = true) {
				return std::shared_ptr<Sound>(new Sound(name, path, preDecode));
			}

			static std::shared_ptr<Sound> Create(const Descriptor& descriptor) {
				return Create(descriptor.m_Name, descriptor.m_Path, descriptor.m_PreDecode);
			}

		private:
			Sound(const std::string& name, const std::filesystem::path& path, const bool preDecode) : m_Name(std::move(name)), m_ID(s_NextIndex.fetch_add(1, std::memory_order_relaxed)) {
				CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Creating Sound '{} (SoundID: {})' from: '{}'", m_Name, m_ID, path.string());
				if (std::filesystem::exists(path)) {
					auto result = Mixer::LoadSound(path, preDecode, m_ID);
					if (result) {
						m_Valid = true;
						CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Sound '{} (SoundID: {})' was created from: '{}'", m_Name, m_ID, path.string());
					} else {
						CORI_CORE_ERROR_TAGGED({ Logger::Tags::Audio::Self, Logger::Tags::Audio::Sound }, "Failed to create Sound '{} (SoundID: {})'. Error: {}. Trying to load a placeholder.", m_Name, m_ID, result.error().what());

						auto result_ = Mixer::LoadSound(Utility::Internal::PathDefines::PlaceholderSound, preDecode, m_ID);
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
					auto result_ = Mixer::LoadSound(Utility::Internal::PathDefines::PlaceholderSound, preDecode, m_ID);
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

			bool m_Valid{ false };
			bool m_Placeholder{ false };
			inline static std::atomic<SoundID> s_NextIndex{ 1 };
		};
	}
}
