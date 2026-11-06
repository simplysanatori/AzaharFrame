# Wiring the controller input into the XR core

Everything below runs on the XR thread (the one that calls `xrWaitFrame`) unless noted. Nothing in
this code has been compiled or run yet, so expect a
round of compile fixes.

## 1. CMake

`src/input_common/CMakeLists.txt` already lists `frame_xr.cpp` / `frame_xr.h`, and
`src/input_common/main.cpp` registers the engine (marked `// AzaharFrame`, six small hooks: include,
Register, Unregister, ButtonToText, AnalogToText, pollers).

Add to the `frame_xr` target:

```cmake
    frame_xr/input/actions.cpp
    frame_xr/input/actions.h
    frame_xr/input/controls.h
    frame_xr/input/default_bindings.h
    frame_xr/input/geometry.cpp
    frame_xr/input/geometry.h
    frame_xr/input/input_bridge.cpp
    frame_xr/input/input_bridge.h
    frame_xr/input/input_frame.h
    frame_xr/input/mapper.cpp
    frame_xr/input/mapper.h
    frame_xr/input/types.h
```

and link `frame_xr` against `input_common` (the bridge writes the pad) and the OpenXR headers/loader
(`actions.cpp` includes `<openxr/openxr.h>`). `input_common` only needs `frame_xr/input/*.h` headers
(`controls.h`, `types.h`, `default_bindings.h`), which are header only; `src/` is already on the
include path.

Tests (not my file to edit, `src/tests/CMakeLists.txt`): add

```cmake
    frame_xr/input_engine.cpp
    frame_xr/input_geometry.cpp
    frame_xr/input_mapper.cpp
    ../frame_xr/input/geometry.cpp
    ../frame_xr/input/mapper.cpp
```

to `add_executable(tests ...)` and `target_link_libraries(tests PRIVATE input_common)`. The tests do
not use OpenXR (`actions.cpp` is not compiled into them).

## 2. At start-up

```cpp
#include "frame_xr/input/actions.h"
#include "frame_xr/input/input_bridge.h"
#include "input_common/frame_xr.h"

// xrCreateInstance: enable Actions::kFrameControllerExtension ("XR_VALVE_frame_controller_interaction")
// when xrEnumerateInstanceExtensionProperties lists it.
FrameXr::Input::Actions actions;
actions.Create(instance, has_frame_controller_extension);   // before the session is created or attached

// after xrCreateSession (one attach per session, ever):
actions.Attach(session);          // attaches the action set and creates the two aim spaces
// If you attach other action sets yourself, attach actions.action_set() in the same
// xrAttachSessionActionSets call, then call actions.CreateSpaces(session).

FrameXr::Input::InputBridge bridge;   // owns the Mapper (MapperConfig has the thresholds)
```

`Create` also suggests Oculus Touch bindings as a development fallback (no D-pad, View or bumpers).

## 3. Default VR profile (once, when VR starts)

```cpp
if (!InputCommon::FrameXr::ProfileUsesFrameXr(Settings::values.current_input_profile)) {
    InputCommon::FrameXr::ApplyDefaultProfile(Settings::values.current_input_profile);
    // then make the HID/IR services reload it, as the Qt Controls dialog's Apply does:
    Core::System::GetInstance().ApplySettings();       // core.cpp: ApplySettings() calls hid->ReloadInputDevices()
}
```

`ProfileUsesFrameXr` leaves a profile the user already edited (and saved with frame_xr bindings)
alone. `Settings::values.input_profiles` and `current_input_profile_index` are untouched; save
through the usual config path if the profile should persist. The Controls dialog then edits it
(texts "Frame A", "Frame Menu" and capture by pressing a Frame control work).

## 4. Every display frame

```cpp
// after xrWaitFrame / xrBeginFrame, only while the session state is FOCUSED:
FrameXr::Input::InputFrame in;
if (actions.Sync(session, local_space, frame_state.predictedDisplayTime, in)) {
    FrameXr::Input::PanelGeometry panels;                 // see below
    const FrameXr::Input::MapperOutput& out = bridge.Process(in, panels);
    // out.cursor, out.commands: see below
}
// Not focused, picker showing, or game stopped: bridge.Release();
```

`Process` does the rest: it writes the virtual pad that the emulation thread reads (atomics, no
locking) and sends touch press / move / release to the touch sink. If another part of the XR core already
calls `xrSyncActions`, pass `sync = false` to `Actions::Sync` and include `actions.action_set()` in
that call's active action sets.

### Panel geometry in

