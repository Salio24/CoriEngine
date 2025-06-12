# Cori Engine (WIP)

![Cori](/CoriEngine/git_logo.png?raw=true "Cori")

Cori is my game engine that I’m actively working on. When building, I took some inspiration from Hazel Engine by The Cherno. The current feature set of the engine is: Basic 2D Batch Renderer, Asset Manager, Event System, Layer System, Input Handler, Logging, ImGui integration, Entity Component System, Scene System, API agnostic window/graphics/rendering abstraction.

***

## Getting Started

Cori Engine uses CMake as a build system. Currently, it supports only Windows.

<ins>**1. Necessary tools**</ins>
- CMake 3.28 or newer 
- Ninja build system
- Python 3.9 or newer
- LLVM toolkit with C++23 support
	- (Can be found here: `https://github.com/llvm/llvm-project`)
	- PS: Unfortinatly some thirdparty libararies only support POSIX compiler frontend and do not support MSVC frontend, and because of that some compiler flags are being ignored and not applied when compiling with MSVC frontended compiler.

<ins>**2. Downloading the repository and dependencies:**</ins>

1. Clone the repository with `git clone -b CoriStable https://github.com/Salio24/CoriEngine.git --recursive`

<ins>**3. Compiling:**</ins>

*Here you have two paths:*

### Using Visual Studio 2022 CMake integration (the preferred way, i use it):

1. Install CMake tools and the clang-cl toolset for Visual Studio in the Visual Studio Installer.
2. Replace `CMakePresets.json` file in the root folder with the file `CMakeSettings.json`, it is located in the platform folder.
	- This step is optional in a way, it will compile with `CMakePresets.json`, but intellisense can behave very weirdly if using `CMakePresets.json` instead of `CMakeSettings.json`.
3. Open the root folder as a local folder in Visual Studio.
4. Done.

Make sure to check compiler paths in `cmake_toolchain.cmake`. It should point to your LLVM installation folders. Although it points to the default installation folder of LLVM, if you changed the install folder, you'll have to specify the correct path in the toolchain file.

### Using plain cmake

- There Are 3 build scripts in the root folder:
	- `build_x64-clang-debug.sh`
	- `build_x64-clang-release.sh`
	- `build_x64-clang-relwithdebinfo.sh`
- Each will build an engine as a static libarary, and all the available apps. 
- Built binaries are located in: 
	- `CoriEngine/bin/PresetName`
	- `Apps/RendererSandbox/bin/PresetName`
	- `Apps/Sandbox/bin/PresetName`

Make sure to check compiler paths in `cmake_toolchain.cmake`. It should point to your LLVM installation folders. Although it points to the default installation folder of LLVM, if you changed the install folder, you'll have to specify the correct path in the toolchain file.

### Visual Studio Solution IS NOT SUPPORTED!

***

## More detailed feature description:

### 2D Batch Renderer:

The 2D Batch Renderer can render flat-color quads, and textured quads. All user needs to do is:
1. Call `BeginBatch`
2. Call the specific appropriate `DrawQuad` function, with or without a texture.
3. Call `EndBatch`
The renderer will handle everything else. 

### Asset Manager:

Asset Manager uses asset descriptor object. For we create a shader descriptor, passing file paths and shader debug name in the descriptors constructor. Then, when we need the shader, we call `GetShader` and pass the descriptor instance as an argument. The Asset Manager will load the asset upon the first time it is requested. We can also preload an asset with the appropriate `Preload` function. The same functionality also applies to textures.

### Event Manager: 

The Event Manager can dispatch events to a user-defined function, this function should return a bool. when a function that is responsible for handling a specific event returns true, the event is considered handled and will not be passed to lower layers in the Layer Stack. The Layer System is tied to the Event System. When an event occurs, it is sent the `OnEvent` functions of the layers, starting from the top of the Layer Stack and proceeding downwards. It is passed to lower layers until it is handled by a layer, or until it reaches the bottom of the Layer Stack.

### Input Handler:

We can ask the Input Handler, whether a specific key is pressed (held), and we can ask it for the current mouse position. Event Manager also creates `KeyPressed` and `KeyReleased` events, but they serve a different purpose. 

### Logging and ImGui:

Logging and ImGui helps a lot with debugging. The Logger logs everything to the console and also into the log file in debug mode, and in release mode, it logs only into the log file.

### Entity Component System and Scene System

A 2 essential systems for every game and game engine. A flexible game structure is impossible without those 2 systems. ECS is a must have to achieve good performance as it grants that entity components of the same type are stored contiguously in memory, minimizing CPU cache misses. I'm using EnTT on the backend.
	
### Abstraction:

Window/graphics/rendering are all abstracted, user code has no direct calls to OpenGL and other libs, the only exceptions being ImGui and GLM. I’m planning to start experimenting with Vulkan in the future, so having a good abstraction will be very handy. 

***

## Plans for the future

- Make ImGui window templates. For example, instead of the user having to write a custom window to show a frame graph, we just call something like this: 'CoriImGuiPerformanceMetrics();'
- Box2D physics engine integration. (in process)
- Improving development tools, adding reference counting and memory tracking to the current debugging toolset.
- I also want to start experimenting with Vulkan.
	- I’m planning to make Vulkan the main graphics API for Cori.
- Separating engine execution into different threads.
	- Creating a render command que, and a separate render thread.
- Also, in the near future, I want to implement some kind of Entity Component System. (done)
- Box2D physics engine integration. (in process)
- Creating an engine editor.

## Currently available Apps

- `RendererSandbox` - this is the benchmark for 2D Batch Renderer.
- `Sandbox` - As the name implies, it is used mainly for testing purposes.
