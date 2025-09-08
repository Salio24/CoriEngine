#pragma once

namespace Cori {
	namespace Core {
		class Engine {
		public:
			static void Start(const bool asyncLogging, const bool fileLogging);
			static void Stop();
		};
	}
}