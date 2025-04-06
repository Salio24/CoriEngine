#include <iostream>
#include <Cori.hpp>

class Sandbox : public Cori::Application {
public:
	Sandbox() {
		std::cout << "Sandbox constructor called" << std::endl;
	}
	~Sandbox() {
		std::cout << "Sandbox destructor called" << std::endl;
	}
};

Cori::Application* Cori::CreateApplication() {
	std::cout << "Creating Sandbox application" << std::endl;
	return new Sandbox();
}