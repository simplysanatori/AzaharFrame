# Unit tests for the in-VR picker (frame_xr/picker, frame_xr/ui). Pulled in by
# frame_xr/picker/CMakeLists.txt when ENABLE_TESTS is on. No emulator core needed.
add_executable(frame_xr_picker_tests
    picker_library.cpp
    picker_layout.cpp
)
target_link_libraries(frame_xr_picker_tests PRIVATE frame_xr_picker_core catch2)
add_test(NAME frame_xr_picker_tests COMMAND frame_xr_picker_tests)
