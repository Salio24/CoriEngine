#pragma once

namespace Cori {
	namespace Core {
		/**
		 * @brief A GameTimer is responsible for managing everything that is connected with time, ticks, elapsed time since start, deltaTime. There can only be one GameTimer.
		 */
		class GameTimer {
		public:
			~GameTimer() = default;

			/**
			 * @brief Changes the tickrate.
			 * @param tickrate Tickrate to set.
			 */
			void SetTickrate(const uint16_t tickrate);

			/**
			 * @brief Returns the deltaTime, scale is in seconds.
			 */
			[[nodiscard]] double GetDeltaTime() const { return m_DeltaTime; }

			/**
			 * @brief Returns the tickAlpha, scale is in seconds. Used for between tick interpolation.
			 */
			[[nodiscard]] double GetTickAlpha() const { return m_TickAlpha; }

			/**
			 * @brief Returns the current timeStep, scale is in seconds.
			 */
			[[nodiscard]] float GetTimestep() const { return m_Timestep; }

			/**
			 * @brief Gets the time in milliseconds since application start.
			 * @return Milliseconds elapsed.
			 */
			[[nodiscard]] double GetElapsedMilliseconds() const { return m_Time * 1000.0f; }

			/**
			 * @brief Gets the time in seconds since application start.
			 * @return Seconds elapsed.
			 */
			[[nodiscard]] double GetElapsedSeconds() const { return m_Time; }

			/**
			 * @brief Gets the time in minutes since application start.
			 * @return Minutes elapsed.
			 */
			[[nodiscard]] double GetElapsedMinutes() const { return m_Time / 60.0f; }

			/**
			 * @brief Gets the time in hours since application start.
			 * @return Hours elapsed.
			 */
			[[nodiscard]] double GetElapsedHours() const { return m_Time / 3600.0f; }

			/**
			 * @brief Formats milliseconds into a string with a format Min:Sec:Ms
			 * @param milliseconds Value to convert.
			 * @return Formated string.
			 */
			[[nodiscard]] static std::string FormatTime_MS_to_M_S_MS(const double milliseconds);

			/**
			 * @brief Formats seconds into a string with a format Min:Sec:Ms
			 * @param seconds Value to convert.
			 * @return Formated string.
			 */
			[[nodiscard]] static std::string FormatTime_S_to_M_S_MS(const double seconds);

			/**
			 * @brief Enables or disables manual tick step.
			 * @param state On or off state.
			 * @details When enabled ticks don't happened on their own, instead you can advance one tick at a time by pressing K, or hold J to enable regular behaviour when enabled.
			 * @note When using manual step interpolation might look wierd, since m_TickAlpha will be close to 0 all the time.
			 * \n This is expected behaviour since it relies on per frame logic, but what we do when using manual step is we advance one tick at a time including running the per frame logic not each frame.
			 * Interpolated position would look like similar to the case if we would run the game at near tickrate FPS.
			 */
			void SetManualTickStep(const bool state);

		private:
			friend class Application;
			GameTimer();
			void SetTickrateUpdateFunc(const std::function<void(GameTimer&)>& func) { m_TickrateUpdateFunc = func; }
			void Update();

			double m_DeltaTime{ 0 };
			double m_TickAlpha{ 0 };

			float m_Timestep{ 0 };
			double m_Accumulator{ 0 };

			// time in seconds since start
			double m_Time{ 0 };

			bool m_ManualStep{ false };
			bool m_ManualTickGate{ false };

			uint64_t m_LastTime{ 0 };

			uint16_t m_Tickrate{ 0 };

			std::function<void(GameTimer&)> m_TickrateUpdateFunc{ nullptr };
		};

		/**
		 * @brief You can use this to manually time something.
		 */
		class ManualTimer {
		public:
			ManualTimer() = default;
			~ManualTimer() = default;

			/**
			 * @brief Start the manual timer.
			 */
			void Start();

			/**
			 * @brief Stops the manual timer.
			 * @return Elapsed time in milliseconds.
			 */
			[[nodiscard]] double End() const;
		private:
			uint64_t m_Start{ 0 };
		};
	}
}