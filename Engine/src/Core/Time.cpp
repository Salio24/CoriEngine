#include "Time.hpp"
#include <SDL3/SDL_timer.h>
#include "Input.hpp"

namespace Cori {
	namespace Core {
		GameTimer::GameTimer() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::GameTimer }, "GameTimer created");
			m_LastTime = SDL_GetPerformanceCounter();
		}

		std::string GameTimer::FormatTime_MS_to_M_S_MS(const double milliseconds) {
			auto minutes = static_cast<int32_t>(milliseconds / 60000);
			auto seconds = static_cast<int32_t>((milliseconds - minutes * 60000) / 1000);
			auto ms      = static_cast<int32_t>(milliseconds) % 1000;

			return std::format("{}:{:02}:{:03}", minutes, seconds, ms);
		}

		std::string GameTimer::FormatTime_S_to_M_S_MS(const double seconds) {
			return FormatTime_MS_to_M_S_MS(seconds * 1000.0);
		}

		void GameTimer::SetManualTickStep(const bool state) {
			m_ManualStep = state;
		}

		void GameTimer::Update() {
			CORI_PROFILE_FUNCTION();
			const uint64_t now = SDL_GetPerformanceCounter();
			m_DeltaTime = static_cast<double>(now - m_LastTime) / SDL_GetPerformanceFrequency();
			m_LastTime = now;

			m_Time += m_DeltaTime;

			if (!m_ManualStep) {
				m_Accumulator += m_DeltaTime;

				if (m_Timestep != 0) {
					while (m_Accumulator >= m_Timestep) {

						m_TickrateUpdateFunc(*this);

						m_Accumulator -= m_Timestep;
					}

					m_TickAlpha = m_Accumulator / m_Timestep;
				}
			} else {
				static bool oneshot = true;
				if (Input::IsKeyDown(CORI_KEY_K)) {
					if (oneshot) {
						oneshot = false;
						m_ManualTickGate = true;
					}
				} else {
					oneshot = true;
				}

				if (Input::IsKeyDown(CORI_KEY_J)) {
					m_ManualTickGate = true;
				}

				if (m_ManualTickGate) {
					m_Accumulator += m_DeltaTime;

					if (m_Timestep != 0) {
						while (m_Accumulator >= m_Timestep) {

							m_TickrateUpdateFunc(*this);
							m_ManualTickGate = false;

							m_Accumulator -= m_Timestep;
						}

						m_TickAlpha = m_Accumulator / m_Timestep;
					}
				}
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
}