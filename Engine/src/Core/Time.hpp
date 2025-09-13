#pragma once

namespace Cori {
	namespace Core {
		class GameTimer {
		public:
			GameTimer();
			~GameTimer() = default;

			void SetTickrate(const uint16_t tickrate);

			double GetDeltaTime() const { return m_DeltaTime; }
			double GetTickAlpha() const { return m_TickAlpha; }
			float GetTimestep() const { return m_Timestep; }

			double GetMilliseconds() const { return m_Time * 1000.0f; }
			double GetSeconds() const { return m_Time; }
			double GetMinutes() const { return m_Time / 60.0f; }
			double GetHours() const { return m_Time / 3600.0f; }


		private:
			friend class Application;
			void SetTickrateUpdateFunc(const std::function<void(GameTimer&)>& func) { m_TickrateUpdateFunc = func; }
			void Update();

			double m_DeltaTime{ 0 };
			double m_TickAlpha{ 0 };

			float m_Timestep{ 0 };
			double m_Accumulator{ 0 };

			// time in seconds since start
			double m_Time{ 0 };

			uint64_t m_LastTime{ 0 };

			uint16_t m_Tickrate{ 0 };

			std::function<void(GameTimer&)> m_TickrateUpdateFunc{ nullptr };
		};

		class ManualTimer {
		public:
			ManualTimer() = default;
			~ManualTimer() = default;

			void Start();
			double End() const;
		private:
			uint64_t m_Start{ 0 };
		};
	}
}