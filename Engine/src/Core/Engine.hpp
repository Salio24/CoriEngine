#pragma once

namespace Cori {
	namespace Core {
		class Engine {
		public:

			/**
			 * @brief Triggers the very first engine initialization step. This is only for internal use.
			 * @param asyncLogging Enable or disable asynch logging.
			 * @param fileLogging Enable or disable logging to a file.
			 */
			static void Start(const bool asyncLogging, const bool fileLogging);

			/**
			 * @brief Stops the engine. This is only for internal use.
			 */
			static void Stop();
		};
	}
}