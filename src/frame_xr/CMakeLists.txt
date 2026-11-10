# AzaharFrame: the OpenXR frontend. The OpenXR headers are vendored
# (third_party/openxr, Khronos, Apache-2.0 OR MIT); the loader is opened at run
# time (SteamVR's, frame_xr/xr_loader.cpp), so nothing here links to OpenXR.
add_library(frame_xr STATIC EXCLUDE_FROM_ALL
    app.cpp
    controls.cpp
    frame_xr.h
    immersive.cpp
    immersive.h
    immersive_layout.cpp
    immersive_layout.h
    hud.cpp # AzaharFrame vrqol
    hud.h # AzaharFrame vrqol
    presets.cpp # AzaharFrame vrqol
    presets.h # AzaharFrame vrqol
    overlays.cpp
    session.cpp
    session.h
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
    input/click_gesture.h
    input/geometry.cpp
    input/geometry.h
    input/input_bridge.cpp
    input/input_bridge.h
    input/input_frame.h
    input/chord_latch.h
    input/mapper.cpp
    input/mapper.h
    input/motion.cpp
    input/motion.h
    input/screen_rig.cpp
    input/screen_rig.h
    input/screen_view.cpp
    input/screen_view.h
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
    vis/async_overlay.cpp
    vis/async_overlay.h
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
    vis/model_shaders.h
    vis/panel_art.cpp
    vis/panel_art.h
    vis/panels.cpp
    vis/panels.h
    vis/menu_diagram.cpp # AzaharFrame tabmenu
    vis/menu_diagram.h # AzaharFrame tabmenu
    vis/visuals.cpp
    vis/visuals.h
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

# AzaharFrame tabmenu: renders the menu's tabs to PNG files without a headset. Built only when
# asked for (ninja frame_xr_menu_dump), never part of a release.
# The diagram's sources are compiled in (as the unit tests do), so it needs no core libraries.
add_executable(frame_xr_menu_dump EXCLUDE_FROM_ALL
    menu/menu_dump.cpp
    vis/controller_diagram.cpp
    vis/help_content.cpp
    vis/help_layout.cpp
    vis/menu_diagram.cpp
    vis/model_file.cpp
)
target_include_directories(frame_xr_menu_dump PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(frame_xr_menu_dump PRIVATE frame_xr_menu lodepng citra_common)
