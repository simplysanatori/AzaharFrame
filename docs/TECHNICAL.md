# How the port works

Technical notes for people working on the port. For playing, see the [README](../README.md), the [user guide](USER_GUIDE.md) and [CONTROLS.md](CONTROLS.md). Build and installer detail is in [AGENTS.md](../AGENTS.md). The list of files changed in the emulator and how to merge new versions is in [TECHNICAL_NOTES.md](TECHNICAL_NOTES.md).

## What Azahar for Frame is

Azahar for Frame is one process: Azahar's Qt program (`azahar`, still called that) plus a new `src/frame_xr/` that owns the OpenXR session. The Qt window is the settings dialog and the game list; it is shown on the SteamVR dashboard as a desktop window. <!-- VERIFY: the window on the dashboard --> The emulator's emulation thread renders the 3DS's screens as usual; a copy of those screens goes to SteamVR.

Everything CitraVR changed in the emulator's core (the vertex shader factor, the view matrix hook in the rasterizer, the 3D depth default) is kept and works with the Vulkan renderer as it stands. The rest of CitraVR (about 5,700 lines of OpenXR code in `jni/vr`, plus the Kotlin app) is Android only and is not built.

## One process

`frame_xr` starts with `azahar --vr` (`GMainWindow` in `citra_qt.cpp`, the `--vr` option). The Qt window opens first. If the Vulkan renderer is not selected, or the OpenXR session cannot be created, the log says `running flat` and the program carries on as a normal emulator.

When the session is up, three settings are adjusted for VR: the current controls profile gets the Frame bindings (see Controls), the layout options Single Screen and Separate Windows are changed to Default (both would hide a screen), and the 3D depth is set to 50 % if it is 0. The hidden second window's Vulkan surface is not created in VR.

One OpenXR session lives for the whole run, not one per game. SIGTERM and SIGINT stop the game and close the window, so the session ends cleanly when Steam stops the app.

## Vulkan: one device for the whole run

The emulator creates a new renderer and a new Vulkan instance for every game it boots. SteamVR wants one device, created through the runtime (`xrCreateVulkanInstanceKHR`, `xrCreateVulkanDeviceKHR`, `XR_KHR_vulkan_enable2`).

`video_core/vulkan_hooks.h` is a small interface (`VulkanHooks`: `CreateInstance`, `PickPhysicalDevice`, `CreateDevice`, `OwnsInstance`, `OwnsDevice`) and a second one for the screens (`XrScreenSink`). `frame_xr` makes the `VkInstance` and `VkDevice` once, through the runtime, when the process starts, using a throwaway `Vulkan::Instance` so that the create infos are Azahar's own. Every emulator `Instance` created later adopts that device without owning it, and the device is never destroyed between games. The call sites are in `vk_platform.cpp` and `vk_instance.cpp`. The libretro build of Azahar already injects a foreign instance and device the same way; the hooks follow that.

The desktop window's swapchain uses Mailbox or Immediate presentation in VR (`vk_swapchain.cpp`): a FIFO present on a hidden window could hold the queue lock the XR threads need.

## Getting the screens into SteamVR

The emulator's Vulkan renderer prepares three screens each frame: top left, top right and bottom. `vk_xr_target.*` owns three OpenXR swapchains for them, each the 3DS screen's size (400x240, 400x240, 320x240) times the internal resolution factor, at most 8x (SteamVR downsamples layers wider than 3458 px). `SwapBuffers` calls `RenderToXr()` (`renderer_vulkan_xr.cpp`) after the desktop window has been drawn:

1. acquire and wait for an image of each swapchain;
2. draw each screen full-rectangle into it, with the emulator's own screen draw (`DrawSingleScreen`);
3. submit with the scheduler's tick;
4. a release thread waits for that tick and only then calls `xrReleaseSwapchainImage`.

Images are released only after the GPU has finished them, so SteamVR never reads a half-drawn screen. The desktop window keeps rendering as before. The queue is shared through the scheduler's submit mutex, which the renderer lends to the XR thread for `xrBeginFrame` and `xrEndFrame` (`XrQueueRegistration`).

The three screens are the 3DS's own images: the right eye is the 3DS's right image, whatever **Render 3D** is set to for the desktop window. Depth comes from the game and the 3D depth setting (default 50 %, CitraVR's `factor_3d` default; Azahar's own is 0).

## Threads

- The emulation thread renders the 3DS at 60 frames a second.
- The `frame_xr` thread runs `xrWaitFrame`, `xrBeginFrame` and `xrEndFrame` at the display's rate and submits the quad layers with the last released images each time. It also syncs the controller actions, runs the frame callbacks (immersive mode) and draws the picker and the pointer dot. It does no GPU work while a game runs; the dot is a small pre-rendered quad.
- Tracking uses the LOCAL space; immersive mode also uses VIEW.
- The display rate is not asked for in the code. `azaharframe.sh` sets SteamVR's per-app settings (120 Hz by default, no throttling, no motion smoothing) before every launch, and `install.sh` sets them for the library entry.

