#pragma once
#include "Profiling/Trackable.hpp"
#include "Mixer.hpp"

namespace Cori {
	class AssetManager;
	namespace Audio {

		class Sound : public Profiling::Trackable<Sound> {
		public:
			class Descriptor {
			public:
				constexpr Descriptor(std::string name, std::filesystem::path path, const bool preDecode = true) noexcept
					: m_Path(std::move(path)),
					m_Name(std::move(name)),
					m_PreDecode(preDecode),
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
				const std::string m_Name;
				const bool m_PreDecode;

			private:
				const uint32_t m_RuntimeID{ 0 };
				inline static std::atomic<uint32_t> s_NextRuntimeID{ 1 };
			};

			~Sound();

			/**
			 * @brief Check if the Sound is valid.
			 * @return Validity state.
			 * @note Generally there is no need to explicitly check for sound validity, because Track already does so.
			 */
			[[nodiscard]] bool IsValid() const;

			/**
			 * @brief Checks if the sound was created with a placeholder.
			 * @return Placeholder state.
			 */
			[[nodiscard]] bool IsPlaceholder() const;

			/**
			 * @brief Returns the SoundID associated with Sound.
			 */
			[[nodiscard]] SoundID GetID() const;

			const std::string m_Name;

			/**
			 * @brief Creates a Sound object.
			 * @param name Name to be assigned to the Sound.
			 * @param path Path to the audio asset.
			 * @param preDecode Whether to precede the audio or no. Generally you want to leave it at default.
			 * @return Shared pointer to the loaded Sound asset.
			 */
			[[nodiscard]] static std::shared_ptr<Sound> Create(const std::string& name, const std::filesystem::path& path, const bool preDecode = true);
		private:
			friend AssetManager;
			[[nodiscard]] static std::shared_ptr<Sound> Create(const Descriptor& descriptor);
			Sound(std::string name, const std::filesystem::path& path, const bool preDecode);

			bool m_Valid{ false };
			bool m_Placeholder{ false };
			const SoundID m_ID{ 0 };
			inline static std::atomic<SoundID> s_NextIndex{ 1 };
		};
	}
}
