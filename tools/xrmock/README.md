# xrmock

A mock OpenXR runtime for testing Azahar for Frame's XR code without the headset,
and `../mockrun.sh`, which runs the emulator against it. Ported from
GalaxyFrame's `tools/xrmock` (Unlicense); the OpenXR headers in `include/` are
Khronos's (Apache-2.0 or MIT).

## Build

`tools/xrmock/build.sh [out dir]` builds `libxrmock.so`, `runtime.json` and the
test program `xrmock_test` into `~/Games/AzaharFrame/build-xrmock` (or the out
dir). It needs a C compiler, the Vulkan headers (found in the repo's
`externals/vulkan-headers`, or set `VULKAN_INCLUDE`) and nothing of the
emulator's build. Select the runtime with `XR_RUNTIME_JSON=<out>/runtime.json`.

Both `build.sh` and `mockrun.sh` first wait while `~/.cache/headset-in-use`
exists, `lasrecomp` runs (or, for `mockrun.sh`, a Steam game does), then take
`~/.cache/af-bench-lock` for the length of the build or run only.

## What it offers

- `XR_KHR_vulkan_enable2` (instance and device made through the mock with the
  app's loader; Vulkan 1.0 to 1.2 as SteamVR), `XR_VALVE_frame_controller_interaction`,
  `XR_FB_display_refresh_rate`, `XR_EXT_performance_settings`,
  `XR_EXT_eye_gaze_interaction`, `XR_KHR_composition_layer_depth`,
  `XR_KHR_composition_layer_cylinder` (as SteamVR); core quad and projection layers, and
  cylinder layers, which are checked as quads and logged with their radius, central angle
  and aspect. There is no passthrough, as on the Frame.
- Swapchains of 3 images (`R8G8B8A8_SRGB` and the other formats SteamVR offers),
  handed out in `COLOR_ATTACHMENT_OPTIMAL`. A swapchain with mip levels, array
  layers or samples is an error.
- A frame clock (`MOCK_XR_HZ`, default 120), session state events, LOCAL, STAGE
  and VIEW spaces (LOCAL begins at the head, 1.6 m up), the head turning
  slowly about the vertical axis, two eyes 64 mm apart.
- Real action state: actions and bindings are recorded, the profile is
  `/interaction_profiles/valve/frame_controller` once it is suggested, an
  `InteractionProfileChanged` event follows `xrAttachSessionActionSets`, and
  `xrGetActionState*` answers for the bindings of that profile.
- A session can end and a new one be made in the same run (an app that switches to a flat
  window and back): `xrDestroyActionSet` frees the action names and bindings, and the times
  of the input script, `MOCK_XR_DUMP_SHOWN` and `MOCK_XR_SECONDS` keep counting from the
  first `xrBeginSession`.

## Environment

| Variable | Meaning |
| --- | --- |
| `MOCK_XR_TRACE=<file>` | the trace |
| `MOCK_XR_SECONDS=<s>` | the session stops after this long (default 20, 0 never) |
| `MOCK_XR_HZ=<hz>` | display refresh |
| `MOCK_XR_SWAY=0` | keep the head still |
| `MOCK_XR_SKIP=1` | a frame begun late loses refreshes |
| `MOCK_XR_EXTENSIONS=a,b` | replace the list of extensions offered |
| `MOCK_XR_END_GPU=1` | `xrEndFrame` waits for the app's queue to go idle before it returns, as SteamVR's did on the Frame while a GPU-heavy game ran (13 to 23 ms calls). The app holds its queue lock around the call, so this also holds back the app's own submits and swapchain releases |
| `MOCK_XR_BUSY_MS=<ms>` | at each release, wait for the app's queue to go idle; longer than this is an error ("released while GPU work could still be pending"). Off by default, because it stalls the app's queue and submits on it from the releasing thread |
| `MOCK_XR_TRACE_POSE=1` | quad positions in the trace |
| `MOCK_XR_INPUT_SCRIPT=<file>` | scripted input, below |
| `MOCK_XR_DUMP_DIR=<dir>` | write swapchain images as PNG |
| `MOCK_XR_DUMP_FRAME=<n\|last>` | `n`: the first image of each swapchain released at or after frame n. `last` (default): the last image released, written when the session ends (`xrEndSession`), so the app has to end its session before it is killed. `n1,n2,...` (ascending): one dump per swapchain after each listed frame (`sc<id>_f<frame>.png`; a swapchain that releases nothing between two listed frames is dumped once for both) |
| `MOCK_XR_DUMP_SHOWN=t1,t2,...` | at the first `xrEndFrame` at or after each listed time (seconds since the session began, as in input scripts), the image every submitted quad layer shows (its last released image) as `sc<id>_shown_f<frame>.png`, and a `shown` trace line. What is on screen then, whether or not anything was released; a hidden layer is not dumped |

## Trace and checks

Each line starts with the frame number (`f<n>`, counted at `xrWaitFrame`):
`createSwapchain`, `acquire`, `release`, `waitFrame`, `beginFrame`,
`endFrame layers=N quad[sc0:400x240 at 0,0 eye=1 sp=local size=1.60x0.96]` (a cylinder:
`cyl[sc0:400x240 eye=1 sp=local r=3.18 angle=30.0 aspect=1.667 arc=1.67x1.00]`),
`suggestBindings`, `attachActionSets`, `input` (script events applied),
`haptic`, `dump`. Once a second:

    f240 summary t=2s waits=120 begins=120 ends=120 layers=3..3 quads=360 proj=0 cyl=0 releases: sc0=60 sc1=60 sc2=60 busy=0 errors=0

and at the end `total sc<n> ... releases=` and `total waits= ends= errors=`.

`ERROR` lines (counted; also in the summary on stderr): acquire, wait and
release out of order; `beginFrame` without `waitFrame`, `endFrame` without
`beginFrame`; a quad layer whose swapchain has no image released, whose image
rect is outside the swapchain or whose space is unknown; extensions that are
not offered; bindings for a profile the mock does not know or for
`frame_controller` without its extension; creating the session before
`xrGetVulkanGraphicsRequirements2KHR`; action states read before the action
sets were attached or with the wrong type; a release before the GPU finished
(`MOCK_XR_BUSY_MS`); for projection layers, eyes from different `xrLocateViews`.
`WARN`: a scripted input that no action binds.

## Scripted input

`MOCK_XR_INPUT_SCRIPT` names a text file, one event per line:

    # <when> <input path> <values>
    1.5   /user/hand/right/input/a/click       1
    f200  /user/hand/left/input/thumbstick     0.5 -0.25
    f210  /user/hand/right/input/trigger/value 0.8
    f5    /user/hand/right/input/aim/pose      0.1 0.2 -0.3 15 -10

`when` is seconds since the session began, or `f<n>` for a frame number. Events
apply at the next `xrSyncActions` after they are due and last until changed.
Booleans are 0/1 (or `true`/`false`); a float input takes one value, a
thumbstick two (`.../thumbstick/x` and `/y` read one axis of it). A bool action
on a float input is true above 0.5. Poses are `x y z` (metres, in LOCAL space,
whose origin is where the head began), then nothing, or `yaw pitch` in degrees
(yaw positive to the left, looking down -Z), or a quaternion `x y z w`. Without
a scripted pose a controller rests 0.25 m to its side, 0.4 m below the head
and 0.35 m ahead, pointing straight ahead. The `frame_controller` paths the
GalaxyFrame app uses: `/user/hand/{left,right}/input/` with `thumbstick`,
`thumbstick/click`, `trigger/value`, `squeeze/value`, `bumper/click`, `a`, `b`,
`x`, `y`, `menu`, `view`, `dpad_up|down|left|right` (all `/click`), `aim/pose`.

## mockrun.sh

    tools/mockrun.sh [--bin PATH] [--rom PATH] [--seconds N] [--script FILE]
                     [--out DIR] [--vvl] [--display xvfb|offscreen|host]
                     [--hz N] [--busy MS] [--dump-frame N|last] [-- extra args]

Runs `BIN --vr ROM` (default `~/Games/AzaharFrame/build-xrcore/bin/Release/azahar`)
for N wall-clock seconds (default 20), ends it with SIGTERM, and writes
`emulator.log`, `trace.txt`, `dumps/`, and `summary.txt` into the out dir
(default `~/Games/AzaharFrame/reports/mock/<timestamp>/`). The summary ends in
`RESULT: PASS` or `RESULT: FAIL: ...`, which is also the exit status.

## Test

`test/xrmock_test.c` is a small app that has nothing to do with
Azahar for Frame: Vulkan instance and device through the runtime, a session, one
400x240 quad swapchain, 120 frames of acquire / clear / fence / release /
`xrEndFrame`, and a few `frame_controller` actions. `test/run.sh` builds and
runs it through `mockrun.sh` with `test/input.script` and checks the result.

## Limits against SteamVR

- Images are not composited; nothing is shown. Only dumps show what was drawn.
- GPU timing is the host's, not the Frame's. No reprojection, no motion smoothing.
- The head, controllers and the refresh clock are idealised. Only the paths the
  script sets exist; the mock does not check that a binding path is a real
  `frame_controller` component.
- Qt needs a display for its Vulkan surface; `mockrun.sh` defaults to a private
  Xvfb. Whether the emulator's own window renders there is up to the emulator.
