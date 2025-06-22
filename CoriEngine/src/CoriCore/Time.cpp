// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "Time.hpp"
#include <SDL3/SDL_timer.h>

namespace Cori {
	GameTimer::GameTimer() {
		CORI_CORE_DEBUG("GameTimer created");
		m_LastTime = SDL_GetPerformanceCounter();
	}

	void GameTimer::Update() {
		CORI_PROFILE_FUNCTION();
		uint64_t now = SDL_GetPerformanceCounter();
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

	void GameTimer::SetTickrate(uint16_t tickrate) {
		m_Tickrate = tickrate;
		m_Timestep = 1.0f / (float)tickrate;
	}

	double GameTimer::GetMiliseconds() const {
		return m_Time * 1000.0f;
	}

	double GameTimer::GetSeconds() const {
	}

	double GameTimer::GetMinutes() const {
		return m_Time / 60.0f;
	}

	double GameTimer::GetHours() const {
		return m_Time / 3600.0f;
	}

	void ManualTimer::Start() {
		m_Start = SDL_GetPerformanceCounter();
	}

	double ManualTimer::End() {
		uint64_t end = SDL_GetPerformanceCounter();
		return static_cast<double>(end - m_Start) * 1000.0f / SDL_GetPerformanceFrequency();
	}

}