#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <tmxlite/Map.hpp>

int main() {

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
	}

	tmx::Map map;

	std::cout << "Hello, World1!" << std::endl;
	std::cin.get();
	return 0;
}