SteamVR on the Frame has no cylinder layer and no passthrough, only quads and projection layers. The screens are quad layers.

## The game picker

One quad, 1.6 m wide, 2 m in front of you, 1536x1024 texels drawn on the CPU (the same UI canvas and bitmap font as GalaxyFrame), a grid of four columns and two rows. It lists the folders the Qt window knows (`UISettings::values.game_dirs`, pushed to the picker by `FrameXrPushGameDirs`: at VR start, when a game folder is added and when a game ends) with the 48x48 icon and title read from each game's SMDH. The installed titles folder is listed too. Icons are cached in `~/.cache/azaharframe/icons`. The file types are the ones the Qt game list accepts (`.3ds`, `.cci`, `.cxi`, `.app`, `.3dsx`, `.elf`, `.axf` and the compressed `.z3dsx`, `.zcci`, `.zcxi`, `.bcci`); a CIA is installed first from **File > Install CIA...**.

Pointing at a tile highlights it; the trigger or **A** boots it through the same code as double-clicking in the Qt list (`GMainWindow::BootGame`, queued onto the Qt thread). **B** goes to the previous page, a stick turns the pages, and **Rescan** rereads the folders. The header shows the "Azahar for Frame" lockup from `art/logo.png` (decoded with lodepng and scaled once, so it is drawn 1:1); without that file it shows the name as text. `art/` is looked for in `$AZAHARFRAME_ART`, next to the program, and in the checkout the build came from.

In a game, holding **View** for 1.5 seconds calls `ShutdownGame` and returns to the picker.

## Controls

`input_common/frame_xr.*` is a new input engine in Azahar's input system, fed by the `frame_xr` thread's controller actions through `frame_xr/input/` (the action sets, the mapper, the geometry of the screens). The mapping is in [CONTROLS.md](CONTROLS.md); `src/frame_xr/input/MAPPING.md` is the developer version. The interaction profile is `/interaction_profiles/valve/frame_controller` (`XR_VALVE_frame_controller_interaction`), with the Oculus Touch profile suggested as a fallback; the left controller's system button is never bound.

A VR profile is the default: when VR starts and no button of the current input profile is a `frame_xr` button, the buttons and sticks of that profile are replaced (`ApplyDefaultProfile`), so Azahar's controls dialog stays the editor. The dialog names the controls **Frame A**, **Frame Left Grip** and so on. Touch goes through the same `GRenderWindow` touch functions as the desktop window, using the bottom screen's rectangle from the framebuffer layout.

Screen focus (`input/screen_view.*`, driven from `controls.cpp`, drawn from `app.cpp`): the placement holds two slots, a main one and a secondary one below it, and a flag says which 3DS screen is in which. The mapper, the touch mapping, the drag and the layers all get the same two quads from `ScreenView::Layout`, so nothing else knows about the swap. The stick clicks are tap or 1 s hold gestures in the mapper (`input/click_gesture.h`). With the bottom screen in focus `Immersive::SetBottomFocused` pauses immersive mode in every mode, the same way the in-game menu pauses modes 2 and 3 (`Decide` in `immersive_layout.cpp`). `ScreenView::Layout` takes its two slots from `Immersive::Screens`, so the swap and the immersive placement compose; while a grip holds the immersive screen (`Immersive::HoldingScreen`) the stick click taps are ignored. The bottom screen's "shown" flag is shared with immersive.cpp in the games it manages (Hide Bottom Screen, immersive mode) and kept by `ScreenView` otherwise; the rules are in [CONTROLS.md](CONTROLS.md#swapping-the-screens).

## Controllers, beams, the controls help and the stats

`frame_xr/vis/` draws what is not a screen or Azahar's window. `Controls::Frame` calls `Visuals::TakeInput` first (a hand aimed at one of its panels is taken out of the input the window and the game see), then `Visuals::Frame` after the window or the game has added its layers. Its quads go on top of everything (`TopLayers`): the pointer dots, then the controller models, then the beams, within the runtime's `maxLayerCount`; `app.cpp` drops what does not fit and says so in the log.

