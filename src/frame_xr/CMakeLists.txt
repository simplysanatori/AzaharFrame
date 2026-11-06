# CitraFrame: the OpenXR frontend. The OpenXR headers are vendored
# (third_party/openxr, Khronos, Apache-2.0 OR MIT); the loader is opened at run
# time (SteamVR's, frame_xr/xr_loader.cpp), so nothing here links to OpenXR.
add_library(frame_xr STATIC EXCLUDE_FROM_ALL
    app.cpp
    frame_xr.h
    session.cpp
    session.h
    vulkan_bridge.cpp
    vulkan_bridge.h
    xr_includes.h
    xr_loader.cpp
    xr_loader.h
)

create_target_directory_groups(frame_xr)

target_include_directories(frame_xr PUBLIC ${PROJECT_SOURCE_DIR}/third_party/openxr/include)
target_link_libraries(frame_xr PUBLIC citra_common citra_core video_core vulkan-headers vma)
target_link_libraries(frame_xr PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
