#pragma once

namespace Cori {
	class GameTimer {
	public:
		GameTimer();

		void Update();
		void SetTickrate(uint16_t tickrate);

		// method GetDeltaTime can be used to retrieve deltatime 
		// also casting this class instance to double will return the deltatime
		double GetDeltaTime() { return m_DeltaTime; };
		operator double() const { return m_DeltaTime; }

		inline double GetMiliseconds() const;
		inline double GetSeconds() const;
		inline double GetMinutes() const;
		inline double GetHours() const;

		inline void SetTickrateUpdateFunc(const std::function<void(const float)>& func) { m_TickrateUpdateFunc = func; }

	private:
		double m_DeltaTime{ 0 };

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