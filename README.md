# Cori Engine (WIP)

![Cori](/CoriEngine/git_logo.png?raw=true "Cori")

Cori is my game engine that I’m actively working on. When building, I took some inspiration from Hazel Engine by The Cherno. The current feature set of the engine is: Basic 2D Batch Renderer, Asset Manager, Event System, Layer System, Input Handler, Logging, ImGui integration, API agnostic window/graphics/rendering abstraction.

***

## Getting Started

Cori Engine uses CMake as a build system. Currently, it supports only Windows.

<ins>**1. Necessary tools**</ins>
- CMake 3.28 or newer 
- Ninja build system
- LLVM Clang compiler with C++23 support
	- Not Visual Studio's clang-cl, which has the MSVC frontend. You need LLVM Clang which comes with the Posix frontend. 
	- (Can be found here: `https://github.com/llvm/llvm-project`)
		- You can also use other compilers with a Posix frontend, but the project is configured to use LLVM. Thus using something other than LLVM will not work without tweaking the toolchain file.

<ins>**2. Downloading the repository and dependencies:**</ins>

1. Clone the repository with `git clone -b CoriStable https://github.com/Salio24/VoidScapeDev.git`
2. Download dependencies by running the 'download_dependencies.bat' script in the root directory.

<ins>**3. Compiling:**</ins>

*Here you have two paths:*

### Using Visual Studio 2022 CMake integration (the preferred way, i use it):

1. Install CMake tools and the clang-cl toolset for Visual Studio in the Visual Studio Installer.
2. Copy the file `CMakeSettings.json` from the platform folder into the root folder.
3. Open the root folder as a local folder in Visual Studio.
4. Done.

Make sure to check compiler paths in `cmake_toolchain.cmake`. It should point to your LLVM installation folders. Although it points to the default installation folder of LLVM, if you changed the install folder, you'll have to specify the correct path in the toolchain file.

### Not using Visual Studio 2022

- Copy the file `CMakePresets.json` from the platform folder into the root folder.

From now on, everything depends on your specific environment. For example, to use CMake from the terminal:

1. List available presets with `cmake --list-presets`
2. Configure the CMake preset with `cmake --preset PresetName`
3. Compile all targets with `cmake --build --preset PresetName`
4. Built binaries are located in: 
- `CoriEngine/bin/PresetName`
- `Apps/RendererSandbox/bin/PresetName`
- `Apps/Sandbox/bin/PresetName`
5. Done.

Make sure to check compiler paths in `cmake_toolchain.cmake`. It should point to your LLVM installation folders. Although it points to the default installation folder of LLVM, if you changed the install folder, you'll have to specify the correct path in the toolchain file.

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
	
### Abstraction:

Window/graphics/rendering are all abstracted, user code has no direct calls to OpenGL and other libs, the only exceptions being ImGui and GLM. I’m planning to start experimenting with Vulkan in the future, so having a good abstraction is will be very handy. 

***

## Plans for the future

- Make ImGui window templates. For example, instead of the user having to write a custom window to show a frame graph, we just call something like this: 'CoriImGuiPerformanceMetrics();'
- By the end of May, I’m planning to port my game VoidScape into the Cori Engine.
- I also want to start experimenting with Vulkan.
	- I’m planning to make Vulkan the main graphics API for Cori.
- Separating engine execution into different threads.
	- Creating a render command que, and a separate render thread.
- Also, in the near future, I want to implement some kind of Entity Component System.

## Currently available Apps

- `RendererSandbox` - this is the benchmark for 2D Batch Renderer.
- `Sandbox` - As the name implies, it is used mainly for testing purposes, nothing specific there.
