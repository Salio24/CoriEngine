#pragma once

namespace Cori {

	class Engine {
	public:
		static void Start(bool asyncLogging, bool fileLogging);
		static void Stop();
	};
}