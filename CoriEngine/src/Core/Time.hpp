#pragma once

namespace Cori {
	class GameTimer {
	public:
		GameTimer();

		void Update();
		void SetTickrate(uint16_t tickrate);

		double GetDeltaTime() const { return m_DeltaTime; }
		double GetTickAlpha() const { return m_TickAlpha; }

		double GetMilliseconds() const { return m_Time * 1000.0f; }
		double GetSeconds() const { return m_Time; }
		double GetMinutes() const { return m_Time / 60.0f; }
		double GetHours() const { return m_Time / 3600.0f; }

		void SetTickrateUpdateFunc(const std::function<void(const float)>& func) { m_TickrateUpdateFunc = func; }

	private:
		double m_DeltaTime{ 0 };
		double m_TickAlpha{ 0 };

		float m_Timestep{ 0 };
		double m_Accumulator{ 0 };

		// time in seconds since start
		double m_Time{ 0 };

		uint64_t m_LastTime{ 0 };

		uint16_t m_Tickrate{ 0 };

		std::function<void(const float)> m_TickrateUpdateFunc{ nullptr };
	};

	class ManualTimer {
	public:
		ManualTimer() = default;
		~ManualTimer() = default;

		void Start();
		double End();
	private:
		uint64_t m_Start{ 0 };

	};
}