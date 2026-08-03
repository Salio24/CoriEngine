//#define CORI_ASYNC_LOGGING
//#define CORI_NO_FILE_LOGGING
#include <Cori.hpp>
#include <CoriEntry.hpp>

class EditorLayer : public Cori::Core::Layer {
public:
	EditorLayer() : Layer("Example") {

	}

	~EditorLayer() {

	}

	void OnEvent(Cori::Core::Event& event) override {

	}

	void OnImGuiRender(Cori::Core::GameTimer& gameTimer) override {

	}

	void OnUpdate(Cori::Core::GameTimer& gameTimer) override {

	}

	void OnTickUpdate(Cori::Core::GameTimer& gameTimer) override {


	}

private:

};

class Snowflake : public Cori::Core::Application {
public:
	Snowflake(): Application("Snowflake Editor") {
		PushLayer(new EditorLayer());

		CORI_INFO("Snowflake editor application created");
	}

	~Snowflake() {
		CORI_INFO("Snowflake editor application destroyed");
	}
};

Cori::Core::Application* Cori::Core::CreateApplication() {
	return new Snowflake();
}