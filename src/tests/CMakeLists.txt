add_executable(tests
    frame_xr/input_engine.cpp # AzaharFrame
    frame_xr/input_geometry.cpp # AzaharFrame
    frame_xr/input_mapper.cpp # AzaharFrame
    frame_xr/screen_rig.cpp # AzaharFrame vrsize
    frame_xr/screen_view.cpp # AzaharFrame screenswap
    frame_xr/keyboard_layout.cpp # AzaharFrame
    frame_xr/presets.cpp # AzaharFrame vrqol
    ../frame_xr/presets.cpp # AzaharFrame vrqol
    ../frame_xr/input/geometry.cpp # AzaharFrame
    ../frame_xr/input/mapper.cpp # AzaharFrame
    ../frame_xr/input/screen_rig.cpp # AzaharFrame vrsize
    ../frame_xr/input/screen_view.cpp # AzaharFrame screenswap
    ../frame_xr/keyboard/layout.cpp # AzaharFrame
    ../frame_xr/input/motion.cpp # AzaharFrame
    ../frame_xr/immersive_layout.cpp # AzaharFrame
    frame_xr/immersive_layout.cpp # AzaharFrame
    frame_xr/input_motion.cpp # AzaharFrame
    ../frame_xr/vis/beam.cpp # AzaharFrame vrvis
    ../frame_xr/vis/controller_diagram.cpp # AzaharFrame ctrldiag
    ../frame_xr/vis/help_content.cpp # AzaharFrame vrvis
    ../frame_xr/vis/help_layout.cpp # AzaharFrame screenswap
    ../frame_xr/vis/model_file.cpp # AzaharFrame vrvis
    frame_xr/vis.cpp # AzaharFrame vrvis
    frame_xr/xr_handoff.cpp # AzaharFrame handoff
    frame_xr/flat_fit.cpp # AzaharFrame flatdialog
    frame_xr/list_nav.cpp # AzaharFrame gamelist
    common/bit_field.cpp
    common/file_util.cpp
    common/param_package.cpp
    common/slot_vector.cpp
    core/core_timing.cpp
    core/file_sys/path_parser.cpp
    core/hle/kernel/hle_ipc.cpp
    core/memory/memory.cpp
    core/memory/vm_manager.cpp
    precompiled_headers.h
    audio_core/hle/hle.cpp
    audio_core/hle/source.cpp
    audio_core/lle/lle.cpp
    audio_core/audio_fixures.h
    audio_core/decoder_tests.cpp
    video_core/pica_types.cpp
    video_core/shader.cpp
    audio_core/merryhime_3ds_audio/merry_audio/merry_audio.cpp
    audio_core/merryhime_3ds_audio/merry_audio/merry_audio.h
    audio_core/merryhime_3ds_audio/merry_audio/service_fixture.cpp
    audio_core/merryhime_3ds_audio/merry_audio/service_fixture.h
    audio_core/merryhime_3ds_audio/audio_test_biquad_filter.cpp
)

create_target_directory_groups(tests)

if (BSD STREQUAL "NetBSD")
    include(DisablePaxMprotect)
    disable_pax_mprotect(tests)
endif()

target_link_libraries(tests PRIVATE citra_common citra_core video_core audio_core)
target_link_libraries(tests PRIVATE input_common) # AzaharFrame
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} catch2 nihstro-headers Threads::Threads)

if (ENABLE_LIBRETRO)
    target_link_libraries(tests PRIVATE $<TARGET_OBJECTS:citra_libretro_common>)
endif()

add_test(NAME tests COMMAND tests)
if(NOT ANDROID AND (CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME))
    catch_discover_tests(tests)
endif()


if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(tests PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (MSVC)
    include(BundleTarget)
    bundle_target_in_place(tests)
endif()
