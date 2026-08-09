#include "Layer.hpp"
#include "WorldSystem/SceneManager.hpp"


namespace Cori {
	namespace Core {
		Layer::Layer(std::string name) : m_Name(std::move(name)) {
			if (m_Name.empty()) {
				CORI_CORE_ERROR_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Creating a Layer with an empty name. This WILL cause issues.");
			}
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' created.", m_Name);
		}

		Layer::~Layer() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' destroyed.", m_Name);
		}

		void Layer::OnAttach() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' attached.", m_Name);
		}

		void Layer::OnDetach() {
			CORI_CORE_INFO_TAGGED({ Logger::Tags::Core::Self, Logger::Tags::Core::Layer }, "Layer '{}' detached.", m_Name);
		}
	}
}