- **Panels.** One atlas swapchain (1368x980) holds the controls help, the **Controls** and **Stats** buttons and the beam art; the stats panel has its own (512x320). They are drawn on the CPU with Noto Sans from SteamOS through stb_truetype (`panel_art.*`), the baked UI font when it is missing, and only when what they show changes. `async_overlay.*` copies them into the swapchain with its own command buffer and fence, waits for an image with a zero timeout and releases it on a later frame once the fence has signalled, so the XR thread never waits on the GPU. The help text is built in `help_content.cpp` from the bindings of the controls profile, which the Qt thread sends with `FrameXr::SetControlBindings` (at VR start, at each boot and after **Configure**). The stats come from `UpdateStatusBar` through `FrameXr::SetEmuStats`, the XR thread's own second summary, `/proc/stat` and the GPU's devfreq clock.
- **Beams.** A quad from the aim pose to where the ray ends, turned about its own axis to face the head and clipped 5 cm in front of it (the same art and look as my Moonlight XR). SteamVR has no colour scale, so the fade picks one of 8 pre-drawn alpha columns with `imageRect`.
- **Controller models.** One projection layer, both eyes side by side at about half the recommended size (at most 1000 px high, `AZAHARFRAME_MODEL_EYE_PX`), 4x MSAA with a depth buffer, cleared to transparent and premultiplied (`XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT`). This is the one piece of GPU work on the XR thread during a game, and it keeps the rules of the screens: at most one image per swapchain waited or in flight, the queue lock only around `vkQueueSubmit` and the swapchain calls, and an image released on a later frame once its fence has signalled. The models may be a frame behind the controllers when the GPU is busy. The files are the MXFM format of my Moonlight XR, posed at the aim pose (`steam-frame/tools/frame-controller-models.py`, made from SteamVR's Frame render models into `user/frame-models`, `AZAHARFRAME_MODELS` to use another folder); they are read and the shaders compiled on a loader thread the first time a model shows.
- **Idle.** A hand that does not move more than a little or press anything for 4 seconds fades its beam and dot, after 30 seconds its model; a tracking loss under 250 ms keeps the last pose (`beam.cpp`, `HandActivity`).

This adds three swapchains (the models' only once they show).

## Immersive mode

CitraVR's trick: the vertex shader divides `gl_Position.xy` by `vr_immersive_mode_factor`, so the game's world is drawn larger on the screen, and the rasterizer multiplies the game's own view matrix register by the inverse of the head pose (`SetVRData` / `ApplyVRDataToPicaVSUniforms`). It guesses where the matrix is, which only works for a few games.

Azahar for Frame feeds the head pose once a frame from the `frame_xr` thread, as an atomic snapshot (`video_core/vr_immersive.h`) that the emulation thread applies after each frame, where CitraVR let the two threads race. The top screen is a quad in view space 0.75 m in front of the head in the Full modes, fitted into the part of the view both eyes see and placed a little low, with its size tied to the game's camera (the **Immersive Game FOV** setting, 60 degrees by default, and the aspect of the **Screen Shape** setting, 5:3 by default) so the world stays put; with an offset of half the distance between your eyes for each eye in the Other Games mode, and a quad fixed in the room in the Wide mode. The grips move and resize it and the placement is saved per game; its pitch goes into the game camera's rotation too (`frame_xr/immersive_layout.h`). Quad layers are drawn in submission order, so the farther screen is submitted first. Immersive mode raises the internal resolution by two steps. The settings are per game in the Qt dialog, with CitraVR's keys (`vr_immersive_mode` and the ones next to it), and the **VR** group of the Graphics tab has them. The heuristics cover Ocarina of Time 3D and Majora's Mask 3D. Everything about the modes, the tuning variables and the differences from CitraVR is in [src/frame_xr/IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md).

In the Other Games mode the game is shown a 3D depth of about 0 (`hid.cpp`, `shared_page.cpp`): the two eyes come from your head, not from the 3DS's slider.

## Shader caches

Azahar's Vulkan renderer writes a disk cache of translated shaders and one of the driver's pipelines for each game, under `user/shaders`. A new game compiles as it draws and loads the cache from then on. The cache can only be made by drawing the game, so there is no ahead-of-time warm-up script the way GalaxyFrame has one.

## Launcher and files

`azaharframe.sh` starts inside `~/.local/share/AzaharFrame`, which holds a `user/` folder, and so Azahar uses it as its portable user directory instead of `~/.local/share/azahar-emu`. It unsets the Mesa overrides and `LD_LIBRARY_PATH` that Steam sets for games, sets `XR_RUNTIME_JSON` to SteamVR's runtime file only if nothing else names a runtime, sets SteamVR's per-app settings, and creates `~/.cache/headset-in-use` while the emulator runs so other work on the Frame waits. It picks the mode (VR or flat) from `--flat` / `--vr`, else `AZAHARFRAME_FLAT`, else the file `mode` in the data folder, and in `user/config/qt-config.ini` it adds the input profile "Flat Gamepad" (SDL gamepad by position, `maptype:all`, so no GUID or port) when it is missing and selects the profile that goes with the mode. Settings for one launch go in `session.env` in the data folder. The environment variables are listed in [TECHNICAL_NOTES.md](TECHNICAL_NOTES.md#environment-variables).

The OpenXR loader is SteamVR's own, `/opt/steamvr/bin/linuxarm64/libopenxr_loader.so`, opened at run time (`$AZAHARFRAME_OPENXR_LOADER` first, the system's loader last); the headers are vendored in `third_party/openxr`. The program links the Qt 6 and SDL2 libraries SteamOS has; OpenAL Soft and Cubeb are built in.
