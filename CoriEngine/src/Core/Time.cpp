#include "Time.hpp"
#include <SDL3/SDL_timer.h>

namespace Cori {
	GameTimer::GameTimer() {
		CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::GameTimer }, "GameTimer created");
		m_LastTime = SDL_GetPerformanceCounter();
	}

	void GameTimer::Update() {
		CORI_PROFILE_FUNCTION();
		const uint64_t now = SDL_GetPerformanceCounter();
		m_DeltaTime = static_cast<double>(now - m_LastTime) / SDL_GetPerformanceFrequency();
		m_LastTime = now;

		m_Time += m_DeltaTime;

		m_Accumulator += m_DeltaTime;

		if (m_Timestep != 0) {
			while (m_Accumulator >= m_Timestep) {

				m_TickrateUpdateFunc(m_Timestep);

				m_Accumulator -= m_Timestep;
			}

			m_TickAlpha = m_Accumulator / m_Timestep;

		}
	}

	void GameTimer::SetTickrate(const uint16_t tickrate) {
		m_Tickrate = tickrate;
		m_Timestep = 1.0f / static_cast<float>(tickrate);
		CORI_CORE_DEBUG_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::GameTimer }, "Tickrate set to '{}', timestep '{}'", tickrate, m_Timestep);
	}

	void ManualTimer::Start() {
		m_Start = SDL_GetPerformanceCounter();
	}

	double ManualTimer::End() const {
		const uint64_t end = SDL_GetPerformanceCounter();
		return static_cast<double>(end - m_Start) * 1000.0f / SDL_GetPerformanceFrequency();
	}

}