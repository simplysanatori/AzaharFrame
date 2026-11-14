# Unit tests for the in-VR picker (frame_xr/picker, frame_xr/ui). Pulled in by
# frame_xr/picker/CMakeLists.txt when ENABLE_TESTS is on. No emulator core needed.
add_executable(frame_xr_picker_tests
    picker_library.cpp
    picker_layout.cpp
    tab_menu.cpp # AzaharFrame tabmenu
    mods_rows.cpp # AzaharFrame fixes7
    remap.cpp # AzaharFrame remap
    cheat_entry.cpp # AzaharFrame ui2
    list_scroll.cpp # AzaharFrame scroll
    utf8_text.cpp # AzaharFrame utf8
    homebrew_uninstall.cpp # AzaharFrame emhb
    angels_aim_menu.cpp # AzaharFrame kiufix
)
target_link_libraries(frame_xr_picker_tests PRIVATE frame_xr_picker_core frame_xr_menu catch2)
add_test(NAME frame_xr_picker_tests COMMAND frame_xr_picker_tests)
