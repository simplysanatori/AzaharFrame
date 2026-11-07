# AzaharFrame: the OpenXR frontend. The OpenXR headers are vendored
# (third_party/openxr, Khronos, Apache-2.0 OR MIT); the loader is opened at run
# time (SteamVR's, frame_xr/xr_loader.cpp), so nothing here links to OpenXR.
add_library(frame_xr STATIC EXCLUDE_FROM_ALL
    app.cpp
    controls.cpp
    frame_xr.h
    immersive.cpp
    immersive.h
    overlays.cpp
    session.cpp
    session.h
    vulkan_bridge.cpp
    vulkan_bridge.h
    xr_calls.h
    xr_includes.h
    xr_loader.cpp
    xr_loader.h
    # Controller input (input): actions, mapping, bridge to input_common.
    input/actions.cpp
    input/actions.h
    input/controls.h
    input/default_bindings.h
    input/geometry.cpp
    input/geometry.h
    input/input_bridge.cpp
    input/input_bridge.h
    input/input_frame.h
    input/mapper.cpp
    input/mapper.h
    input/motion.cpp
    input/motion.h
    input/types.h
)

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
