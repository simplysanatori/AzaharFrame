# CitraFrame in-VR UI canvas: CPU rasteriser and baked bitmap font. No dependencies.
add_library(frame_xr_ui STATIC
    ui_canvas.cpp
    ui_canvas.h
    ui_font.h
)
target_include_directories(frame_xr_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_compile_features(frame_xr_ui PUBLIC cxx_std_20)
