# Picker integration (for the XR core)

The picker is plain C++20 with no XR, Qt or Vulkan includes. Everything is in
`namespace frame_xr::picker` (UI canvas in `frame_xr::ui`).

## CMake

```cmake
# in src/frame_xr/CMakeLists.txt (needs the emulator's include root, i.e. src/)
add_subdirectory(picker)            # also pulls in ../ui when frame_xr_ui is not defined yet
target_link_libraries(frame_xr PRIVATE frame_xr_picker)   # or whatever the XR target is called
```

Targets:

| Target | Sources | Needs |
|---|---|---|
| `frame_xr_ui` | `ui/ui_canvas.{h,cpp}`, `ui/ui_font.h` | nothing |
| `frame_xr_picker_core` | `picker/library.{h,cpp}`, `picker/picker.{h,cpp}` | `frame_xr_ui`, Threads |
| `frame_xr_picker` | `picker/library_loader.{h,cpp}` + core | `citra_core`, `citra_common` |

If you prefer to list sources in your own target instead: the seven `.cpp` files
above, with `src/` on the include path (includes are written `frame_xr/picker/...`).

Unit tests (not built unless `ENABLE_TESTS`): `src/tests/frame_xr/`, target
`frame_xr_picker_tests`; `picker/CMakeLists.txt` adds them itself, no edit to
`src/tests/CMakeLists.txt` is needed. They need only `frame_xr_picker_core` and `catch2`.

## Calls

```cpp
#include "frame_xr/picker/library.h"
#include "frame_xr/picker/library_loader.h"
#include "frame_xr/picker/picker.h"
using namespace frame_xr::picker;

// 1. Create (any thread; starts one idle worker thread). The icon cache lives in
//    ~/.cache/azaharframe/icons (XDG_CACHE_HOME honoured).
auto library = std::make_shared<Library>(ReadMetadataWithLoader, DefaultIconCacheDir());
Picker picker(library);          // 1536 x 1024 texture, fonts decoded here

// 2. Folders, from the Qt side (GMainWindow thread). UISettings::GameDir has
//    path / deep_scan. Skip the "INSTALLED" and "SYSTEM" pseudo entries (or map INSTALLED to
//    the two sdmc title folders with GameDir{path, false, /*max_depth=*/2}).
std::vector<GameDir> dirs;
for (const auto& d : UISettings::values.game_dirs) {
    if (d.path != QStringLiteral("INSTALLED") && d.path != QStringLiteral("SYSTEM"))
        dirs.push_back({d.path.toStdString(), d.deep_scan});
}
library->SetDirs(std::move(dirs));
library->Rescan();               // never blocks; call on startup and whenever game_dirs change
```

`SetDirs` is thread safe, so it can be called from Qt while the XR thread runs
`picker.Update`. Re-run `SetDirs` + `Rescan` when the game list directories change in the
settings dialog (the picker's own Rescan button re-scans the folders last given).

Per frame, on the XR thread, only while the picker is showing:

```cpp
PickerInput in;
in.hover  = hit_on_picker_quad ? std::optional<Vec2>({u, v}) : std::nullopt;
in.select = trigger_or_A_went_down;   // edge, not level
in.back   = B_went_down;              // edge
in.scroll = right_or_left_stick_y;    // -1..1, dead zone already applied, +1 = next page
in.dt     = seconds_since_last_update;
PickerResult r = picker.Update(in);

if (r.dirty)        // upload picker.Pixels() (picker.PixelBytes() bytes) to the quad swapchain
    UploadRGBA(picker.Pixels(), picker.Width(), picker.Height());
if (r.launch_path)  // hand to Qt, see below
    ...
if (r.back_at_root) // B pressed on page 1: e.g. nothing, or leave VR
    ...
```

- **UV convention**: `u` to the right, `v` downwards, both 0..1 over the whole quad;
  (0,0) is the top-left texel. For a quad centred on its pose with width 1.6 m and height
  1.6 * 1024 / 1536 = 1.0667 m: `u = x_local / 1.6 + 0.5`, `v = 0.5 - y_local / 1.0667`
  (local x right, y up), and drop the hit when u or v is outside 0..1.
- **Pixels**: `uint8_t[1536*1024*4]`, R,G,B,A in memory order, rows top down, fully
  opaque. The colours are sRGB values, so use `VK_FORMAT_R8G8B8A8_SRGB` for the swapchain (or
  UNORM if the layer is composited without conversion and the look is acceptable). Upload only
  when `dirty` is set; also call `picker.Invalidate()` after showing the picker again or
  recreating the swapchain, then upload on the next Update.
- **Quad size**: 1.6 m x 1.0667 m, 2 m ahead. A texel is about 1.04 mm; the smallest text
  (28 px) is about 2.9 cm tall at 2 m.
- `Update` is cheap when nothing changed (no drawing, no allocation). A redraw is a few
  milliseconds of CPU work, so keep it on the XR thread's non-critical part or a worker.
- `picker.Update` pulls new library results itself (via `library->Generation()`).
- Select on a tile never touches the library; select on the Rescan button calls
  `library->Rescan()`. Do not let the picker run (or call `Rescan`) while a game is running: the
  file reads compete with emulation. Hide the quad and stop calling `Update`.

## Launching

`launch_path` is an absolute path in the same form the Qt game list uses. Hand it to the
main window on the Qt thread:

```cpp
QMetaObject::invokeMethod(main_window, [main_window, path = QString::fromStdString(*r.launch_path)] {
    main_window->BootGame(path);
}, Qt::QueuedConnection);
```

(`BootGame` is private in `GMainWindow`; call it from a lambda inside a member function, or
emit the existing `OnGameListLoadFile(QString)` slot via `QMetaObject::invokeMethod(main_window, "OnGameListLoadFile", Qt::QueuedConnection, Q_ARG(QString, path))`.)
Ignore further `launch_path` results until the game has started, or the user can double
launch with a second trigger pull.

## Show and hide

- Game starts (`EmuThread` running / `BootGame` done): hide the picker quad, stop calling
  `Update`, show the screen quads.
- Game stops (`ShutdownGame` finished, including the View-hold-1.5-s exit): show the picker
  quad, `picker.Invalidate()`, optionally `library->Rescan()` (cheap: icons come from the
  cache, only changed files are re-read), resume calling `Update`.
- The picker keeps its page and hover state across hide/show.

## Notes for the Loader-backed reader

`ReadMetadataWithLoader` mirrors `GameListWorker::AddFstEntriesToGameList`: same
extensions (`SupportedExtensions()` copies `GameList::supported_file_extensions`; CIA files
are not in Azahar's list either), skips non-executable files, hides system titles without the
SMDH visible flag, title from `ReadTitle(title, true)` (system language), large 48x48 SMDH
icon converted from RGB565. It runs on the library worker thread, like Azahar's own worker.
Titles outside Latin-1/ASCII show as `?` (the baked Inter atlas has ASCII only).

## UI text

All strings are in `picker.cpp` (`Draw`, `Plural`, `DrawButton` calls). Title Case.
