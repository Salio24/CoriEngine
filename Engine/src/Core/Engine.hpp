#pragma once

namespace Cori {
	namespace Core {
		namespace Internal {
			class Engine {
			public:
				static void Start(const bool asyncLogging, const bool fileLogging);

				static void Stop();
			};
		}
	}
}