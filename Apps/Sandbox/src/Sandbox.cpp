#include <iostream>
#include <Cori.hpp>

class Sandbox : public Cori::Application {
public:
	Sandbox() {
		CORI_INFO("Sandbox application created");
	}
	~Sandbox() {
		CORI_INFO("Sandbox application destroyed");
	}
};

Cori::Application* Cori::CreateApplication() {
	return new Sandbox();
}