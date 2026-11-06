# How the port works

Technical notes for people working on the port. For playing, see the [README](../README.md) and [CONTROLS.md](CONTROLS.md). The list of files changed in the emulator and how to merge new versions is in [TECHNICAL_NOTES.md](TECHNICAL_NOTES.md).

> Parts of this document describe code that is written after the notes. Each such section is marked `VERIFY` in the source of this file and is checked against the finished build before the release.

## What CitraFrame is

CitraFrame is one process: Azahar's Qt program (`azahar`, still called that) plus a new `src/frame_xr/` that owns the OpenXR session. The Qt window is the settings dialog and stays on the SteamVR dashboard. The emulator's emulation thread renders the 3DS's screens as usual; a copy of those screens goes to SteamVR.

Everything CitraVR changed in the emulator's core (the vertex shader factor, the view matrix hook in the rasterizer, the forced 3D slider) is kept and is Vulkan-ready as it stands. The rest of CitraVR (about 5,700 lines of OpenXR code in `jni/vr`, plus the Kotlin app) is Android only and is not built.

## One process

<!-- VERIFY -->

`frame_xr` starts with `azahar --vr`. The Qt window opens first and stays mapped for the whole run: unmapping a window crashed gamescope when I tried it in another app. If the OpenXR session cannot be created the window carries on as a normal emulator and the toolbar has an **Enter VR** action.

One OpenXR session lives for the whole run, not one per game.

## Vulkan: one device for the whole run

<!-- VERIFY -->

The emulator creates a new renderer and a new Vulkan instance for every game it boots. SteamVR wants one device, created through the runtime (`xrCreateVulkanInstanceKHR`, `xrCreateVulkanDeviceKHR`, `XR_KHR_vulkan_enable2`).

`video_core/vulkan_hooks.h` is a small set of function pointers (`create_instance`, `pick_physical_device`, `create_device`). `frame_xr` creates the `VkInstance` and `VkDevice` once, through the runtime, when the process starts. Every emulator `Instance` created later adopts that device without owning it, and the device is never destroyed between games. The call sites are in `vk_platform.cpp` and `vk_instance.cpp`. The libretro build of Azahar already injects a foreign instance and device the same way; the hooks follow that.

## Getting the screens into SteamVR

<!-- VERIFY -->

The emulator's Vulkan renderer prepares three screens each frame: top left, top right and bottom. `vk_xr_target.*` owns three OpenXR swapchains for them, each the 3DS screen's size (400x240, 400x240, 320x240) times the internal resolution factor. `SwapBuffers` calls `RenderToXr()` (`renderer_vulkan_xr.cpp`) after the desktop window has been drawn:

1. acquire and wait for an image of each swapchain;
2. draw each screen full-rectangle into it, with the emulator's own screen draw;
3. submit with a fence;
4. a release thread waits for that fence and only then calls `xrReleaseSwapchainImage`.

Images are released only after the GPU has finished them, so SteamVR never reads a half-drawn screen. The desktop window keeps rendering as before.

The 3D slider is forced on while in VR (CitraVR's `factor_3d`). If **Render 3D** is Off in a game's settings, the right image has to differ from the left one; if it does not, the render mode is forced to Side by Side.

## Threads

<!-- VERIFY -->

- The emulation thread renders the 3DS at 60 frames a second.
- The `frame_xr` thread runs `xrWaitFrame`, `xrBeginFrame` and `xrEndFrame` at the display's rate and submits the three quad layers with the last released images each time. It also syncs the controller actions. It does no GPU work while a game runs; the laser's dot is a small pre-rendered quad.
- Tracking uses the LOCAL space. The app asks SteamVR for 120 Hz for its own entry and takes 90 Hz otherwise.

SteamVR on the Frame has no cylinder layer and no passthrough, only quads and projection layers. The screens are quad layers.

## The game picker

<!-- VERIFY -->

One quad, 1.6 m wide, 2 m in front of you, drawn on the CPU (the same UI canvas and bitmap font as GalaxyFrame). It lists the games in the folders the Qt window knows (`UISettings::values.game_dirs`) with the 48x48 icon and title read from each game's SMDH. Icons are cached in `~/.cache/citraframe/icons`. Pointing at a tile highlights it; the trigger or **A** boots it through the same code as double-clicking in the Qt list (`GMainWindow::BootGame`, queued onto the Qt thread). **B** goes back, the stick scrolls. A **Rescan** tile rereads the folders.

In a game, holding **View** for 1.5 seconds calls `ShutdownGame` and returns to the picker.

## Controls

<!-- VERIFY -->

`input_common/frame_xr.*` is a new input engine in Azahar's input system, fed by the `frame_xr` thread's controller actions. A VR profile is the default, so Azahar's controls dialog stays the editor. Touch goes through the same `GRenderWindow` touch functions as the desktop window, using the bottom screen's rectangle from the framebuffer layout. The mapping is in [CONTROLS.md](CONTROLS.md). The interaction profile is `/interaction_profiles/valve/frame_controller`; the left controller's system button is never bound.

## Immersive mode

<!-- VERIFY -->

CitraVR's trick: the vertex shader divides `gl_Position.xy` by `vr_immersive_mode_factor`, so the game's world is drawn larger on the screen, and the rasterizer multiplies the game's own view matrix register by the inverse of the head pose (`SetVRData` / `ApplyVRDataToPicaVSUniforms`). It guesses where the matrix is, which only works for a few games.

CitraFrame feeds the head pose once a frame from the `frame_xr` thread, as an atomic snapshot the emulation thread reads, where CitraVR let the two threads race. The top screen is a quad in view space 0.5 m in front of the head, sized by the factor, with an offset of 0.0325 m for each eye in mode 3. Immersive mode raises the internal resolution by two steps. The settings are per game in the Qt dialog, with CitraVR's keys (`vr_immersive_mode` and the ones next to it). The heuristics cover Ocarina of Time 3D and Majora's Mask 3D.

## Shader caches

Azahar's Vulkan renderer writes a disk cache of translated shaders and one of the driver's pipelines for each game, under `user/shaders`. A new game compiles as it draws and loads the cache from then on. The cache can only be made by drawing the game, so there is no ahead-of-time warm-up script the way GalaxyFrame has one. <!-- VERIFY: a seed cache for popular games may be shipped later -->

## Launcher and files

`citraframe.sh` starts inside `~/.local/share/CitraFrame`, which holds a `user/` folder, and so Azahar uses it as its portable user directory instead of `~/.local/share/azahar-emu`. It unsets the Mesa overrides and `LD_LIBRARY_PATH` that Steam sets for games, sets `XR_RUNTIME_JSON` to SteamVR's runtime file only if nothing else names a runtime, sets SteamVR's per-app settings, and creates `~/.cache/headset-in-use` while the emulator runs so other work on the Frame waits.

The OpenXR loader is SteamVR's own, `/opt/steamvr/bin/linuxarm64/libopenxr_loader.so`, opened at run time; the headers are vendored in `third_party/openxr`. The program links the Qt 6 and SDL2 libraries SteamOS has; OpenAL Soft and Cubeb are built in.
