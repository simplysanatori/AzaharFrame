# AzaharFrame: the OpenXR frontend. The OpenXR headers are vendored
# (third_party/openxr, Khronos, Apache-2.0 OR MIT); the loader is opened at run
# time (SteamVR's, frame_xr/xr_loader.cpp), so nothing here links to OpenXR.
add_library(frame_xr STATIC EXCLUDE_FROM_ALL
    app.cpp
    controls.cpp
    dynamic_resolution.cpp # AzaharFrame menuux
    dynamic_resolution.h
    frame_xr.h
    image_lag.h # AzaharFrame imglag
    immersive.cpp
    immersive.h
    immersive_layout.cpp
    immersive_layout.h
    ipd_smoother.h # AzaharFrame ipd
    hud.cpp # AzaharFrame vrqol
    hud.h # AzaharFrame vrqol
    presets.cpp # AzaharFrame vrqol
    controls_presets.cpp # AzaharFrame kiu
    controls_presets.h # AzaharFrame reticle
    presets.h # AzaharFrame vrqol
    flat_fit.h # AzaharFrame flatdialog
    overlays.cpp
    pacing.cpp # AzaharFrame pacing
    pacing.h
    session.cpp
    session.h
    toast_depth.h # AzaharFrame kiu2
    screen_handover.h # AzaharFrame flash3
    toast_fit.h # AzaharFrame menuux
    ui_sound.cpp # AzaharFrame menuux
    ui_sound.h
    ui_sound_wav.cpp
    vulkan_bridge.cpp
    vulkan_bridge.h
    watchdog.cpp
    watchdog.h
    xr_calls.h
    xr_includes.h
    xr_loader.cpp
    xr_loader.h
    # Controller input (input): actions, mapping, bridge to input_common.
    input/actions.cpp
    input/actions.h
    input/controls.h
    input/default_bindings.h
    input/devices.cpp # AzaharFrame pads
    input/devices.h
    input/gamepads.cpp
    input/gamepads.h
    input/key_device.cpp # AzaharFrame keys
    input/key_device.h
    input/key_names.h
    input/mouse.cpp # AzaharFrame padsui
    input/mouse.h
    input/mouse_pointer.cpp # AzaharFrame padsui: adapted from Frametop (MIT)
    input/mouse_pointer.h
    input/phys_keyboard.cpp # AzaharFrame input2
    input/phys_keyboard.h
    input/reticle.cpp # AzaharFrame mouse
    input/reticle.h
    input/profile_routes.h
    input/click_gesture.h
    input/geometry.cpp
    input/geometry.h
    input/input_bridge.cpp
    input/input_bridge.h
    input/input_frame.h
    input/list_nav.h
    input/chord_latch.h
    input/flat_pads.h # AzaharFrame flat
    input/flat_vr_hold.h # AzaharFrame flat
    instance_policy.h # AzaharFrame flat2
    input/mapper.cpp
    input/mapper.h
    input/motion.cpp
    input/motion.h
    input/gyro_aim.cpp # AzaharFrame kiugyro
    input/gyro_aim.h
    kiu_probe.cpp # AzaharFrame kiugyro
    kiu_probe.h
    kiu_scan.cpp
    kiu_scan.h
    kiu_weapon.cpp # AzaharFrame kiuguns
    kiu_weapon.h
    input/screen_rig.cpp
    input/screen_rig.h
    input/screen_view.cpp
    input/screen_view.h
    input/touchpad_touch.cpp # AzaharFrame touchpad
    input/touchpad_touch.h
    input/types.h
    # The Qt window mirrored as the VR home panel, XR side (qt_mirror; no Qt here).
    qt_mirror/mirror_panel.cpp
    qt_mirror/mirror_panel.h
    # The headset keyboard under the home panel (keyboard; no Qt here).
    keyboard/art.cpp
    keyboard/art.h
    keyboard/keyboard.h
    keyboard/keyboard_panel.cpp
    keyboard/keyboard_panel.h
    keyboard/layout.cpp
    keyboard/layout.h
    keyboard/stb_truetype.h # its implementation is built into frame_xr_ui (AzaharFrame tabmenu)
    # AzaharFrame vrvis: controls help, stats, beams, controller models (vis; no Qt here).
    menu_panel.cpp # AzaharFrame tabmenu
    menu_panel.h
    per_game.cpp # AzaharFrame useglobal
    per_game.h
    vis/async_overlay.cpp
    vis/async_overlay.h
    vis/autosave_label.cpp # AzaharFrame autosave
    vis/autosave_label.h
    auto_shape.cpp # AzaharFrame fixes7
    auto_shape.h
    vis/shader_badge.cpp # AzaharFrame shaderbadge
    vis/shader_badge.h
    vis/beam.cpp
    vis/beam.h
    vis/controller_diagram.cpp
    vis/controller_diagram.h
    vis/controller_models.cpp
    vis/controller_models.h
    vis/help_content.cpp
    vis/help_content.h
    vis/help_layout.cpp
    vis/help_layout.h
    vis/model_file.cpp
    vis/model_file.h
    vis/model_plane.h # AzaharFrame ctrlfront
    vis/model_shaders.h
    vis/weapon_fit.cpp # AzaharFrame kiuguns
    vis/weapon_fit.h
    vis/muzzle_flash.cpp # AzaharFrame kiuflash
    vis/muzzle_flash.h
    vis/panel_art.cpp
    vis/panel_art.h
    vis/panels.cpp
    vis/panels.h
    vis/stats_place.cpp # AzaharFrame statspos
    vis/stats_place.h
    vis/menu_diagram.cpp # AzaharFrame tabmenu
    vis/menu_diagram.h # AzaharFrame tabmenu
    vis/visuals.cpp
    vis/visuals.h
    vis/layer_plan.h # AzaharFrame vis
    vis/proj_screen.h # AzaharFrame projscreen
    vis/proj_screen_layer.h
)
# AzaharFrame tabmenu: the in-game menu (Material 3 on the UI canvas).
add_subdirectory(menu)

