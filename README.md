# Cori Engine

![Cori](https://raw.githubusercontent.com/Salio24/CoriEngine/CoriStable/.github/images/git_logo.png)

Cori is my game engine that I’m actively working on. When building, I took some inspiration from **Hazel Engine** by **The Cherno** and **Unity**. The engine is still in development and its feature set constantly extends and improves.

***

## Getting Started

Cori Engine uses CMake as a build system.

<ins>**1. Necessary tools**</ins>
- CMake 3.28.x - 3.31.x
- Ninja build system
- Python 3.9 or newer
- Jinja2 python module
- Supported compilers:
  - Clang 20+
  - GCC 15+
  - Should work with earlier versions of compilers that also support C++23, but untested.
  - No support for MSVC and Clang-CL.


<ins>**2. Project configuration:**</ins>

1. Add Cori as a submodule for you project, recursively init all submodules with: `git submodule update --init --recursive`
2. Configure CMake, example:
```CMake
cmake_minimum_required(VERSION 3.28...3.31)

if(MSVC)
    message(FATAL_ERROR "MSVC is not supported, use GCC or Clang (not clang-cl)")
endif()

if(CMAKE_CXX_COMPILER MATCHES "/msys[36][24]/")
    set(CLANG_VARIANT "MSYS2" CACHE STRING "Detected MSYS2 environment")
else()
    set(CLANG_VARIANT "OTHER" CACHE STRING "Not MSYS2 environment")
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

if(CLANG_VARIANT STREQUAL "MSYS2")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic -lc++ -lc++abi -Wl,-Bdynamic")
    endif()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")
    endif()
endif ()

project(VoidScape)
    if(CLANG_VARIANT STREQUAL "OTHER" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:Debug>")
    endif ()

    if (NOT (
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/CMakeLists.txt")
            ))
        message(FATAL_ERROR "VoidScape thirdparty dependencies not found. Are you sure you cloned the repo recursively? Try running 'git submodule update --init --recursive' to download dependencies.")
    endif()

    set(CMAKE_CXX_STANDARD 23)

    add_compile_options(-march=native)

    add_subdirectory(${PROJECT_SOURCE_DIR}/CoriEngine)

    set(EXECUTABLE_NAME ${PROJECT_NAME})

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>)
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

    add_executable(${EXECUTABLE_NAME} ${MY_SOURCES})

    target_compile_options(${EXECUTABLE_NAME} PRIVATE -w)

    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:RelWithDebInfo>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>
    )

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE CoriEngine_static)
```

<ins>**3. Building:**</ins>

### Windows

First install Jinja2 by running: `pip install Jinja2`

Now you have several options: Visual Studio 2022 CMake integration, directly through CMake, CLion

#### Visual Studio 2022 CMake integration
1. Download LLVM toolkit from here: `https://github.com/llvm/llvm-project/releases`
    - Note: Don't change the default installation folder, and if you did so you need to change paths to the executables in: `clang_toolchain.cmake`
2. Install CMake tools and the clang-cl toolset for Visual Studio in the Visual Studio installer.
3. Copy `CMakePresets.json` and `clang_toolchain.cmake` from `(CoriEngineRoot)/platform/Windows/VS2022` to the project root folder.
4. Open project as a folder in Visual Studio 2022, wait for the CMake generation and choose `VoidScape` target and hit build.

- Note: Syntax highlighting might be working weirdly in VS2022 versions 17.13.0 and higher because as always microsoft broke somthing, and IntelliSense doesn't work great with clang.
- MSVC and Clang-CL are not supported! As well as visual studio solution!

#### Directly through CMake

1. Download LLVM toolkit from here: `https://github.com/llvm/llvm-project/releases`
    - Note: Don't change the default installation folder, and if you did so you need to change paths to the executables in: `Build_Debug_Windows.bat` `Build_Release_Windows.bat` `Build_RelWithDebInfo_Windows.bat`
2. Copy build scripts from `(CoriEngineRoot)/platform/Windows` to the project root folder.
3. Run one of build scrips for windows: `Build_Debug_Windows.bat` `Build_Release_Windows.bat` `Build_RelWithDebInfo_Windows.bat`

#### CLion

1. Open project folder.
2. By default, CLion will create a `Debug` CMake profile, you can add `Release` and `RelWithDebInfo` if you want.
3. Wait for CLion to generate CMake config and hit build on `VoidScape` target.

### Linux

First install Jinja2:
  - For Arch, you need to run: `sudo pacman -S python-jinja`
  - Idk about other distros.

Now you have several options: CLion, directly through CMake, and a bunch of other tools Linux can offer.

#### CLion

1. Open project folder.
2. By default, CLion will create a `Debug` CMake profile, you can add `Release` and `RelWithDebInfo` if you want.
3. Wait for CLion to generate CMake config and hit build on `VoidScape` target.

#### Directly through CMake

1. Make sure you have gcc package installed
2. Copy build scripts from `(CoriEngineRoot)/platform/Linux` to the project root folder.
3. Run one of build scripts for linux: `Build_Debug_Linux.sh` `Build_Release_Linux.sh` `Build_RelWithDebInfo_Linux.sh`

## Feature Set

### Core Systems
- **Modern C++23:** Leverages the latest C++ features for safer and more expressive code.
- **Layer-Based Architecture:** Applications are built as a stack of layers (e.g., game layer, UI layer), which can be modal to control event and update flow.
- **Asynchronous Task Handling:** A built-in worker thread pool and a main-thread command queue allow for offloading heavy tasks and safely interacting with engine systems from any thread.
- **Event System:** A flexible event system with compile-time event dispatching for handling input, window events, and custom gameplay events.
- **Tag-Based Logging:** A powerful logging system built on `spdlog` that allows for fine-grained filtering of log messages by category (e.g., Graphics, Physics, Asset Manager).
- **Asset Manager:** Descriptor based asset manager utilizing smart pointers for safe memory management. 

### Rendering
1. Legacy OpenGL:
    - **High-Performance 2D Batch Renderer:** Utilizes instanced rendering to draw thousands of quads and in minimal amount of draw calls. Utilizes a concept of depth so user doesn't need to worry about rendering order, correctly renders opaque and semi transparent object using depth testing and k-way merge algorythm.
    - **World & Screen Space Rendering:** Easily switch between rendering in camera-relative world coordinates and screen-fixed UI coordinates.
    - **High-Quality Text Rendering:** Uses Multi-channel Signed Distance Fields (MSDF) via `msdf-atlas-gen` to render crisp, scalable text. Font atlases are automatically cached to disk for fast startups. Supports several types of text alignment: Left, Center, Right.
    - **2D Sprite Animation System:** A `QuadAnimator` component plays animation sequences loaded from `AnimationPack` assets. Supports JSON configurations exported from `Aseprite` as well as engine custom formats.
    - **Automatic Texture Padding:** `SpriteAtlas` loading includes automatic padding/extrusion to prevent common "texture bleeding" artifacts.
2. Vulkan 1.3 (WIP!, not stable at the moment):
    - **GPU-Driven architecture:** Indirect rendering with compute culling and draw command generation in compute shader.
    - **Hybrid Memory Subsystem:** Custom templated containers like `VulkanFlatSlotMap` and `VulkanDynamicVectors` that sync data to the GPU, use sector-based dirty tracking to minimize PCIe bus usage. `VulkanVirtualBufferAllocator` allows to create buffers of necessary size each frame without physically creating separate `VkBuffer` objects, creating such buffer (`VulkanVirtualBuffer`) is practically free. And last but not least, asset streaming line, uses timeline semaphores and a separate transfer queue for non blocking asset loading.
    - **Bindless Architecture:** Fully bindless resources using `VK_EXT_descriptor_buffer` for samplers and images and `Buffer Device Address` (`BDA`) for buffers.
    - **Shader Objects:** Utilizing `VK_EXT_shader_object` to decouple shaders from pipeline state, preventing pipeline state object (`PSO`) explosion.
    - **Render Graph:** A DAG-based system that automatically resolves execution dependencies and injects memory barriers where needed.
    
### Entity Component System (ECS)
- **Powered by EnTT:** Built on the fast and feature-rich `EnTT` library.
- **Scene & Entity Management:** Provides a clean `Scene` and `Entity` API for creating, finding, and managing game objects.
- **Hierarchical Scene Graph:** Entities can be parented to each other, with automatic inheritance of transforms (position, rotation, scale, and depth).
- **System Architecture:** Game logic is organized into `Systems` that operate on components, with a priority system to control the update order. Includes built-in systems for transforms, physics, animations, and more.
- **State Management:** A built-in `StateMachine` component allows for robust management of entity states (e.g., idle, walking, attacking), with scriptable states defined by inheriting from an `EntityState` base class.

### Physics
- **Integrated 2D Physics:** Cori uses `Box2D` for its physics and a `Box2cpp` wrapper for more C++ like API.
- **Scriptable Triggers:** A powerful `Trigger` component system allows for creating scripted zones that react to entities entering, staying, and exiting, using a custom `TriggerBehaviour` interface.
- **Multithreading Support:** Physics stepping can optionally be parallelized across multiple threads for performance-heavy simulations.

### Audio
- **SDL3_mixer Backend:** A capable audio engine for playing sounds and music.
- **Advanced Playback Control:** The `Track` and `Mixer` system allows for complex audio sequencing, looping, fading, and tag-based group control (e.g., "Stop all `Enemy` sounds").

### File System
- **Alias-Based Path Management:** A `PathManager` loads a project's file structure from a `fsgame.json` config, allowing for organized, portable, and easily accessible file paths using simple aliases (e.g., `ASSETS, APP_ROOT, etc`).
- **Robust Binary Serialization:** A `BinaryFileManager` for saving and loading aggregate structs with a strong focus on data integrity. It features automatic corruption detection via header/footer UUIDs, compile-time struct type validation, checksums, and a backup/fallback system.
- **JSON Serialization:** A simple `JsonSerializer` utility for human-readable configuration and save files, built on the `nlohmann/json` library.

### Tooling & Debugging
- **Tracy Profiler Integration:** [Tracy Profiler](https://github.com/wolfpld/tracy) integration for incredible insight into performance bottlenecks.
- **Dear ImGui Integration:** Seamlessly create debug menus, editors, and in-game tools.
- **Instance Metrics:** A built-in profiling tool to track the currently alive and total created count of any asset type.

### Utilities
- **Mathematical Expression Parser:** A `Math::Function` class, powered by `ExprTK`, allows for runtime parsing and evaluation of complex mathematical expressions with variable and alias support.
- **Custom Error Handling:** A robust `CoriError` class provides detailed, context-rich error objects for use with `std::expected`, improving debugging and safe error propagation.
- **Compile-Time String Hashing:** Generates 64-bit or 32-bit FNV-1a hashes from strings at compile time using user-defined literals (`"my_string"_hs64` for 64-bit hash or `"my_string"_hs32"` for 32-bit hash).

***

## Documentation:

- **API Documentation:** Available [Here](https://salio24.github.io/CoriEngine/)
- **Project Wiki:** Planed, WIP

***

## Plans for the future
Cori is still under active development. Here are some of the major long-term plans:
- **In-Engine Editor:** Plan is to make an editor with support for C# scripting using `Mono`, custom project format, scene serialization and more.
- **Vulkan Renderer:** In the future I'm planning to move from OpenGL to Vulkan.

***

## Core Dependencies
Cori Engine is built on top of several excellent open-source libraries, special thanks to:

- **[EnTT](https://github.com/skypjack/entt):** For the high-performance Entity Component System.
- **[Box2D](https://github.com/erincatto/box2d):** For this amazing 2D physics engine.
- **[Box2cpp](https://github.com/HolyBlackCat/box2cpp):** For providing a convenient C++ wrapper for `Box2D`.
- **[SDL3](https://github.com/libsdl-org/SDL):** For handling: windowing, input, application events, `OpenGL` context and more.
- **[SDL3_mixer](https://github.com/libsdl-org/SDL_mixer):** For handling audio.
- **[SDL3_image](https://github.com/libsdl-org/SDL_image):** For helping in loading and later transforming images.
- **[Dear ImGui](https://github.com/ocornut/imgui):** For creating debug UIs and tools.
- **[spdlog](https://github.com/gabime/spdlog):** For the powerful and flexible logging system.
- **[glad2](https://github.com/Dav1dde/glad):** For loading `OpenGL` function pointers.
- **[glm](https://github.com/g-truc/glm):** For all mathematics-related functionality.
- **[msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen):** For high-quality font atlas generation.
- **[Tracy Profiler](https://github.com/wolfpld/tracy):** For great in-depth performance profiling tool.
- **[nlohmann/json](https://github.com/nlohmann/json):**  For `JSON` serialization and deserialization.
- **[ExprTK](https://github.com/ArashPartow/exprtk/tree/master):** For mathematical expression parsing.
- **[ska_sort](https://github.com/skarupke/ska_sort):** For very fast radix sort implementation.
- **[magic_enum](https://github.com/Neargye/magic_enum):** For enum reflection.
- **[boostorg/pfr](https://github.com/boostorg/pfr):** For an absolute wizardry with the aggregate struct reflection.
- **[stduuid](https://github.com/mariusbancila/stduuid):** For implementing `P0959R3` proposal.
- **[utfcpp](https://github.com/nemtrif/utfcpp):** For convenient utility to convert frm `UTF-8` to `UTF-32` and vice versa.
- **[tmxlite](https://github.com/fallahn/tmxlite):** For an amazing parser for `Tiled` `TMX` map format.
