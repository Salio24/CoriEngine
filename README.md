# Cori Engine

![Cori](https://raw.githubusercontent.com/Salio24/CoriEngine/CoriStable/.github/images/git_logo.png)

Cori is my game engine that I'm actively working on. It started as a 2D OpenGL engine, taking inspiration from **Hazel Engine** by **The Cherno** and **Unity**, and has since turned into something quite different: a **3D, Vulkan-only engine written in C++26**, built around a GPU-driven renderer, a fully bindless resource model, and a threaded architecture with a dedicated render thread. **Snowflake** is the in-repo editor that doubles as the engine's main test bed ***(in early stages of development)***.

***

## Platform & Toolchain

**Linux only, GCC 16 only, for the time being.** (I will add Windows support via mingw later.)

The engine uses **C++26 static reflection** (P2996 and friends) in: asset type registration, console variable schemas, and serialization all go through `std::meta`. **GCC 16 with `-freflection` is currently the only compiler that builds this repo.** Clang support will come back when its reflection implementation is ready. MSVC and clang-cl are not supported and won't be.

**Hardware:** a **Vulkan 1.3** driver. Required device extensions are `VK_EXT_shader_object`, `VK_EXT_descriptor_buffer`, `VK_EXT_extended_dynamic_state3`, `VK_KHR_robustness2`, `VK_EXT_memory_priority` and `VK_EXT_memory_budget`, alongside a fairly wide slice of the 1.3 core feature set: dynamic rendering, sync2, descriptor indexing with update-after-bind and non-uniform indexing, `runtimeDescriptorArray`, buffer device address, scalar block layout, `multiDrawIndirect` + `drawIndirectCount`, timeline semaphores, `samplerFilterMinmax`, `shaderInt64` + `shaderBufferInt64Atomics`, `shaderDrawParameters`, `shaderDemoteToHelperInvocation`, `fragmentStoresAndAtomics`, `fillModeNonSolid` + `wideLines`, and the robustness2 trio (`robustBufferAccess2`, `robustImageAccess2`, `nullDescriptor`). Device selection fails outright if any of it is missing - see `VulkanEngine::PickPhysicalDevice`. In practice that means roughly **Turing+ / RDNA1+ / Arc**.

***

## Getting Started

Cori Engine uses CMake with Ninja.

<ins>**1. Necessary tools**</ins>

- CMake 3.28 or newer (developed against 4.3)
- Ninja
- GCC 16 or newer
- `make` and a C compiler - hwloc is built from its autotools tree by an `ExternalProject`, not by CMake
- Network access at configure time - hwloc and FreeType are pulled by `FetchContent` rather than vendored as submodules, so the first configure is considerably slower than the rest
- A Vulkan 1.3 capable GPU and driver, see above
- Only if you intend to edit shaders: `slangc`, from the Vulkan SDK or a standalone Slang release. See [Shaders](#shaders).

<ins>**2. Cloning**</ins>

```bash
git clone --recursive git@github.com:Salio24/CoriEngine.git
cd CoriEngine
# or, if you already cloned without --recursive:
git submodule update --init --recursive
```

<ins>**3. Building**</ins>

The repository is self-contained - the root `CMakeLists.txt` builds the engine static library (`CoriEngine_static`) and the `Snowflake` editor.

```bash
cmake -S . -B out/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/Debug --parallel
```

Or copy a script from `platform/Linux/` into the repo root and run it: `Build_Debug_Linux.sh`, `Build_Release_Linux.sh`, `Build_RelWithDebInfo_Linux.sh`.

The editor binary lands in `Snowflake/bin/<build-dir-name>/Snowflake` - the last path component of the build directory is used as the output folder name, so `-B out/Debug` produces `Snowflake/bin/Debug/`. Run it with that directory as the working directory: `PathManager` resolves the project layout from `../fsgame.json`, which lives at `Snowflake/bin/fsgame.json`.

<ins>**4. CMake options**</ins>

| Option                       | Default | What it does                                                                                                                     |
|------------------------------|---------|----------------------------------------------------------------------------------------------------------------------------------|
| `CORI_ENABLE_TRACY_PROFILER` | `OFF`   | Tracy client in Debug / RelWithDebInfo builds.                                                                                   |
| `CORI_USE_NATIVE`            | `ON`    | Adds `-march=native`.                                                                                                            |
| `CORI_USE_AVX2`              | `ON`    | AVX2 in third-party libraries                                                                                                    |
| `CORI_FORCE_AVX2`            | `OFF`   | force `-mavx2` instead of native.                                                                                                |
| `CORI_USE_MOLD`              | `OFF`   | Link with mold.                                                                                                                  |
| `CORI_SANITIZE`              | `OFF`   | Address sanitizer.                                                                                                               |
| `CORI_VK_DL_DEBUG_AMD`       | `OFF`   | Device-loss debugging via `VK_EXT_device_fault`, `VK_EXT_device_address_binding_report` and `VK_AMD_buffer_marker` breadcrumbs.  |
| `CORI_VK_DL_DEBUG_NVIDIA`    | `OFF`   | Nsight Aftermath crash dumps + `VK_NV_device_diagnostic_checkpoints`. Needs the Aftermath SDK dropped into `Engine/thirdparty/`. |
| `CORI_BUILD_DOCS`            | `OFF`   | Doxygen documentation target.                                                                                                    |
| `CORI_CLANG_TIDY`            | `OFF`   | clang-tidy for third-party libraries where supported.                                                                            |
| `CORI_BUILD_SANDBOX_APPS`    | `OFF`   | Sandbox apps. They are not really maintained and will almost certainly fail to build.                                            |

***

## Repository Layout

| Path                 | What lives there                                                                                 |
|----------------------|--------------------------------------------------------------------------------------------------|
| `Engine/src/`        | The engine itself, built as the `CoriEngine_static` library.                                     |
| `Engine/enginedata/` | Engine-owned runtime data: the built-in Slang shaders and their descriptors, placeholder assets. |
| `Engine/thirdparty/` | Vendored dependencies, mostly submodules.                                                        |
| `Engine/docs/`       | Doxygen configuration, built by `CORI_BUILD_DOCS`.                                               |
| `Snowflake/`         | The editor - `src/`, its `assets/`, and `bin/` where builds land next to `fsgame.json`.          |
| `Sandboxes/`         | Older single-purpose test apps. Unmaintained, off by default.                                    |
| `Tools/gdb/`         | GDB pretty printers for the engine's SPSC ring and the TBB containers.                           |
| `platform/Linux/`    | Convenience build scripts.                                                                       |

***

## Shaders

Shaders are not compiled by CMake. They are written in [Slang](https://github.com/shader-slang/slang), compiled ahead of time with `slangc`, and the resulting `.spv` are committed to the repo - so a plain build needs no shader toolchain at all.

If you edit a `.slang` file, recompile it from `Engine/enginedata/shaders/`:

```bash
cd Engine/enginedata/shaders
./recompile.sh        # or ./recompileDebug.sh for -g -O0, for RenderDoc and shader debugging
```

`slangc` comes with the Vulkan SDK, or from a standalone Slang release. Each shader is compiled to SPIR-V 1.6 with `-fvk-use-scalar-layout` and `-fvk-use-entrypoint-name`, with every entry point of a file emitted into one module. Alongside each `.spv` sits a small JSON descriptor naming the module and its per-stage entry points - that is what the engine actually loads. Rasterization state is not in there; it belongs to the shader effect that references the pair. The editor's own shaders under `Snowflake/assets/Shaders/` work the same way and have their own pair of scripts. Recompiled shaders are picked up by the asset hot reload without restarting the editor.

***

## Feature Set

### Core Systems
- **Threaded architecture:** the main thread runs SDL event pumping, a fixed-rate tick loop, ECS system updates and ImGui frame building, a **dedicated render thread owns all Vulkan**. Work crosses between them through pooled `FrameData` slots with hand-rolled SPSC rings (zero per-frame allocation) and a `RenderThreadCommandQueue` with watermark ordering guarantees. The render thread parks on a single wakeup atomic that is notified each frame.
- **Frame pacer:** a just-in-time pacing wait at the top of the update loop, driven by real scanout timestamps. It moves the idle wait to the *front* of the frame so sampled input only ages across actual work.
- **Job system & CPU topology:** The use of TBB job system is ***planned*** (so far I'm using a simple thread pool, tho I use TBB's concurrent containers in quite a few places). An hwloc-backed `CpuTopology` pins the main and render threads to a preferred last-level-cache domain (L3, falling back to L2), so the two threads that hand frames to each other share a cache.
- **Layer-based architecture:** applications are a stack of layers which can be modal to control event and update flow.
- **Event system:** input, window and custom gameplay events, dispatched by `std::type_index` against a compile-time type tag baked into each event class.
- **Tag-based logging:** built on `spdlog`, with fine-grained runtime filtering by category and a ring buffer feeding the editor console.
- **Console & CVars:** a real developer console with reflection-generated variable schemas, tab completion, contextual usage, cheat gating, read-only and restart-tier variables, and a persistent user archive. Console commands self-register through an intrusive lock-free list.
- **Error handling:** `std::expected<T, ErrorCode>` or just `ErrorCode`. `-fno-exceptions` is the goal. `CoriError<>` is legacy and will be removed.

### Vulkan Renderer

The renderer is the largest subsystem in the engine by a wide margin, and the part under the most active development.

- **Two-level renderer split:** a `MasterRenderer` owns the render thread, the scene renderer registry, the swapchain and the compositor. Each `SceneRenderer` owns its own render graph, its own `PersistentRenderTarget` and a three-stage frame (`Stage1` build / `Stage2` record / `Stage3` submit). Multiple scenes render per frame and are composited in direct-blit, hybrid or dockspace mode.
- **GPU-driven architecture:** instances are culled in compute against frustum planes, indirect draw commands are generated on the GPU, surviving instance IDs are compacted through atomic counters, and the whole thing is submitted with `drawIndexedIndirectCount` - one indirect command per batch, where a batch is a mesh inside a draw group keyed by shader effect. Nothing is culled on the CPU.
- **Render graph:** a per-frame declarative DAG compiler. Passes declare reads and writes, the graph topologically sorts them, synthesizes all `VkImageMemoryBarrier2` / `VkBufferMemoryBarrier2` automatically by tracking per-resource stage/access/layout, and pools transient images by their description across frames. The graph is rebuilt from scratch every frame, so image state tracking starts uniform each frame and explodes to per ***mip, layer*** granularity on the first divergent subresource use.
- **Fully bindless:** there is **not a single `vkCmdBindDescriptorSets` call in the engine**. Samplers and images live in a descriptor buffer (`VK_EXT_descriptor_buffer`), everything else is reached through buffer device addresses carried in a 128-byte all-stages push constant range. Meshes, materials and textures are indexed by integer slot into GPU-visible slot maps, with versions validated shader-side. I will move to `VK_EXT_descriptor_heap` once it becomes somewhat adopted.
- **Shader objects:** `VK_EXT_shader_object` decouples shaders from pipeline state, so there is no PSO explosion, rasterization state is set dynamically via `vkCmdSet*` and never triggers a recompile.
- **Slang shaders:** all shaders are written in [Slang](https://github.com/shader-slang/slang), compiled ahead of time to SPIR-V 1.6 with scalar block layout and named entry points. See [Shaders](#shaders).
- **Hybrid memory subsystem:** custom templated GPU containers `VulkanFlatSlotMap`, `SparseFlatSlotMap<VulkanGPUSyncedSequentialStorage>`, `VulkanDynamicVector` that mirror CPU mutations to GPU memory using **sector-based dirty tracking**, so only modified regions cross the PCIe bus. `VulkanVirtualBufferAllocator` hands out `VulkanVirtualBuffer` views over pre-allocated arenas, making per-frame buffers practically free.
- **Asynchronous asset streaming:** meshes and textures stream over a dedicated transfer queue with timeline semaphores and completion tickets. CPU-side parsing and loading from disk is done on the workers so it never stalls the main thread or the render thread.
- **Deferred destruction:** a frames-in-flight-aware `DeletionQueue` with separate queues for buffers, images, shader objects, virtual allocations and virtual blocks.
- **Sticky placeholders:** every asset type except compute shaders has a placeholder that is never unloaded. A slot gets the placeholder the moment it is registered and keeps it until the real asset streams in, and a failed load leaves it there rather than a null handle - so nothing in the draw path ever dereferences a missing asset. When a material reload swaps its shader effect, a listener notifies the scene renderer so the affected objects are re-batched into the right draw group.
- **Editor rendering features:** GPU entity picking with a ticketed readback ring, selection and hover outlines, mesh-space AABB debug wireframes, wireframe mode, and an off-screen thumbnail rendering path that copies into a shared atlas.
- **Input latency measurement:** input-to-photons latency via `VK_EXT_present_timing` + `VK_KHR_present_id2` + `VK_KHR_calibrated_timestamps`, plotted in Tracy. Real hardware input timestamps come from Wayland's `zwp_input_timestamps_v1`, every present stage is separately calibrated because the driver reports each in its own clock domain. A `VK_KHR_present_wait2` queue throttle caps how far the engine may run ahead of scanout.
- **GPU crash & device loss debugging:** one facade over two vendor backends. AMD gets `VK_EXT_device_fault` fault addresses resolved to named buffers/images through `VK_EXT_device_address_binding_report`, plus `VK_AMD_buffer_marker` breadcrumbs so a hang reports as “BEGIN reached, END not reached” for the culprit pass. NVIDIA gets Nsight Aftermath crash dumps with shader-line attribution and `VK_NV_device_diagnostic_checkpoints`.
- **Debug instrumentation:** `VK_EXT_debug_utils` label regions around the frame, every render graph pass, barrier batches, compositor blits, streaming copies and queue submits always on in debug builds so RenderDoc and RGP captures are readable. Tracy GPU zones on top.

### Entity Component System
- **Powered by EnTT:** built on the fast and feature-rich `EnTT` library, wrapped in a `Scene` / `Entity` / `SceneHandle` API.
- **Hierarchical scene graph:** entities can be parented, with automatic inheritance of transforms and depth, dirty-flag propagation.
- **System architecture:** logic is organized into `System`s with declared priorities controlling update order, including built-in `Transform`, `Hierarchy`, `Physics`, `Animation`, `StateMachine`, `Trigger` and `RenderSync` systems.
- **ECS ↔ renderer bridge:** the `RenderSync` system translates `Rendering` and `Transform` components into `RenderObject` handles in the scene renderer, pushing only dirty state across the thread boundary using dirty component tags.
- **State management:** a `StateMachine` component with scriptable states derived from an `EntityState` base class.

### Asset System
- **Hub-and-spoke asset manager:** `AssetManager2` is the hub, per-type managers (texture, mesh, shader, shader effect, material) are spokes, reached generically through concept-checked contracts rather than virtual dispatch.
- **Thread-safe by construction:** `Load<T>` is callable from any thread and returns immediately. One mutex guards against some logical races, refcounts and versions are lock-free atomics with terminal-zero semantics. Parsing happens on worker threads, GPU-side finalization on the render thread through the command queue.
- **RAII references:** `AssetRef<T>` is a custom-made ref-counted handle, `AssetID` is a 64-bit FNV-1a hash of the asset path.
- **Hot reloading:** assets reload in place on a bumped generation - there is no separate reload path, a reload is just another `Load`. Once a second, worker threads rescan the asset directories, comparing descriptor hashes and source file timestamps, and reload whatever changed. Policy is two `static constexpr bool` flags per type - declared by the spoke, overridable by the asset type itself - so shader pairs, compute shaders, meshes and textures pick up edits on their own, while shader effects and materials are reload-capable but not currently triggered by anything.
- **JSON asset descriptors:** assets are described by small JSON files (metadata + data) parsed with `glaze`, with virtual `assets://` paths support.

### Snowflake Editor

![Snowflake](https://raw.githubusercontent.com/Salio24/CoriEngine/EngineDev/.github/images/Screenshot-2026-08-29_03-02-54.png)

- **Docking editor shell:** an ImGui dockspace with a persistent default layout, keyboard-driven dock navigation, floating window move/resize, panel shortcuts, a launcher and a custom (and some Catppuccin)  palette(s).
- **Scene viewport:** renders into a `PersistentRenderTarget` handed to ImGui as a texture, with a fly camera, relative-mouse capture, and settled resize handling.
- **Entity picking & highlighting:** click to select and hover to highlight, resolved by a GPU picking pass writing entity IDs, read back asynchronously through a ticketed ring so the main thread never stalls. Selection and hover get distinct outline colors.
- **Content browser:** a virtual asset tree backed by an `efsw` file watcher with incremental targeted rescans, pending/unresolved entry handling for files still being written.
- **Thumbnail cache:** meshes and materials are rendered off-screen into a shared thumbnail atlas with multiple size classes, framed automatically from the mesh-space AABB, textures are shown directly. Thumbnails refresh when the underlying asset hot-reloads or the required thumbnail resolution changes.
- **Console panel:** the reflection based CVars and command console in-editor, with completion, contextual usage hints, and a log view filtered by level, text, and a tag tree that sets per-tag level floors at the source - so filtering actually removes cost from the logging path rather than just hiding lines.

### Physics & Audio

Carried over from the 2D era. They are not where the engine's attention is right now and might not work.

- **2D physics:** `Box2D` with the `box2cpp` C++ wrapper, optional multithreaded stepping across the worker pool, and a scriptable `Trigger` component system with enter/stay/exit callbacks through a `TriggerBehaviour` interface.
- **Audio:** an `SDL3_mixer` backend with a `Track` / `Mixer` system supporting sequencing, looping, fading and tag-based group control.
- **Text & sprites - currently orphaned.** MSDF text rendering via `msdf-atlas-gen` with disk-cached atlases, sprite atlases with automatic padding, and a `QuadAnimator` component driving `AnimationPack` assets (Aseprite JSON and custom formats). The OpenGL `Renderer2D` that used to draw all of this is gone, and the Vulkan renderer has no 2D path yet, so **nothing currently renders text or sprites**. I will bring back text rendering back for sure, but later.

### File System
- **Alias-based path management:** `PathManager` loads a project's file structure from `fsgame.json`, giving portable alias paths (`ASSETS`, `APP_ROOT`, …).
- **Robust binary serialization:** `BinaryFileManager` for aggregate structs, with corruption detection via header/footer UUIDs, compile-time struct type validation, checksums, and a backup/fallback.
- **JSON serialization:** `glaze` for the hot paths (asset descriptors, CVar archives, Vulkan flag enums) with `nlohmann/json` still around for the older utilities.

### Tooling & Debugging
- **Tracy Profiler integration:** CPU zones, GPU zones with host-calibrated contexts, frame markers, and custom plots for latency, pacing and thread affinity.
- **Dear ImGui integration:** docking branch, single-context with a deep-snapshotted `ImDrawData` published across the thread boundary.
- **GDB pretty printers** for the engine's SPSC ring and TBB containers.

### Misc
- **Compile-time string hashing:** `"my_string"_hs64` / `"my_string"_hs32` for 64- and 32-bit FNV-1a hashes.
- **Mathematical expression parser:** `Math::Function`, powered by `ExprTK`, for runtime parsing and evaluation of expressions with variables and aliases.
- **Data structures:** flat and sparse slot maps, packed arrays, sequential storage, concurrent handle allocators, and a hand-rolled SPSC ring (based on rigtorp's implementation).

***

## Running & Profiling

**Wayland.** SDL3 falls back to XWayland whenever the compositor lacks `fifo-v1`, and the X11 WSI supports neither present timing nor the real input timestamps the latency measurement is built on. If the latency plots come up empty, force the driver:

```bash
SDL_VIDEO_DRIVER=wayland ./Snowflake
```

**Tracy.** Configure with `-DCORI_ENABLE_TRACY_PROFILER=ON` (clean reconfigure) and build the profiler UI and the headless capture tool from the vendored Tracy:

```bash
./BuildTracy.sh   # -> Tools/TracyGUI/tracy-profiler, Tools/TracyCapture/tracy-capture
```

Tracy wants a few things the distro locks down by default - unprivileged system-wide perf events, readable tracefs tracepoint IDs, and an rtprio allowance for its sampling thread - or it silently collects no thread state.

**GDB.** `Tools/gdb/` has pretty printers for the engine's SPSC ring and for the oneTBB containers, which are otherwise unreadable in a debugger. Source them from `~/.gdbinit`; see `Tools/gdb/README.md`.

***

## Documentation

- **API Documentation:** available [here](https://salio24.github.io/CoriEngine/) - generated with Doxygen. **Badly out of date:** the workflow only publishes on pushes to the `CoriStable` branch, which last moved in October 2025, and I haven't updated the docs since, maybe will deal with it later if I will have time.
- **Project Wiki:** planned.

***

## Plans for the future

### Renderer

The current renderer is the GPU-driven scaffolding: culling, indirect submission, bindless resources and the render graph all work, but there is no real lighting model yet. I'm working on the design that will go on top of that, work will start once the editor reaches v1.

- **Clustered forward, not deferred.** Forward keeps MSAA cheap and matches the GPU-driven indirect stack that already exists.
- **Antialiasing is the engine's identity.** 2× MSAA with `VK_EXT_sample_locations` programmable sample positions on the Decima even/odd pattern, plus SMAA for morphological edges and geometric specular AA for normal-map variance. **The projection matrix is never jittered, so there is no TAA resolve** - no accumulation buffer, no blur, no smear. Temporal filtering is permitted on the lighting signal only, and only after albedo demodulation.
- **Hardware raytracing for indirect lighting.** Ray queries (`VK_KHR_ray_query`) first, ray tracing pipelines later. Direct lighting and shadows stay raster - shadow maps are noiseless by construction, which is exactly the property this engine is optimizing for. The existing bindless + BDA model and the BLAS-compatible vertex layout mean no refactor is needed to get there.
- **Clusters instead of whole-mesh culling.** meshoptimizer meshlets, cone culling, two-pass Hi-Z occlusion culling against a min-reduction depth pyramid. Mesh shaders come after that, and the traditional path is far easier to debug while the bounds math is being made correct.
- **Device Generated Commands** (`VK_EXT_device_generated_commands`) as the shader permutation mechanism, layered on once the renderer is correct. Execution sets of shader objects make per-object and per-LOD shader selection free at submission time, which removes one of the main historical reason to reach for deferred.
- **A reference path tracer as an editor mode**, accumulating while the camera is still - nearly free once acceleration structures exist, and the best available ground truth to A/B the real-time path against.

I explicitly rejected/avoid: deferred shading, TAA in any form with an accumulation buffer, dynamic resolution and upscaling as a crutch, light probes and lightmaps, Nanite-style micropolygons.

### Editor

Snowflake v1 is the current focus. Still to come:

- **Component inspector** will be built on C++26 reflection, following the same annotation pattern the CVar system already uses rather than inventing a second registration mechanism.
- **Scene hierarchy panel**, transform gizmos, and multi-selection.
- **Scene serialization and a project format**, so a scene is a file rather than code in `EditorLayer.cpp`.
- **Asset creation and editing** from the content browser - materials and shader effects first.

### C++ Hot Reload Scripting

Scripting will be **C++ hot reload**, not a scripting language: game code builds into a plugin-style module with stable entry points and a stable ABI across reloads. The boundary is already being treated as an invariant in the renderer design - the engine owns all rendering types, game code holds handles and POD components only, and nothing in the renderer stores a pointer into game-module memory or dispatches through a game-defined vtable.

### Other

- Compiling with `-fno-exceptions`.
- Restoring the Windows build once the Linux/GCC 16 path settles.
- Returning Clang support when its C++26 reflection implementation is ready.

### Some Known Issues and Suboptimal Places

- **VulkanStreamingLine:** was written quite a long time ago and the design turned out to be quite suboptimal. When a very large stream comes (say 64mb texture), it tries to stream it in one frame - trashing the PCIe lane and stalling/starting the graphics queue even tho it uses a dedicated transfer queue. Basically what I need to do is upload chunking and per frame copy budget (target ms * PCIe one way link speed * some mod (say 0.8)). Also, it doesn't have a dedicated thread, so rendering thread pays a hefty copy cost. Also, a new way to copy directly into the staging buffer, we request a reserve and get a mapped ptr into the reserved staging buffer memory. 
- **AssetLoading:** Current way of loading textures and meshes is quite suboptimal. First of all, using `RGBA8` instead of `BCx` for textures should be considered a war-crime in an of itself, I might write my own BCx loader, or just use `KTX` if it will fit me. Also, an engine native binary formats is a big thing, that way it will be possible to read from disk using file descriptors directly into the staging buffer (if it has space of course). Also `obj` is quite slow for loading at runtime, eventually I will move to `glTF` and to baking those ***regular*** asset files into engine native bins, and after that asset pack baking.
- **AssetManager Scanning:** Currently I just recursively scan the asset dirs and look for changes, which is brute force and can be optimized to not eat workers so much, will need to move to use `efsw` here as well.

***

## Core Dependencies

Cori Engine is built on top of several excellent open-source libraries, special thanks to:

- **[EnTT](https://github.com/skypjack/entt):** For the high-performance Entity Component System.
- **[SDL3](https://github.com/libsdl-org/SDL):** For handling windowing, input, application events and more.
- **[SDL3_mixer](https://github.com/libsdl-org/SDL_mixer):** For handling audio.
- **[SDL3_image](https://github.com/libsdl-org/SDL_image):** For helping in loading and later transforming images.
- **[Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)** and **[Vulkan-Loader](https://github.com/KhronosGroup/Vulkan-Loader):** For the vendored Vulkan API and loader.
- **[VulkanMemoryAllocator-Hpp](https://github.com/YaaZ/VulkanMemoryAllocator-Hpp):** For a C++ interface to AMD's VMA.
- **[oneTBB](https://github.com/uxlfoundation/oneTBB):** For concurrent containers and for the job system.
- **[hwloc](https://github.com/open-mpi/hwloc):** For CPU topology discovery and thread affinity.
- **[Dear ImGui](https://github.com/ocornut/imgui)** and **[imgui_club](https://github.com/ocornut/imgui_club):** For the editor and debug UI.
- **[efsw](https://github.com/SpartanJ/efsw):** For cross-platform filesystem watching behind the content browser.
- **[glaze](https://github.com/stephenberry/glaze):** For extremely fast reflection-based `JSON` serialization.
- **[nlohmann/json](https://github.com/nlohmann/json):** For `JSON` serialization and deserialization.
- **[spdlog](https://github.com/gabime/spdlog):** For the powerful and flexible logging system.
- **[glm](https://github.com/g-truc/glm):** For all mathematics-related functionality.
- **[fast_obj](https://github.com/thisistherk/fast_obj):** For fast `OBJ` mesh parsing.
- **[Tracy Profiler](https://github.com/wolfpld/tracy):** For a great in-depth performance profiling tool.
- **[Box2D](https://github.com/erincatto/box2d):** For this amazing 2D physics engine.
- **[Box2cpp](https://github.com/HolyBlackCat/box2cpp):** For providing a convenient C++ wrapper for `Box2D`.
- **[msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen):** For high-quality font atlas generation.
- **[FreeType](https://github.com/freetype/freetype):** For font rasterization and kerning data.
- **[dynamic_bitset](https://github.com/pinam45/dynamic_bitset):** For a convenient runtime-sized bitset.
- **[ska_sort](https://github.com/skarupke/ska_sort):** For a very fast radix sort implementation.
- **[magic_enum](https://github.com/Neargye/magic_enum):** For enum reflection.
- **[boostorg/pfr](https://github.com/boostorg/pfr):** For an absolute wizardry with the aggregate struct reflection.
- **[stduuid](https://github.com/mariusbancila/stduuid):** For implementing `P0959R3` proposal.
- **[utfcpp](https://github.com/nemtrif/utfcpp):** For a convenient utility to convert from `UTF-8` to `UTF-32` and vice versa.
- **[ExprTK](https://github.com/ArashPartow/exprtk/tree/master):** For mathematical expression parsing.
- **[tmxlite](https://github.com/fallahn/tmxlite):** For an amazing parser for `Tiled` `TMX` map format.
