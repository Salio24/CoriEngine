#include "Track.hpp"
#include <SDL3_mixer/SDL_mixer.h>

namespace Cori {
	namespace Audio {
		void Track::TrackStopCallback(void* userdata, [[maybe_unused]] MIX_Track* track) {
			try {
				Track* coriTrack = static_cast<Track*>(userdata);
				CORI_CORE_DEBUG("Track '{}' (ID: {}) stopped.", coriTrack->m_Name, coriTrack->m_Index);
			} catch (const std::exception& e) {
				CORI_CORE_ERROR("{}", e.what());
			}

		}
	}
}