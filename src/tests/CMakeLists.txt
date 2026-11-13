add_executable(tests
    frame_xr/input_engine.cpp # AzaharFrame
    frame_xr/input_geometry.cpp # AzaharFrame
    frame_xr/input_mapper.cpp # AzaharFrame
    frame_xr/screen_rig.cpp # AzaharFrame vrsize
    frame_xr/screen_view.cpp # AzaharFrame screenswap
    frame_xr/touchpad_touch.cpp # AzaharFrame touchpad
    frame_xr/gamepads.cpp # AzaharFrame pads
    frame_xr/mouse.cpp # AzaharFrame padsui
    frame_xr/reticle.cpp # AzaharFrame mouse
    frame_xr/keyboard_layout.cpp # AzaharFrame
    frame_xr/presets.cpp # AzaharFrame vrqol
    frame_xr/shader_notice.cpp # AzaharFrame stall
    ../frame_xr/presets.cpp # AzaharFrame vrqol
    ../frame_xr/controls_presets.cpp # AzaharFrame kiu
    ../frame_xr/input/geometry.cpp # AzaharFrame
    ../frame_xr/input/mapper.cpp # AzaharFrame
    ../frame_xr/input/screen_rig.cpp # AzaharFrame vrsize
    ../frame_xr/input/screen_view.cpp # AzaharFrame screenswap
    ../frame_xr/input/touchpad_touch.cpp # AzaharFrame touchpad
    ../frame_xr/input/gamepads.cpp # AzaharFrame pads
    ../frame_xr/input/mouse.cpp # AzaharFrame padsui
    ../frame_xr/input/key_device.cpp # AzaharFrame keys
    frame_xr/keys.cpp # AzaharFrame keys
    ../frame_xr/input/mouse_pointer.cpp # AzaharFrame padsui
    ../frame_xr/input/reticle.cpp # AzaharFrame mouse
    ../frame_xr/input/phys_keyboard.cpp # AzaharFrame input2
    frame_xr/phys_keyboard.cpp # AzaharFrame input2
    ../frame_xr/keyboard/layout.cpp # AzaharFrame
    ../frame_xr/input/motion.cpp # AzaharFrame
    ../frame_xr/immersive_layout.cpp # AzaharFrame
    frame_xr/immersive_layout.cpp # AzaharFrame
    frame_xr/immersive_scale.cpp # AzaharFrame kiu
    frame_xr/ipd.cpp # AzaharFrame ipd
    frame_xr/stereo.cpp # AzaharFrame stereo
    frame_xr/log_gate.cpp # AzaharFrame integ8
    frame_xr/input_motion.cpp # AzaharFrame
    ../frame_xr/vis/beam.cpp # AzaharFrame vrvis
    ../frame_xr/vis/controller_diagram.cpp # AzaharFrame ctrldiag
    ../frame_xr/vis/help_content.cpp # AzaharFrame vrvis
    ../frame_xr/vis/help_layout.cpp # AzaharFrame screenswap
    ../frame_xr/vis/model_file.cpp # AzaharFrame vrvis
    ../frame_xr/vis/stats_place.cpp # AzaharFrame statspos
    frame_xr/vis.cpp # AzaharFrame vrvis
    frame_xr/xr_handoff.cpp # AzaharFrame handoff
    frame_xr/flat_fit.cpp # AzaharFrame flatdialog
    frame_xr/mem_guard.cpp # AzaharFrame modfix
    frame_xr/crash_report.cpp # AzaharFrame crashreport
    frame_xr/preload_budget.cpp # AzaharFrame memguard
    frame_xr/mem_watch.cpp # AzaharFrame memwarn
    frame_xr/startup_escape.cpp # AzaharFrame memguard
    frame_xr/list_nav.cpp # AzaharFrame gamelist
    frame_xr/flat_pads.cpp # AzaharFrame flat
    frame_xr/flat_vr_hold.cpp # AzaharFrame flat
    frame_xr/instance_policy.cpp # AzaharFrame flat2
    frame_xr/swkbd.cpp # AzaharFrame kbd
    ../frame_xr/menu/swkbd_model.cpp # AzaharFrame kbd
    ../frame_xr/swkbd_config.cpp # AzaharFrame kbd
    frame_xr/dynamic_resolution.cpp # AzaharFrame menuux
    ../frame_xr/dynamic_resolution.cpp # AzaharFrame menuux
    frame_xr/ui_sound.cpp # AzaharFrame menuux
    ../frame_xr/ui_sound_wav.cpp # AzaharFrame menuux
    frame_xr/toast_fit.cpp # AzaharFrame menuux
    frame_xr/toast_depth.cpp # AzaharFrame kiu2
    common/bit_field.cpp
    common/file_util.cpp
    common/param_package.cpp
    common/slot_vector.cpp
    common/xr_pacer.cpp # AzaharFrame pacing
    core/core_timing.cpp
    core/sharkive.cpp # AzaharFrame cheats
    core/game_version.cpp # AzaharFrame versions
    core/cheats_online.cpp # AzaharFrame cheats
    core/mods_library.cpp # AzaharFrame moddata
    core/mods_mega.cpp # AzaharFrame mega
    core/mods_catalog3.cpp # AzaharFrame catalog3
    core/mods_running.cpp # AzaharFrame catalog3
    core/mods_compat.cpp # AzaharFrame compat
    core/harden.cpp # AzaharFrame harden
    core/mods_transfer.cpp # AzaharFrame fixes7
    core/cpp_helpers.cpp # AzaharFrame cpphelpers
    core/emerald_fallback.cpp # AzaharFrame emfallback
    core/custom_tex_packs.cpp # AzaharFrame texdata
    core/custom_tex_download.cpp # AzaharFrame texdl
    core/custom_tex_parts.cpp # AzaharFrame texparts
    core/custom_tex_layers.cpp # AzaharFrame texlayers
    core/pack_config_json.cpp # AzaharFrame jsoncrash
    core/custom_tex_optimize.cpp # AzaharFrame ui3
    core/custom_tex_convert.cpp # AzaharFrame cppconv
    frame_xr/zip_install.cpp # AzaharFrame zipinstall
    core/homebrew_emerald3ds.cpp # AzaharFrame emerald
    core/homebrew_update_check.cpp # AzaharFrame ui3
    core/savestate_frame.cpp # AzaharFrame savestate
    core/autosave_frame.cpp # AzaharFrame autosave
    frame_xr/autosave_label.cpp # AzaharFrame autosave
    frame_xr/use_global.cpp # AzaharFrame useglobal
    ../frame_xr/per_game.cpp # AzaharFrame useglobal
    ../frame_xr/vis/autosave_label.cpp # AzaharFrame autosave
    ../frame_xr/vis/shader_badge.cpp # AzaharFrame shaderbadge
    frame_xr/shader_badge.cpp # AzaharFrame shaderbadge
    ../frame_xr/auto_shape.cpp # AzaharFrame fixes7
    frame_xr/auto_shape.cpp # AzaharFrame fixes7
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
    audio_core/mic_queue_tests.cpp # AzaharFrame fixes11
    audio_core/ff_audio_tests.cpp # AzaharFrame ffaudio, ffspeed
    audio_core/rate_trim.cpp # AzaharFrame pacing
    audio_core/dsp_output_tests.cpp # AzaharFrame pacing + ffaudio
    video_core/pica_types.cpp
    video_core/shader.cpp
    video_core/right_eye_disabler.cpp # AzaharFrame feshimmer
    video_core/transfer_overlap.cpp # AzaharFrame feshimmer
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
target_link_libraries(tests PRIVATE httplib) # AzaharFrame crashreport: Send Report to 127.0.0.1
target_link_libraries(tests PRIVATE json-headers) # AzaharFrame jsoncrash: pack_config.h; emfallback: the recipe parser header
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} catch2 nihstro-headers Threads::Threads)

if (ENABLE_LIBRETRO)
    target_link_libraries(tests PRIVATE $<TARGET_OBJECTS:citra_libretro_common>)
endif()

# AzaharFrame cppconv: the texture pack converter as a program, for the parity check against the
# old script and for timing (the app runs the same code in-process; not shipped).
add_executable(af_texconv core/texconv_cli.cpp)
target_link_libraries(af_texconv PRIVATE citra_common citra_core Threads::Threads)

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
