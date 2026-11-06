# CitraFrame in-VR application picker.
#
#   frame_xr_picker_core  library scan + icon cache + picker state/drawing. No emulator
#                         dependencies, so it is unit-tested on its own.
#   frame_xr_picker       adds the Loader-backed metadata reader (needs citra_core).
#
# Link frame_xr_picker into the XR code; see INTEGRATION.md.

if (NOT TARGET frame_xr_ui)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ui ${CMAKE_CURRENT_BINARY_DIR}/ui)
endif()

find_package(Threads REQUIRED)

add_library(frame_xr_picker_core STATIC
    library.cpp
    library.h
    picker.cpp
    picker.h
)
target_link_libraries(frame_xr_picker_core PUBLIC frame_xr_ui Threads::Threads)
target_compile_features(frame_xr_picker_core PUBLIC cxx_std_20)

add_library(frame_xr_picker STATIC
    library_loader.cpp
    library_loader.h
)
target_link_libraries(frame_xr_picker PUBLIC frame_xr_picker_core PRIVATE citra_core citra_common)

if (ENABLE_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/frame_xr/CMakeLists.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tests/frame_xr ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
