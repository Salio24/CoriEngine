//#define CORI_ASYNC_LOGGING
//#define CORI_NO_FILE_LOGGING
#include <Cori.hpp>
#include <CoriEntry.hpp>
#include "EditorLayer.hpp"
#include "Core/Logger.hpp"

namespace Snowflake {
	class Editor final : public Cori::Core::Application {
	public:
		Editor() : Application("Snowflake Editor") {
			const auto result = PushLayer(new EditorLayer());
			CORI_ASSERT(result, "Failed to push the editor layer. Error: {}", result.error().what());

			CORI_INFO("Snowflake editor application created");
		}

		~Editor() override {
			CORI_INFO("Snowflake editor application destroyed");
		}
	};
}

Cori::Core::Application* Cori::Core::CreateApplication() {
	return new Snowflake::Editor();
}
