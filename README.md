# Cori Engine (WIP)

![Cori](/CoriEngine/git_logo.png?raw=true "Cori")

Cori is my game engine that I’m actively working on. When building I took some inspiration from Hazel Engine by The Cherno. Current feature set of the engine is: Basic 2D Batch Renderer, Asset Manager, Event System, Layer System, Input handler, Logging, ImGui integration, API agnostic window/graphics/rendering abstraction.

***

## Install guide

Cori engine uses cmake as a build system. For now it supports only Windows. 

<ins>**1. Necessary tools**</ins>
- CMake 3.28 or newer 
- Ninja build system
- LLVM Clang compiler with C++23 support
	- Not Visual Stduio's clang-cl, that has MSVC frontend. You need LLVM Clang that comes with Posix frontend. (Can be found here: `https://github.com/llvm/llvm-project`)
		- You can also use other compilers with Posix frontend, but the project is configured to use LLVM. Thus using something other than LLVM will not work without tweaking toolchain file.

<ins>**2. Donwloading the repository and dependencies:**</ins>

1. Clone the repository with `git clone -b CoriStable https://github.com/Salio24/VoidScapeDev.git`
2. Download dependencies by running 'download_dependencies.bat' script in the root directory.

<ins>**3. Compiling:**</ins>

*Here you have 2 paths:*

### Using Visual Studio 2022 CMake integration (the prefered way, i use it):

1. Instal CMake tools and clang-cl toolset for Visal Studio in Visual Studio installer.
2. Copy file `CMakeSettings.json` from platform folder into the root folder.
3. Open the root folder as a local folder in Visual Studio.
4. Done.

Make sure to check compiler paths in `cmake_toolchain.cmake`, it should point to your LLVM instalation folders. Althought it point to default instalation folder of LLVM, if you changed the install folder, you'll have to specify the correct path in the toolchain file.

### Not using Visual Studio 2022

- Copy file `CMakePresets.json` from platform folder into the root folder.

From now everyting depends on your specific enviroment, for example, to use CMake from terminal:

1. List available presets with `cmake --list-presets`
2. Configure CMake preset with `cmake --preset PresetName`
3. Compile all targets with `cmake --build --preset PresetName`
4. Build binaries are located in: 
- `CoriEngine/bin/PresetName`
- `Apps/RendererSandbox/bin/PresetName`
- `Apps/Sandbox/bin/PresetName`
5. Done.

Make sure to check compiler paths in `cmake_toolchain.cmake`, it should point to your LLVM instalation folders. Althought it point to default instalation folder of LLVM, if you changed the install folder, you'll have to specify the correct path in the toolchain file.

***

## More detailed feature description:

### 2D Batch Renderer:

2D Batch Renderer can render flat color quads, and textured quads, all user need to do Is:
1. Call BeginBatch.
2. Call specific appropriate DrawQuad function, with or without texture.
3. Call EndBatch.
renderer will handle everything else. 

### Asset Manager:

Asset Manager uses asset descriptor object, we create shader descriptor for example, we pass file paths and shader debug name, then when we need our shader, we call GetShader and pass descriptor instance as an argument, Asset Manager will load asset upon first time it is requested. We can also preload asset with appropriate Preload function. The same functionality also applies to Textures.

### Event Manager: 

Event Manager can dispatch events to function that user is to define, function should return bool, when function that is responsible for handling a specific event returns true, event is considered handled and will not be passed to lower Layers. Layer System is tied to event system, when event occurs, it is being sent to layers OnEvent functions, starting from the top of layer stack to the bottom, it is pasted to lower layers until it is handled in some layer, or until it was passed to lowest layer in layer stack and there are no more layers to pass it to.

### Input handler:

We can ask Input handler, whether a specific key is pressed (held), and we can ask it for current mouse position. Event Manager also creates KeyPressed and KeyReleased events, but they serve a different purpose. 

### Logging and ImGui:

Logging and ImGui helps a lot with debugging. Logger logs everything into console and also into log file in debug mode, and in release mode in logs only into log file.
	
### Abstraction:

Window/graphics/rendering are all abstracted, user code has no calls to OpenGL and other libs, the exception being ImGui and glm. I’m planning to start playing with Vulkan in the future so having a good abstraction is will be very handy. 

***

## Plans for the future

- Make ImGui window templates. For example, instead of user having to write a custom window that will show a frame graph, we just call something like this: CoriImGuiPerformanceMetrics();
- By the end of May I’m planning to port my game into Cori engine.
- I also want to start playing with Vulkan.
	- I’m planning to make Vulkan a main graphics API for Cori.
- Also, in the near future I want to implement some kind of Game Entity component system.

