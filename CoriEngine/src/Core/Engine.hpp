#pragma once

namespace Cori {

	class Engine {
	public:
		static void Start(bool asyncLogging);
		static void Stop();
	};
}