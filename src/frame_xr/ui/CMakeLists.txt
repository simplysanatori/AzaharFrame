# Azahar for Frame in-VR UI canvas: CPU rasteriser, baked bitmap font and TrueType text
# (AzaharFrame tabmenu: stb_truetype, vendored in ../keyboard, is built here). No dependencies.
add_library(frame_xr_ui STATIC
    ui_canvas.cpp
    ui_canvas.h
    ui_font.h
    ui_ttf.cpp
    ui_ttf.h
    ../keyboard/stb_truetype.cpp
)
# stb_truetype is vendored unchanged (MIT or public domain): its warnings are not ours.
set_source_files_properties(../keyboard/stb_truetype.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_include_directories(frame_xr_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_compile_features(frame_xr_ui PUBLIC cxx_std_20)