create_target_directory_groups(frame_xr)

# The in-VR application picker and its UI canvas (picker).
add_subdirectory(picker)

# Release packages (steam-frame/build.sh) must not carry this machine's checkout path:
# drop the picker's development fallback to <checkout>/steam-frame/art.
option(AZAHARFRAME_RELEASE "Build for a release package" OFF)
if (AZAHARFRAME_RELEASE AND TARGET frame_xr_picker)
    get_target_property(picker_defs frame_xr_picker COMPILE_DEFINITIONS)
    if (picker_defs)
        list(FILTER picker_defs EXCLUDE REGEX "^AZAHARFRAME_SOURCE_ART=")
        set_target_properties(frame_xr_picker PROPERTIES COMPILE_DEFINITIONS "${picker_defs}")
    endif()
endif()

target_include_directories(frame_xr PUBLIC ${PROJECT_SOURCE_DIR}/third_party/openxr/include)
target_link_libraries(frame_xr PUBLIC citra_common citra_core video_core input_common vulkan-headers vma)
target_link_libraries(frame_xr PRIVATE frame_xr_picker ${CMAKE_DL_LIBS} Threads::Threads)
target_link_libraries(frame_xr PUBLIC frame_xr_menu) # AzaharFrame tabmenu
# AzaharFrame menuux: Edit Controls' click plays through SDL's audio (ui_sound.cpp).
if (ENABLE_SDL2)
    target_link_libraries(frame_xr PRIVATE SDL2::SDL2)
    target_compile_definitions(frame_xr PRIVATE HAVE_SDL2)
endif()

# AzaharFrame kbd: the game's keyboard (the 3DS software keyboard applet) in VR.
target_sources(frame_xr PRIVATE
    swkbd.h
    swkbd_config.cpp
    swkbd_config.h
    swkbd_panel.cpp
    swkbd_panel.h
)

# AzaharFrame tabmenu: renders the menu's tabs to PNG files without a headset. Built only when
# asked for (ninja frame_xr_menu_dump), never part of a release.
# The diagram's sources are compiled in (as the unit tests do), so it needs no core libraries.
add_executable(frame_xr_menu_dump EXCLUDE_FROM_ALL
    menu/menu_dump.cpp
    menu/texparts_dump.cpp # AzaharFrame texparts: the Customize screens, dump only
    menu/texparts_dump.h
    vis/controller_diagram.cpp
    vis/help_content.cpp
    vis/help_layout.cpp
    vis/menu_diagram.cpp
    vis/model_file.cpp
)
target_include_directories(frame_xr_menu_dump PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(frame_xr_menu_dump PRIVATE frame_xr_menu lodepng citra_common)

# AzaharFrame kbd: renders the game's keyboard to PNG files without a headset (ninja
# frame_xr_swkbd_dump), never part of a release.
add_executable(frame_xr_swkbd_dump EXCLUDE_FROM_ALL menu/swkbd_dump.cpp)
target_include_directories(frame_xr_swkbd_dump PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(frame_xr_swkbd_dump PRIVATE frame_xr_menu lodepng)
