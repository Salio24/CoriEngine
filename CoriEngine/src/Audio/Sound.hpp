#pragma once
#include "Profiling/Trackable.hpp"
#include "Core/SelfFactory.hpp"
#include "Mixer.hpp"

namespace Cori {
	namespace Audio {
		class Sound : public Profiling::Trackable<Sound>, public SharedSelfFactory<Sound> {
		public:
			static bool PreCreateHook([[maybe_unused]] std::string name, [[maybe_unused]] const std::filesystem::path& path, [[maybe_unused]] const bool preDecode = true) {
				return true;
			}

		protected:
			Sound(std::string name, const std::filesystem::path& path, const bool preDecode = true) : m_Index(s_NextIndex.fetch_add(1, std::memory_order_relaxed)), m_Name(std::move(name)), m_Filename(path.filename().c_str()) {
				Mixer::LoadSound(path, preDecode, m_Index);
			}

			~Sound() {
				Mixer::UnloadSound(m_Index);
			}

			friend class Track;
			const uint32_t m_Index{ 0 };

		private:
			std::string m_Name;
			const char* m_Filename;
			inline static std::atomic<uint32_t> s_NextIndex{ 1 };
		};
	}
}