`PanelGeometry{top, bottom}`; each `PanelQuad` is `{Posef pose, float width_m, float height_m,
bool visible}` in the same space as the aim poses (LOCAL), i.e. exactly the XrCompositionLayerQuad
pose and size you submit. Facing +Z, as OpenXR quads do. `visible=false` (picker showing, panel
hidden) makes the panel unhittable. Pass the pose you submitted last frame; the commands below
change it for the next one.

### Commands out (`out.commands`)

- `top_drag_active` + `top_drag_target`: the top screen is grabbed. Apply

  ```cpp
  FrameXr::Input::DragLimits limits;          // max_z = -0.5 (CitraVR), set the second field to
  limits.min_distance_from_other = initial_top_to_bottom_distance;   // |top_default - bottom_default|
  if (auto p = FrameXr::Input::SolveSphereDrag(top.pose, out.commands.top_drag_target, viewer,
                                               &bottom.pose.position, limits)) {
      top.pose = *p;
  }
  ```

  `viewer` is (0,0,0) as in CitraVR (LOCAL origin), or the head position if you prefer the sphere
  around the head.
- `top_depth_delta_z`: add to `top.pose.position.z`, clamped:
  `top.pose.position.z = ApplyDepth(top.pose.position.z, delta, bottom.pose.position.z);`
  (this is CitraVR's rule: never nearer than the bottom panel's z, never past -10 m).
- `reset_panels`: put both screens back to their default poses (CitraVR `ResetPanelPositions`). It
  is also the "recentre": place the defaults in front of the current head heading.
- `stop_game`: once per View hold of 1.5 s: queue `ShutdownGame` and show the picker.
  `stop_hold_progress` (0..1, 0 when not holding) is for an on-screen "Hold View to quit" bar.

### Cursor out (`out.cursor`)

`visible`, `position` (world hit point), `orientation` (the panel's), `scale_m` (dot width in
metres), `panel` (Top/Bottom), `px`/`py` (3DS pixels, bottom screen only), `u`/`v` (0..1 on the
panel), `pressed`. Draw a small pre-rendered dot quad; no GPU work on the XR thread beyond the quad
layer.

## 5. Touch sink

```cpp
class VrTouchSink final : public InputCommon::FrameXr::TouchSink {
    void TouchPressed(float x, float y) override  { window.TouchPressed(Map(x, y)...); }
    void TouchMoved(float x, float y) override    { window.TouchMoved(Map(x, y)...); }
    void TouchReleased() override                 { window.TouchReleased(); }
};
InputCommon::FrameXr::SetTouchSink(&sink);   // when VR starts
InputCommon::FrameXr::SetTouchSink(nullptr); // before the sink or the window goes away
```

x, y are the 3DS bottom screen's native pixels (0..320, 0..240). `EmuWindow::TouchPressed(fx, fy)` wants
framebuffer coordinates, so map through the window's layout:

```cpp
const auto& r = window.GetFramebufferLayout().bottom_screen;       // Common::Rectangle<u32>
const unsigned fx = r.left + static_cast<unsigned>(x / 320.0f * r.GetWidth());
const unsigned fy = r.top + static_cast<unsigned>(y / 240.0f * r.GetHeight());
```

`EmuWindow`'s touch calls lock their own mutex, so calling them from the XR thread is fine
(`GRenderWindow` overrides only the Qt event handlers). Pitfall: `TouchPressed` returns false and
`TouchMoved` ignores points outside `layout.bottom_screen`; when the bottom screen is not in the
layout (single screen layout) nothing is touched. Force a layout with a bottom screen while VR runs,
or add a native-pixel touch entry point. `SetTouchSink(nullptr)` waits for a call in flight, so the
sink may be destroyed right after it returns.

## 6. Shutdown / game end

`bridge.Release()` (releases the pad, lifts a touch in progress through the sink). The pad is also
released by `InputCommon::Shutdown()`. `actions.Destroy()` before `xrDestroySession` is not needed
(destroying the session frees the spaces), but destroying the action set afterwards is safe.

## 7. To compile and test once the Frame is free

1. Build `tests` with the extra sources above, run `tests "[frame_xr]"` (about 190 checks over three
   files). Expect a few compile fixes (never compiled).
2. Build `input_common` and `citra_qt`; start Azahar flat: the Controls dialog must still work and
   show no `[unknown]` for frame_xr bindings.
3. With xrmock (`frame_controller` profile is advertised): confirm `xrSuggestInteractionProfileBindings`
   returns success for the Frame profile, `Actions::Sync` finds both aim poses, and the log shows the
   suggested binding lines.
4. Headset: A/B/X/Y, D-pad, grips, triggers as ZL/ZR in a game; touch on the bottom screen at the
   right pixel (check corners); drag and depth on the top screen; reset click; View hold quits.
