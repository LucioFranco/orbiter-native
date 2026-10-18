# Local macOS port status

Latest instrument validation: 2026-10-07, continuing from `d621bd4`. The LM
X-pointer black needles are now verified in both crew views after correcting
legacy zero-power specular rendering. See
[macos-instrument-validation.md](macos-instrument-validation.md) and the visual
validation record for current evidence. The checkout descriptions and numbered
implementation entries below retain their historical context.

Original status dated 2026-09-30. This is a private, local-only experiment. Do not push,
publish, or upstream changes. The architecture in
[the race plan](nassp-macos-recommendation.md) is fixed by the user handoff.

## Verified checkout

- Engine: `LucioFranco/orbiter`, local branch
  `claude/compassionate-shannon-tzfehi`, at `1eed1765` (design documents on
  upstream `4137930c`). `origin` points to the fork; `upstream` points to
  `orbitersim/orbiter`.
- Addon: `Addons/NASSP`, a submodule whose URL is
  `https://github.com/LucioFranco/NASSP.git` and configured branch is `macos`.
  The local `macos` branch is at
  `96b6380943c382a297f969842be9344ef969a6f4`, fetched from CaptainSwag101's
  CMake branch.
- The fork has no remote `macos` branch as of this setup. The branch exists
  locally only. Do not use `git submodule update --remote` until this is
  deliberately resolved; ordinary submodule updates use the recorded commit.
- The sibling `../NASSP` checkout remains untouched and is not the port's
  source tree.

## Current checkpoint

The native app runs the original CSM and LM 3D cockpits, dynamic displays and
mouse picking, with shared Rust/wgpu rendering, basic local lights, original
flight computers and propulsion systems. Native Orbit/Surface and Apollo MFDs
are working, and XRSound-based cockpit audio now uses shared miniaudio output. The 2D FDAI now has a tested CPU renderer, though complete 2D panel
presentation is still pending. SDL joystick mappings now reach both spacecraft;
see the latest joystick integration entry and quickstart for configuration.

Adaptive terrain patches use simulation elevation and DDS imagery from loose
files or compressed archives. Downloaded Orbiter 2024 Earth/Moon imagery and sky
textures are staged locally and have been exercised in actual spacecraft and pad
views. Elevation-boundary ownership and matching grid coordinates are tested;
neighbour texture padding removes the observed pad foreground seam. Global and
mixed-resolution seam validation remains incomplete, and padding currently raises
texture memory and loading cost. Contact probes cover individual landing/liftoff
operations rather than a full lunar mission.

Particle streams now simulate emission and expiry, submit sorted textured GPU
batches, select sprite-atlas variants, and respect scene depth. A real LM DPS
fixture verifies visible lunar dust and its custom texture. Dust origins follow
the local ground datum; diffuse sprites dim in planetary shadow. Diffuse particles now have local ground-plane contact; curvature across large
plumes, reentry streams and more faithful lighting remain unfinished.

Engine flames use the original vessel exhaust definitions. Spacecraft materials
now preserve specular colour, shininess and opacity. Earth has an approximate
Rayleigh atmosphere; focused Sun-shadow cascades include exterior hulls when
viewed from the cockpit. These effects have GPU and actual-vessel regressions,
but plume lighting, environment reflections and richer atmospheric effects remain.

Remaining scope includes completing terrain validation and optimisation, particle
fidelity, legacy mission-audio sequencing, atmosphere/shadow refinement,
complete 2D panel presentation, physical controller verification and a
full end-to-end lunar mission. Existing flight checks cover individual operations.
The latest complete native integration run passes all 78 tests (140.04 seconds),
including 27 GPU-labelled checks, after the terrain-border, exhaust, specular and
material-opacity changes. Log: `build/native-poc/material-terrain-integration.log`.
This verifies the covered fixtures, not a completed lunar mission.
The entries below record successive implementation checkpoints; their test counts
and limitations describe those checkpoints, not the current total.

## Implementation history

For the built app and cockpit controls, see [the local PoC quickstart](macos-poc-quickstart.md).

Initial cockpit checkpoint: native arm64 Saturn5 startup, saved orbital insertion, and a
standalone free-flight CSM run with real Apollo 11
mission configuration, Comanche055 ROM and checklist assets. The insertion
fixture paints powered DSKY segments and advances 60 seconds under sanitizers.
The SDL3 window
now renders the textured vessel cockpit through Rust/wgpu while the original
simulation runs. `--vc` invokes NASSP's VC initialization, selects its cockpit
meshes, and follows Orbiter's camera; right-button mouse-look and wheel FOV
use SDL input. Camera-relative transforms and reversed-Z are active.
Low-resolution planet spheres now share the cockpit depth buffer, and scene
lighting follows the simulated Sun. Planet textures and shadows are pending.
A separate runtime now runs this cockpit with native Sun/Earth/Moon ephemerides,
Earth atmosphere, and Earth/Moon gravity coefficients (see the solar checkpoint).

At that checkpoint, all 25 CTests passed in the ASan/UBSan build, including a real Saturn5 live-cockpit
run that checks camera rotation and changed framebuffer pixels after SDL mouse
motion. Rust itself is not sanitizer-instrumented. The renderer now evaluates mesh-group animation transforms and uploads changed
vertex poses in a batch. Native VC picking dispatches clicks to NASSP: an SDL
glare-shade click changes its saved equipment state and the rendered geometry.
Native cockpit texture redraws now update instrument regions; 29 regions in
the startup fixture paint pixels that are verified against the mesh texture.
Per-visual mesh copies now receive NASSP visibility and vertex edits; ordinary
material reads/writes and texture replacement are supported. Extended Diffuse,
Light, and Emission properties now reach the shader; lighting changes use the
batched vertex update path. Lighting is still approximate: separate emission
maps and specular response remain unimplemented. Cockpit point/spot lights now
reach the renderer (see the local-light checkpoint below);
LOCALVERTEXLIST animation outputs are not yet updated. This is not yet a flyable
Apollo mission. Details below include
earlier checkpoints.

The first native compilation checkpoint is implemented:

- `win32_compat.h` supplies fixed-width Win32 SDK types, opaque handles, RGB
  helpers, case-insensitive comparisons, and bounded CRT copy/format wrappers.
- The umbrella SDK and GraphicsClient headers compile with Apple clang without
  Windows headers or unconditional x86 intrinsics. Portable vectors retain
  32-bit coordinates and 16-byte vector/matrix alignment.
- All ten PanelSDK translation units build into an arm64 static library from
  the actual NASSP submodule. The checkpoint uses NASSP's PanelSDK CMake target
  without linking Windows import libraries or XRSound.
- NASSP fixes include obsolete `fstream.h`, standard min/max calls, actual
  source filename casing, a header guard typo, and format argument mismatches.
- `liborbiter.dylib` builds from the existing configuration, scenario metadata,
  clock, orbital-elements, vector/matrix, color and logging code. The global
  clock and debug buffer have moved out of the application into the core.
- VFS lookup translates backslashes, resolves ASCII case component by
  component, handles output paths, and preserves symlink/dot-dot semantics.
  Config's main asset-path accessors and scenario reads use it. This is not yet
  wired through every engine or NASSP file open.
- A low-level dynamic-library owner uses `dlopen`/`dlsym` on POSIX and
  `LoadLibraryExW`/`GetProcAddress` on Windows, with native suffixes and errors.
  It is tested with a module linked to the same core library.
- The module manager now dispatches `InitModule`/`ExitModule` explicitly, with
  fallback to `opcDLLInit`/`opcDLLExit`. References from multiple vessels share
  one initialization and shutdown runs on the final release. The SDK no longer
  invokes callbacks from `DllMain`; modules must be rebuilt against this SDK.
  Native SDK version exports compile, and vessel, celestial-body and application
  module loaders use the manager. The application itself still needs a native
  host for the full graphics session. The lifecycle test verifies both callback naming conventions, actual
  native handles, shared references, final shutdown, and duplicate-close safety.
- Seven CTest tests pass: SDK layout/CRT checks, NASSP matrix/vector checks,
  native addon lifecycle and full world/vessel propagation,
  generated bitmap-resource lookup,
  native-core integration, and real NASSP Apollo 11 scenario metadata. The
  native-core test advances 5,000 fixed timesteps through the existing Kepler
  solver, checks energy conservation and orbital closure, and verifies that
  the dynamically loaded fixture shares the core clock with its executable.
  Tests do not stub engine APIs.
- The scenario test loads and round-trips environment and focus metadata from
  `Apollo 11 - Launch.scn`. This caught CRLF parsing assumptions, now fixed.
  It does not load the scenario's vessels, AGC, systems or rendering state.
- Compiler fixes cover nonstandard friend defaults and standard-math clashes.
  The file-based Elements constructor now initializes its solver cache; logging
  correctly copies `va_list` when formatting for both the log and callback.
- Floating-point contraction is disabled. Format mismatches and pointer/integer
  cast diagnostics are errors in the checkpoint.
- Another 30 world/host translation units compile as the `OrbiterWorld`
  object target, including vessel/rigid-body dynamics, planets, meshes, bases,
  terrain archives, flight recording, navigation, and camera-mode parsing.
  Fixed header casing, MSVC-only declarations, Win32 file/directory calls,
  DirectInput header coupling, and legacy inline D3D7 method dependencies.
  This target now links into `liborbiter` with `NativeHost.cpp`. The thin
  `orbiter` executable accepts `--scenario`, `--root`, `--steps`, and `--dt`.
  It is currently a bounded headless runner, with 600 steps by default.
- SDK file/scenario APIs and particle-stream ownership have been extracted from
  the Windows application-facing API files into shared implementation files.
  They link with the world target. File opens use VFS resolution and an
  owned path string instead of a fixed 512-byte path buffer. Broad SDK file-API
  roundtrip tests are still pending.
- Native module lookup is wired into celestial-body, vessel and module API
  code. Legacy `.dll` names map to the platform suffix, including mixed-case
  names, before VFS lookup. Core tests cover this path and loader diagnostics.
- Tile decompression uses zlib's `uLongf` output length instead of a 32-bit
  `DWORD`; `_fseeki64` maps to a checked 64-bit `fseeko`. A sparse seek test
  verifies offsets beyond 4 GiB without allocating or writing a large file.
- `tools/nassp_bitmap_resources.py` generates separate CSM, LM and MFD ID/path
  tables from the actual RC/header files: 226 + 141 + 1 entries. Generation
  rejects duplicate IDs within a module, undefined IDs, unsupported bitmap
  declarations, and missing/invalid BMP assets. CSM and LM have overlapping
  IDs and must never be combined into one table. The build produces
  `generated/NasspBitmapResources.h` and `generated/nassp-bitmaps.json`.
  Paths currently refer to the local source tree, relative to the engine root;
  the generator accepts a different asset prefix for a staged runtime.
  NASSP's `LOADBMP`/panel call sites still need the surface API conversion.

The native host loads a real planetary system and generic vessel, applies the
scenario state and focus, and advances the original rigid-body integrators and
vessel callbacks. The integration fixture verifies 600 steps (60 simulated
seconds), finite state throughout, orbital displacement, bounded orbital radius,
and cleanup. This is a generic-vessel integration test, not a NASSP mission.
The CLI fails when a configured vessel module is missing; it must not disguise
an unavailable NASSP module as a generic vessel.

Camera input now accepts relative motion/buttons/wheel values. Legacy Win32
input adapters remain for the old build. Core calls to cockpit, HUD, panel and
selection services use virtual dispatch, so the core can link without creating
those UI objects. Their native rendering implementations are still outstanding.
Runtime testing found uninitialized camera/secondary-list fields and invalid
Kepler/interpolation evaluation for a stationary module-less root star; those
paths are fixed and covered by the world test.

The native host is still an initial implementation. Pending runtime work includes
vessel deletion, complete playback/recording and plugin state loading, remaining
SDK exports, actual celestial modules/assets, SDL input/windowing and rendering.
NASSP vessel modules, AGC and electrical/hydraulic systems have not run. Existing
string-literal constness and deprecated `sprintf` warnings remain. Linux and
Windows builds have not been run.

Changes are uncommitted in both repositories; nothing has been pushed. Preserve
the NASSP working tree as well as the engine working tree: the submodule gitlink
alone does not capture addon edits.

NASSP's CMake entry point is
`Addons/NASSP/Orbitersdk/samples/ProjectApollo/CMakeLists.txt`. It now has an
in-tree native path using the engine SDK/core targets. Its existing Saturn5
source list is reused without Windows import libraries or RC compilation.
The native `Saturn5` target is now pulled into the default build by the real
NASSP asset test. It can also be built explicitly with `cmake --build
build/portable --target Saturn5`.
`NASSP_NATIVE_MODULES` defaults to `Saturn5`; other module CMake targets still
need native adaptation. Saturn5 now links as an arm64 dylib in
`build/portable/runtime/Modules/ProjectApollo/Saturn5.dylib`.

All 82 native Saturn5 translation units compile and the shared module links for
arm64 against liborbiter and liblua. The real addon now initializes and completes 600 headless steps in a simplified
startup world. Flight with launch infrastructure and rendering remain unverified.
The target build log is `build/portable/nassp-build.log`.

L1 addon changes now include an explicit null implementation of the XRSound
calls used by `soundlib`, a disabled DirectSound mission-audio implementation,
and FDAI drawing guards. The FDAI still compiles its electrical load, attitude
motion, animation and state logic; 2D drawing is disabled until its CPU renderer
is implemented. GDI EMS debug bitmap dumping is Windows-only. Toggle switches
now obtain shift state through the Orbiter SDK rather than `GetKeyState`.
Mechanical compiler fixes include extra member qualifications, standard min/max,
portable 64-bit formatting and three 32-bit state union words that were using
LP64 `unsigned long`. BasicExcel and the remaining serialized state still need
broader LP64 runtime validation. Saturn's unused RTCC `KillableWorker` source is
omitted; its actual AGC worker remains in the build.

## Build this checkpoint

Run from the engine checkout:

```sh
nix develop path:./nix
cmake -S . -B build/portable -G Ninja -DORBITER_PORTABILITY_CHECKS=ON
cmake --build build/portable
ctest --test-dir build/portable --output-on-failure
```

The pinned flake provides CMake, Ninja, pkg-config and Python; on macOS its
shell hook selects Apple clang via `xcrun`. The `.envrc` supports nix-direnv
as an optional alternative. The flake lives below `nix/` to avoid copying
multi-gigabyte source and asset trees into the Nix store during evaluation.
Rust tooling will be added with the renderer implementation.

`ORBITER_PORTABILITY_CHECKS` bypasses the old Windows application build and
builds the shared core and native executable. `ORBITER_BUILD_NASSP=OFF` skips
PanelSDK and NASSP-specific tests. The temporary `ORBITER_LINK_NATIVE_WORLD`
option is no longer needed: the world now links by default.

For a local headless smoke run, copy the integration fixture to a writable
runtime directory (the executable writes `Orbiter.log` there), then run:

```sh
build/portable/orbiter --root <runtime-directory> --scenario orbit --steps 600 --dt 0.1
```

The fixture is in `Tests/Fixtures/NativeWorld`. It does not include NASSP or the
real solar-system ephemeris modules and is only a core integration fixture.

The native checkpoint also passes AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake -S . -B build/portable-sanitize -G Ninja \
  -DORBITER_PORTABILITY_CHECKS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build/portable-sanitize
ctest --test-dir build/portable-sanitize --output-on-failure
```

## Next implementation checkpoint

1. Finish the native host's runtime semantics, SDK exports and solar-system
   modules, then add the SDL3 application/input layer. Finish VFS integration at
   remaining file-open boundaries. The core/host link and generic scenario CLI
   checkpoint are now passing.
2. Adapt the rest of NASSP's CMake to an engine subdirectory and exclude
   Configurator, sound and joystick for L1. Expand the compatibility header
   only as required by actual sources; audit LP64 serialization.
3. Build the Rust wgpu static library through Corrosion inside the C++
   GraphicsClient plugin. Batch blit and Sketchpad commands per surface per
   frame across the C ABI; render ImGui on the Rust side. Use camera-relative
   floats and reversed-Z from the start.

The generic headless world integration checkpoint is passing. The next runtime
gate is a real NASSP vessel and its systems loaded into this same core. L1 flight, L2 lunar operations, and L3 visual
effects retain the scope in the race plan.

The DSKY external UDP transport is compiled out under `NASSP_NATIVE` for L1;
local key handling, AGC display state, and electrical load calculations remain.
The DSKY translation unit now compiles. All seven portable CTests still pass.

The native Saturn joystick setup and DirectInput polling are compiled out.
Orbiter keyboard attitude-control branches remain active; native VESIM stores
default input values without opening devices. LEM headers are portable, but its
source initialization still needs the corresponding input port before enabling
the LEM module. Standard min/max type fixes allow propulsion and systems sources
to compile. IMU serialized state now uses a 32-bit word.

CSM PCM native initialization skips Winsock while preserving the in-process MCC
uplink path, including AGC interrupt pacing and the telemetry switch gate.
Saturn uses the existing engine debug-string fallback until native MFD module
lookup is integrated. Lua includes use actual filesystem case; callback linkage
and time formatting are portable. Both translation units now compile.

CSM panel resources now use the generated CSM ID table and oapiLoadSurfaceEx:
192 instrument loads and 33 background variants. Panel background registration
accepts a SURFHANDLE and transfers ownership to the panel. The old Pane/Panel
implementation was adjusted to adopt the surface; it is not yet part of the
native build, so its rendering and lifetime behavior remain to be tested with
the graphics host. Other addons and LM panel resource call sites still need
migration to the changed API. VC material lighting now calls oapiSetMaterialEx
through the engine SDK without gcCore headers or private client interfaces.

47 world/simulation SDK functions were extracted from OrbiterAPI.cpp into
WorldAPI.cpp and linked into both build paths. These are original core queries
and operations, not substitutes. The native-world integration test now exercises
public vessel lookup, focus, planetary mass/radius, integrated relative position,
and time queries. All seven tests pass. The embedded AGC no longer tries to
change process stdin mode; DSKY input remains the input path. HUDToggle inherits
the existing mouse handler, which dispatches its virtual SwitchTo, instead of
declaring an override whose implementation was commented out upstream.

Camera SDK functions now live in CamAPI.cpp in both hosts. The world test
verifies camera target, aperture updates, finite global position and normalized
direction through public APIs. SurfaceAPI.cpp supplies 30 existing graphics
SDK entry points, forwarding to the active GraphicsClient (or returning the
original unavailable result when there is none). This is dispatch plumbing; no
rendering backend is active yet. The Saturn5 link now has 44 unresolved symbols,
primarily cockpit/MFD, vessel creation/deletion, mesh loading, and Lua. All seven
portable CTests pass after these changes.

MeshAPI.cpp now shares 19 mesh SDK functions and the original persistent mesh
loader between hosts. Native teardown flushes cached meshes before unloading
plugins and clearing the host. A new test loads the actual NASSP CM-HatchF.msh
from an untouched CRLF asset, verifies 110 vertices/144 triangles, valid indices,
material count, cache reuse, and missing-file handling. This exposed and fixed
CRLF header rejection and an uninitialized material pointer in a zero-length
copy. All eight tests pass under ASan/UBSan. The last Saturn5 link has 41
unresolved symbols; geometry loading does not yet imply visible rendering.

VesselLifecycleAPI.cpp shares the original creation/deferred-deletion APIs and
KillVessels implementation across hosts, excluding only legacy dialog/console
notifications from the native path. Native StepSimulation processes pending
destruction after state finalization. If no surviving focus vessel is available,
it stops stepping with an explicit error instead of leaving a dangling camera.
The integration test verifies both status formats, deferred deletion, focus and
camera transfer, and deletion of a non-focus vessel. All eight tests pass in
normal and sanitizer builds. Saturn5 now has 38 unresolved symbols. Actual
NASSP staging remains untested until the addon links and runs.

The native build uses the existing Lua 5.1 dependency, pinned to upstream commit
69ea087dff1daba25a2000dfb8f1883c17545b7a, as one shared library. NASSP links it
directly. A runtime test built against the SDK Lua headers verifies script
execution, native callback registration, numeric results, and protected errors.
For offline rebuilds, both local CMake caches use FETCHCONTENT_SOURCE_DIR_LUA
pointing at build/dependencies/lua; fresh builds can fetch the pinned source.
Viewport, panel-scale and effect-texture registration now use shared SDK code.
All nine tests pass normally and with ASan/UBSan; Saturn5 has 19 unresolved
symbols, covering cockpit/MFD APIs and scenario saving.

Native scenario saving now reuses the core serializer and reports stream-close
failures. Integration tests save a staged world, destroy the host, reload the
survivor with its focus/position/date, and resume stepping. CockpitAPI.cpp shares
existing pane/MFD dispatch; calls without a pane return unavailable/empty results
(the native host is currently headless). This is not a replacement cockpit.
The pane still needs to be instantiated with the native graphics/input host.
Saturn5 now links with no unresolved symbols. Runtime behavior remains unproven.

The reproducible bring-up command is `python3 tools/stage_nassp_smoke.py
build/portable-sanitize`, then `UBSAN_OPTIONS=halt_on_error=1
build/portable-sanitize/orbiter --root build/portable-sanitize/runtime --scenario
NasspStartup --steps 600`. This uses Columbia's real Apollo 11 launch vessel state
in the fixture Sun/Earth system, omitting crawler/ML/VAB/MCC and base references.
It is a startup probe, not a launch or lunar-mission scenario. The real Saturn5
module now completes 10 simulated seconds without sanitizer errors, including
loading Apollo 11 mission configuration through the VFS. Normal startup also
completed before the mission-file path fix; the final mission-path fix has been
verified in the sanitizer build. Further direct CRT asset reads (AGC/checklists
and others) still need auditing before claiming full systems operation.

Runtime bring-up exposed and fixed a mesh overflow caused by discarding an empty
group, a null FILE access from a Windows-style PanelSDK path, and an LVDC
initializer using an exhausted inner-loop index. Empty mesh groups keep their
original slots; a real CM-Docktgt regression test covers this layout.

AGC ROM loads now pass through VFS resolution, report successful loads, and
throw on actual load failure rather than silently continuing. The yaAGC loader
previously left its failure return code set after reading the complete image;
it now returns success after the full read. Mission text strips retained CR
characters. BasicExcel resolves workbook paths and decodes RK doubles using
unsigned bit operations and memcpy rather than shifting a negative integer.
The real Apollo 11 workbook opens with its GROUPS sheet; a new asset test checks
all 36 Comanche055 ROM banks against the original bytes, missing-file failure,
negative/scaled RK values, and workbook groups. Ten sanitizer tests pass.
The mission-configured Saturn startup still completes 600 headless steps after
these changes, with the Comanche055 load recorded in Orbiter.log. This does not
yet verify powered AGC execution or checklist UI operation.

SDL3 release 3.2.0 is pinned and built statically through CMake. `--window`
creates a resizable high-DPI window, paces steps, maps physical scancodes to
Orbiter DIK codes, dispatches buffered/direct keys to the focused vessel, and
feeds camera mouse events. Focus loss releases held keys and mouse buttons.
The adapter test uses SDL's dummy video driver to verify mapping, press/release,
modifier clearing and quit. A desktop run with the real Saturn5 module completed
120 windowed steps (2 simulated seconds). Desktop access was necessary because
the execution sandbox cannot access macOS displays. This verifies window startup,
not rendered output or interactive vessel controls. Native global key bindings,
panel mouse routing and gamepads remain unfinished. Both builds pass all 11 tests.

Run the window probe after staging assets with:
```sh
build/portable/orbiter --root build/portable/runtime --scenario NasspStartup --steps 600 --window
```
Fresh builds fetch the pinned SDL source; local caches use
`FETCHCONTENT_SOURCE_DIR_SDL3` pointing at `build/dependencies/SDL`.

The Rust renderer is a static library imported through pinned Corrosion v0.5.2.
The Nix shell now includes cargo/rustc. Cargo.lock records wgpu 24.0.5 and its
transitive dependencies. WgpuBridge.h defines a C ABI with opaque renderer and
surface handles, explicit errors, RGBA upload/readback, and ordered batched
rectangle fills. The fill pipeline emits all rectangles in one draw call, with
clipping and replacement semantics (including alpha). Rust catches unwinds at
the FFI boundary. This is the GPU surface foundation; no GraphicsClient plugin
is connected yet, so the Orbiter window remains blank.

The native_wgpu C++ test exercises the real GPU through the C ABI: upload,
overlapping and clipped fills, padded-row readback, byte-for-byte pixel results,
invalid dimensions/buffers and stale handles. It requires desktop GPU access;
the execution sandbox reports no adapter. It is labeled `gpu`, so CPU-only
checks can run with `ctest --test-dir build/portable -LE gpu`. Do not interpret
that exclusion as GPU verification. Full normal and C++ sanitizer suites pass (12/12 each). Rust code
is not instrumented by the C++ ASan/UBSan configuration.

Local caches use `FETCHCONTENT_SOURCE_DIR_CORROSION=build/dependencies/corrosion`
(with an absolute path) and `ORBITER_CARGO_HOME=build/cargo-home`. Fresh builds
fetch dependencies; `ORBITER_BUILD_WGPU=OFF` retains CPU-only bring-up.

WgpuClient now builds as a loadable C++ graphics-client plugin linked to the
Rust static library. `--graphics WgpuClient` loads it before the scenario. The
SDK dispatches surface create/update/fill/release through the plugin, which
queues fills in C++ and submits a batch per surface at the frame boundary.
`oapiUpdateSurface` accepts top-down RGBA8 pixels and byte pitch. GPU readback
is available as a diagnostic export. The plugin has surface reference counting
and loads supported image formats with pinned stb_image. It does not yet
implement blits, Sketchpad, DDS textures, ImGui, meshes or presentation.

NASSP contains 234 RLE8-compressed BMPs that stb_image cannot read directly.
The resource-table generator now expands these at build time into
`runtime/Resources/NASSP`, leaving originals untouched. Its table points at
these runtime paths. All 397 source bitmaps are staged; uncompressed files are
copied unchanged. The decoder accepts encoded row padding used by the source
assets, handles RLE runs/literals/deltas and rejects out-of-bounds/truncated data.
Tests cover these operations and compare a real converted master-alarm bitmap
after GPU upload/readback with SDL's independent decode of its original RLE file.

Attaching a graphics client exposed an engine null dereference when the client
declined particle streams. Vessel particle/exhaust/reentry creation now returns
without retaining null entries; a GPU world regression covers all three paths.
With this fix, the real Saturn5 probe completes 600 steps under ASan/UBSan
with WgpuClient attached. The scene is still not visible: RenderNativeFrame
currently flushes surface commands, and no cockpit is instantiated yet.

Example after staging:
```sh
build/portable-sanitize/orbiter --root build/portable-sanitize/runtime --scenario NasspStartup --graphics WgpuClient --steps 600
```
This run requires GPU access even without `--window`.

Final checkpoint validation: normal build succeeds; all 15 sanitizer CTests pass,
including GPU surface, plugin and world regressions. Rust remains outside C++
sanitizer instrumentation. No changes have been pushed.

Native window presentation is implemented with one wgpu pipeline and SDL-owned
window handles. macOS passes a CAMetalLayer; Win32, Xlib and Wayland handle
adapters are present but have not been run on those systems. Resize, zero-size
suspension, surface-loss recovery and detach-before-window-destruction are
handled. The desktop test presents a real NASSP panel, resizes, suspends and
reattaches to a second window. The Mac desktop inspection tool confirmed the
panel is upright and proportioned correctly, with colour-keyed window areas
showing the test background. This remains a static preview, not a live cockpit.
The actual Saturn probe completed 120 windowed steps under ASan/UBSan; its
scene output is still blank because cockpit/scene compositing is not connected.

Batched commands now include fills, rectangular blits and nearest-neighbour
scaled blits with source RGB colour keys. C++ records per-surface batches with
global sequence numbers; Rust merges their order before GPU execution. Thus
a source change after a recorded copy does not alter that earlier copy.
Consecutive operations sharing a source/target use one draw; overlapping
self-copies snapshot the source. Upload, readback and final release synchronize
pending commands to preserve API ordering and resource lifetime. Ordinary
frame rendering sends one FFI batch per destination, never one per primitive.
Destination colour-key flags are currently ignored (per the SDK fallback
contract); alpha blending and general Sketchpad operations remain unfinished.

The plugin test verifies scaling, source keying, cross-surface command ordering,
self-copy overlap, early source release and clipping at negative coordinates
with pixel readbacks. C++ and Rust assert the shared command layout. SDL old
name aliases are disabled to avoid collisions with Orbiter's key modifiers.

Priority update: the user requested focusing primarily on the 3D virtual
cockpit. Subsequent rendering work targets that path; the surface/blit backend
remains useful for dynamic cockpit textures and residual 2D panels.

A first indexed 3D mesh pipeline now renders into GPU surfaces with a
Depth32Float buffer, reversed-Z comparison and zero depth clear. The bridge
accepts positions, normals, UVs and material colours, validates index bounds,
and accepts a column-major camera-relative MVP. A GPU readback test proves that
a near triangle wins even when a farther triangle is submitted last.

The diagnostic cockpit viewer loads the actual 1,157-group CM-VC mesh through
Orbiter's mesh parser, uploads its geometry through the C++ plugin and presents
it in SDL. The desktop inspection tool confirmed visible 3D controls and panel
geometry. It is untextured and not animated yet. The camera uses NASSP's
commander-seat coordinates and 18.4-degree downward tilt, including the
additional -0.15 m offset applied in saturnvc.cpp. The preview uses a 60-degree
vertical FOV, infinite-far reversed-Z projection and 1 cm near plane.
It does not yet obtain its camera/mesh state from a running Saturn vessel.
DDS textures, group visibility/animation state, picking and dynamic instruments
remain necessary for the real virtual cockpit. Transparent materials and
physical cockpit lighting are also unfinished.

The diagnostic viewer (after staging assets) is:
```sh
build/portable-sanitize/wgpu_cockpit_test.app/Contents/MacOS/wgpu_cockpit_test "$PWD/build/portable-sanitize/runtime/Modules/Plugin/WgpuClient.dylib" "$PWD/build/portable-sanitize/runtime" 600
```
The optional last argument is frame count. The viewer keeps all local
experimentation separate from automatic scene selection in the real host.

### Textured 3D cockpit milestone (2026-09-29)

The standalone commander-seat preview now renders the real 1,157-group CM-VC
mesh with all 37 original textures. Desktop inspection confirmed panel labels,
switches, the DSKY, and instrument faces. This remains a static diagnostic view,
not a live or interactive vessel cockpit.

- DDS loading supports legacy DXT1/3/5 (BC1/2/3) and masked 32-bit RGB(A).
  Aligned BC images upload compressed, including tiny mip levels. NASSP's
  unaligned DXT1 images decode and crop partial blocks into RGBA; unaligned
  BC2/BC3 are explicitly unsupported for now. BMP files named `.dds` still use
  signature detection and stb_image.
- Mesh group ranges and texture handles cross the ABI in one array. The shader
  samples UVs with mip filtering and alpha cutout. Material/texture inheritance
  is resolved in C++. Texture allocations remain alive with their mesh bindings,
  and updates to the same surface appear on subsequent renders.
- Texture filenames already include extensions; corrected the doubled `.dds`
  suffix. Mesh texture flag D now copies into writable RGBA surfaces for dynamic
  instruments. Sample-only DDS textures reject render-target/readback operations.
- All 18 CTests passed in the ASan/UBSan build (Rust itself is not sanitizer
  instrumented). New GPU checks cover BC formats, tiny mips, partial blocks,
  malformed/truncated DDS, masked channels/padded pitch, textured group depth,
  texture lifetime, and dynamic texture updates. The full cockpit additionally
  ran in the sanitizer window build, asserting all 37 texture handles exist.

Next: connect rendering to the live vessel's virtual cockpit, camera, mesh group
visibility and animations, then mouse picking and cockpit redraw callbacks.
Lighting is currently diagnostic; alpha blending, emissive materials, variant
visibility, and animation state are not yet represented faithfully.


### Live vessel cockpit and camera (2026-09-29)

`orbiter --vc` now enters the focus vessel's virtual cockpit through its actual
`clbkLoadVC` callback. The renderer gathers meshes marked for cockpit/VC views,
including their vessel-local offsets, and builds its projection from Orbiter's
current camera rotation, local eye position, aperture and viewport. It avoids
subtracting astronomical global positions before float conversion. Resize and
focus changes rebuild the initial cockpit geometry. Mesh changes within the
same vessel are not yet tracked.

Native `oapiCockpitMode()` reports virtual mode so NASSP executes its VC system
updates. Entering this path exposed an uninitialized camera smoothing flag;
the Camera constructor now initializes that flag and its angular velocities.
The test stages the simplified startup scenario, invokes the real Saturn5 VC,
injects SDL right-button mouse motion, verifies camera rotation and framebuffer
change, and steps the simulation. All 20 sanitizer-build CTests pass; a separate
120-frame live run also passed, and desktop inspection confirmed the view.

From the engine checkout:

```sh
build/portable/orbiter --root build/portable/runtime --scenario NasspStartup \
  --graphics WgpuClient --window --vc --steps 36000
```

Right-drag looks around; the wheel changes FOV. The startup fixture omits launch
infrastructure and other mission vessels. Next work is per-vessel visual state,
mesh animations, VC area registration/picking and instrument redraw dispatch.


### Native VC click dispatch (2026-09-29)

The native host now stores NASSP's VC area registrations and their spherical or
quadrilateral click geometry. Picking uses a vessel-local camera ray, filters
mouse-event masks, and chooses the closest hit. Quad coordinates are normalized
using two triangles. Regions can overlap; arbitrary mesh occlusion is not yet
used for picking. The registry resets when the VC or focus vessel changes.

SDL mouse positions are converted from logical window units to framebuffer
pixels before ray construction, including on Retina displays. Button capture
routes releases back to the clicked area even when the pointer has left it;
focus loss releases captures. Held events dispatch on simulation steps. A
right-button click on a registered control takes precedence over camera drag.
Quad dragging updates within the hit region; movement outside retains the last
valid coordinates for the captured control.

Regression coverage includes sphere/quad hits, UV coordinates on both quad
triangles, nearest-area ordering, mouse masks, behind-camera/parallel rays and
degenerate areas. The live Saturn5 test projects the actual DSKY glare-shade
region, injects SDL button events, releases outside the window, and verifies
that bit 4 of NASSP's saved CREWEQUIPMENTSTATE changes after its two-second
logical animation. This proves input reaches vessel logic; it does not claim
that animated geometry or instrument textures are drawn yet.


### Animated cockpit geometry (2026-09-29)

The live renderer evaluates NASSP's animation definitions from their default
mesh states each frame. It handles rotations around pivots, translations,
scaling, component state intervals and child transforms. Local copies of the
transform definitions prevent one visual from modifying shared vessel data.
Group lookup is indexed by mesh/group, unchanged group poses skip vertex work,
and changed vertices cross the C ABI in one mesh update. Scaling uses inverse
transpose normals. LOCALVERTEXLIST simulation outputs remain unimplemented.

The live regression now checks more than 1,000 significantly changed pixels
after the SDL glare-shade click, before moving the camera. Separate tests cover
pivots, reverse/default states, hierarchy, scale normals, definition immutability
and GPU vertex-buffer updates. All 23 sanitizer-build tests passed; the affected
live/pose tests were repeated after caching and indexed lookup changes. Desktop
inspection confirmed the transformed cockpit. No frame-rate target is claimed.

Animation validation exposed NaN normals from the existing mesh loader: unused
vertices and degenerate faces were normalized with zero length. Normal generation
now preserves a zero normal for those vertices and clamps roundoff at acos input
limits. A regression covers both valid and degenerate faces plus unused vertices.

Remaining immediate work: per-vessel visual mesh edits and group visibility,
VC instrument surface redraw dispatch, mesh texture replacement and material
updates. The existing click regions are still NASSP's registered geometry;
picking does not derive occlusion or moving regions from rendered triangles.


### Cockpit instrument redraws (2026-09-29)

Native VC registrations now retain redraw rectangles, target textures, mapping
modes and scratch/background surfaces. The host dispatches initialization,
per-frame and mouse-triggered redraws through the original NASSP callback and
copies successful area draws into their cockpit texture rectangles. Scratch
backgrounds are restored before each background-mapped redraw. Surface references
are released on VC/focus reset and before renderer shutdown.

DDS textures remain compressed until their first write. The renderer then
converts them to writable RGBA while preserving the surface handle and updating
existing mesh texture bindings. This is covered by a GPU test that binds a BC
texture to a mesh, converts and paints it, and checks the new rendered color.
Warm redraws retain the per-surface command batches across the C++/Rust boundary.

The EMS callback required native Sketchpad support. Basic pens, pixels, lines,
polylines and origin offsets are now implemented as recorded surface fills;
line clipping bounds work before rasterization. Filled shapes were added later
(see Native Sketchpad brushes and shapes below); basic text and fonts were
added subsequently (see Portable Sketchpad text). Extended operations remain unfinished. GPU tests verify clipped
polyline pixels and pen color conversion.

The live Saturn5 test confirms 29 callback scratch surfaces differ from their
original backgrounds and checks the painted pixels against their mapped atlas
regions. It also deliberately overwrites the DSKY scratch surface and verifies
that the next redraw restores it. The DSKY segment image itself is unchanged in
this fixture: NASSP skips segment painting when power is absent or EL is off.
This milestone does not prove a powered AGC/DSKY mission sequence. All 23 CTests
pass in the sanitizer build (Rust is not sanitizer-instrumented).

Remaining redraw work includes explicit user-triggered redraw APIs and
background-on-request dispatch. Multi-vessel texture isolation and repeated VC
entry with pristine backgrounds also need verification as part of per-vessel
visual ownership. The present test uses one Saturn5 and one VC entry.


### Per-vessel visual meshes (2026-09-29)

The native graphics client now registers a visual for the current cockpit
vessel, providing owned CPU mesh copies before calling `clbkVisualCreated`.
NASSP consequently applies its mission-specific cockpit group visibility.
The renderer skips groups with the invisible user flag and supports device-mesh
vertex reads/edits, texture replacement, and ordinary MATERIAL reads/writes.
Changed geometry or properties invalidate the flattened render mesh; its output
surface is reused when the viewport size stays fixed. Animation poses continue
to start from the edited visual mesh.

Mesh insertion/deletion/template-change notifications rebuild the visual on the
next frame; offsets and visibility notifications invalidate the render mesh.
Focus changes, deletion notifications and session shutdown unregister the visual
before releasing its meshes. The host closes the graphics session before deleting
world objects so `clbkVisualDestroyed` sees a live vessel.

Copying a core Mesh now retains its texture references, and self-assignment is
safe. Tests verify copied-mesh destruction leaves the template and caller's
texture references usable. The real Saturn5 test verifies visual registration,
mission-group changes, independent vertex/material edits, and cleared/rejected
handles after a deletion notification. All 23 sanitizer-build tests pass.

This still maintains one rendered cockpit visual at a time. Texture pixels are
shared by reference until explicitly replaced; separate per-vessel dynamic atlas
copies remain pending; pristine backgrounds across repeated VC entry are
covered by the later redraw/re-entry checkpoint.
Extended `SetMeshMaterialEx` lighting/emission rendering, full external-scene
rendering, and LOCALVERTEXLIST outputs also remain unfinished. Bulk rebuilds on
mesh edits are a correctness path and have not been performance-tuned.


## Cockpit material lighting checkpoint

The native `oapiSetMaterialEx` / `oapiMeshMaterialEx` SDK path now supports
Diffuse, Light, and Emission on owned visual meshes. NASSP's existing native
`SetVCLighting` calls use this path. Unsupported material properties return an
error, and nonfinite values are rejected. Light and Emission changes update the
retained vertex attributes in one mesh upload without rebuilding the geometry;
unchanged values do not trigger an upload. C++ and Rust assert the 80-byte
vertex layout.

The shader adds Light to the provisional directional/ambient illumination and
uses Emission as a diffuse-texture-modulated brightness floor. This is an
approximation, not the D3D9 emission-map/PBR model. Separate emission textures,
local lights, specular response, and physically accurate cockpit illumination
remain pending. This does not establish that the startup fixture's electrical
systems power its DSKY or cabin lighting.

Validation includes GPU pixel checks for added red light and green emission,
material round trips and template isolation, and an SDK-driven material-only
lighting change in the live NASSP cockpit. The test temporarily suspends
instrument redraws because NASSP reapplies its lighting during those callbacks.
It requires over 1,000 framebuffer pixels to become brighter, then restores
the original materials and redraw registrations
before testing the glare-shade animation. The full 23-test ASan/UBSan suite
passed; the final material integration and GPU mesh checks also pass after
adding the SDK getter. Rust is not sanitizer-instrumented.
The normal native build also succeeds. All work remains local.


## Saved orbital cockpit checkpoint

`tools/stage_nassp_smoke.py build/portable --insertion` now stages
`NasspInsertion.scn` from NASSP's Apollo 11 insertion save. It preserves
Columbia's saved vessel/systems state and the original MJD. Like the startup
fixture it uses the simplified Sun/Earth world and omits other vessels, so it
is not verification of the full mission scenario or its trajectory.

The saved state completed 3,600 headless steps (60 simulated seconds) with
ASan and UBSan enabled. Both normal and sanitizer builds succeed.

The saved state exposed an uninitialized `EDSCutoffDisabled` boolean in the
S-IVB systems constructor. It now starts false, matching the default enabled
cutoff circuitry until a switch-selector command disables it. The orbital
fixture advances under ASan/UBSan and renders the native virtual cockpit.
Unlike the prelaunch fixture, its DSKY segment surface differs from the original
background: the existing powered-display callback paints digits. In total,
31 instrument regions paint changed pixels that reach their cockpit textures.
The same visual ownership, material lighting, SDL glare-shade click/animation,
and camera movement checks pass for this saved orbital state.

The regression suite adds `nassp_stage_insertion` and
`wgpu_insertion_cockpit`. This demonstrates powered display rendering and DSKY keyboard interaction
(the follow-up check below), not a complete AGC guidance sequence. Native external
planet rendering, full mission infrastructure, and other renderer/system gaps
still prevent declaring the overall proof of concept complete.

Run locally:

```sh
python3 tools/stage_nassp_smoke.py build/portable --insertion
build/portable/orbiter --root build/portable/runtime --scenario NasspInsertion --graphics WgpuClient --window --vc --steps 36000
```


## DSKY input and AGC execution checkpoint

The insertion cockpit regression now projects the registered 3D DSKY button
locations into the SDL window and injects mouse down/up events for VERB, 3, 5,
and ENTER. Each key must be selected by native ray picking; simulation steps
run between presses and releases. The test does not call DSKY callbacks or
write AGC memory directly.

The real Comanche055 AGC echoes `VERB 35` in the saved DSKY state, then executes
the display test after ENTER: `PROG`, `VERB`, and `NOUN` become `88`, and the
captured state shows all three registers as `+88888`. GPU readback verifies
that both key entry and command execution change the cockpit's DSKY surface.
Existing material, glare-shade, and camera checks still pass afterward.

Both builds succeed. The updated insertion test passes under ASan/UBSan;
the previous full 25-test suite passed before this test-only extension.
Desktop inspection was retried but timed out; these rendering results are
verified by GPU readback rather than a desktop screenshot. The mission goal
remains open because the external scene and full mission environment, among
other remaining renderer/system paths, are incomplete.


## Cockpit redraw and re-entry checkpoint

The native cockpit now accepts explicit redraw requests from
`VESSEL::TriggerRedrawArea`, `oapiVCTriggerRedrawArea`, and the legacy combined
redraw function. Requests for another cockpit ID are ignored; matching or
wildcard requests are coalesced and delivered as `PANEL_REDRAW_USER` on the
next native frame. Initialization retains priority without dropping a pending
user request. A redraw requested from a callback remains pending for the next
frame instead of recursively invoking callbacks.

`oapiBltPanelAreaBackground` now implements `PANEL_MAP_BGONREQUEST` using the
saved original area texture. Releasing/re-registering cockpit areas restores
their original background into the mesh atlas before releasing the snapshots,
matching the existing virtual cockpit lifecycle. This prevents instrument
pixels from becoming the new background when the user re-enters the cockpit.

Live NASSP tests cover ignored inactive-cockpit requests, one-shot vessel
redraw requests, exact background-copy pixels, and pristine DSKY backgrounds
across cockpit re-entry. Other panel/HUD/MFD APIs still require implementation.

Validation: both builds succeed. All five selected cockpit/fixture tests pass
under ASan/UBSan, including prelaunch and powered orbital cockpit re-entry.


## Native cockpit station navigation

The native core now retains `oapiVCSetNeighbours` registrations and handles
Ctrl+Left/Right/Up/Down after giving the vessel its normal buffered-key callback.
If the vessel consumes the key, its handling wins. Navigation applies only to
an active virtual cockpit belonging to the focused vessel, and unavailable
neighbours leave the current view unchanged. Resetting cockpit areas clears
old neighbour registrations so they cannot leak between vessels/views.

A view transition uses NASSP's own `clbkLoadVC` for the selected station,
releases mouse capture, restores the outgoing instrument backgrounds, registers
the new controls, and resets the simulation camera to the new station. This
allows access to the existing seat, docking, hatch, and lower-equipment-bay
view graph rather than hardcoding camera offsets in the renderer.

Use Ctrl+arrow keys to change stations; right-drag looks around within a station
and the wheel changes field of view. The powered cockpit regression checks
commander-to-center navigation, the changed camera location, return to the
commander, and an unavailable left neighbour through SDL keyboard events.

Validation: normal and sanitizer builds succeed; native SDL and powered
orbital cockpit checks pass under ASan/UBSan with the navigation assertions.


## Planet scene foundation

The wgpu bridge now accepts a batch of mesh/matrix draws for one scene pass.
Each draw retains its texture groups and camera transform, while all draws
share a single colour and reversed-Z depth attachment. The original single-mesh
entry point forwards to this batch API. A GPU test draws a near blue mesh
before a separate far red mesh and verifies that the near mesh stays visible.

The live cockpit scene adds 32-by-64-segment spheres for the simulation's
planetary bodies. Meshes are cached; each frame obtains body radius/position and
the camera's rotation/position from Orbiter. Global positions are subtracted in
double precision before conversion to the float camera-relative transform.
The cockpit retains its local-coordinate transform and shares the planet pass's
depth buffer. Earth is provisionally blue and other planets grey. This is a
geometric foundation: surface textures, correct solar illumination, atmosphere,
stars, terrain tiles, and external vessel views remain unfinished.

The orbital cockpit test hides the cockpit meshes temporarily, points the
simulation camera toward Earth, and checks blue planet pixels at the view
centre. It then looks away and requires black space at the centre before
restoring the cockpit. These checks complement the shared-depth GPU test.

Validation: both native builds succeed. All five selected mesh/cockpit fixture
tests pass under ASan/UBSan, including the orbital Earth-facing/away pixel
checks and the prelaunch cockpit regression. Rust remains uninstrumented.


## Sun direction in the scene

Scene draws now carry a directional-light vector and ambient term across the
C/Rust ABI, with layout assertions on both sides. The native client derives the
direction from the simulated star position minus the body/vessel position in
double precision. Planet normals use that direction in world coordinates;
cockpit normals use the direction transformed into vessel coordinates. Camera
rotation therefore does not rotate the physical light source with the view.
Static mesh diagnostics retain their existing preview light.

The shader applies Lambert diffuse lighting plus the existing NASSP Light and
Emission material properties. Planet ambient is 0.03 and cockpit ambient is
0.35 for this provisional scene; these are visibility aids, not calibrated
radiometry. Planetary eclipse/occlusion, cockpit self-shadowing, local lights,
solar-distance intensity changes, and atmospheric scattering remain pending.

GPU regression coverage reverses the directional light and verifies that
surfaces facing away become black with ambient set to zero. The orbital
Earth-facing test also accepts the intentionally dim night-side colour, while
still requiring black space when looking away. Existing cockpit material and
DSKY tests exercise the combined solar/material path.

Validation: both builds succeed and all 25 CTests pass with ASan/UBSan
enabled for C++, including solar-direction GPU checks and both live cockpit
scenarios. The Rust renderer is not sanitizer-instrumented.


## Native Sun/Earth/Moon environment

`NativeCelbody.cmake` rebuilds the existing Sun and Earth VSOP87 implementations,
Moon ELP82 implementation, and Jacchia71-Gill Earth atmosphere against the same
native shared core. VSOP87 support is a PIC static library linked into its body
modules. Module filenames use the platform's shared-library suffix. The native
SDK no longer emits the legacy static-import-library `dummy()` reference from
`ORBITER_MODULE`; module date exports are preserved.

The ephemeris readers resolve their Windows-style data paths through the VFS.
Gravity coefficient loading does the same. Loading the real Earth configuration
exposed an out-of-range floating-point-to-unsigned conversion for untunable
navigation frequencies; those now retain their frequency with an invalid-channel
sentinel. Loading surface bases exposed a zero-length copy from an uninitialized
object-array pointer; the copy now happens only when the array contains objects.

`--solar` on the staging script creates a separate `runtime-solar` with original
Sun/Earth/Moon configs, VSOP87/ELP82 tables, Earth/Moon gravity models, and engine
plus NASSP mesh/texture assets. The original simplified fixture is retained as a
regression case. NativeSolar currently includes three bodies; other planetary
and satellite modules, full NASSP launch infrastructure, and complete mission
staging remain unfinished. The Jupiter/Saturn VSOP wrappers require the separate
Galsat/Satsat libraries and are not yet included in the native build.

The new headless regression requires active ephemeris interfaces for all three
bodies, finite changing model outputs, active Earth atmosphere, realistic
Earth–Sun and Earth–Moon distance ranges, and 60 seconds of NASSP propagation.
The GPU regression runs the full orbital cockpit interaction suite in this
world, including DSKY VERB 35 ENTER, view navigation, redraw/re-entry, and Earth
placement. These checks establish module integration, not cross-platform
trajectory accuracy or a completed Apollo mission.

Run locally:

```sh
python3 tools/stage_nassp_smoke.py build/portable --insertion --solar
build/portable/orbiter --root build/portable/runtime-solar --scenario NasspInsertion --graphics WgpuClient --window --vc --steps 36000
```

Validation: both builds succeed. The normal solar runtime passes the 60-second
headless test, and all three new solar fixture/headless/cockpit tests pass under
ASan/UBSan after merging engine and NASSP assets. The suite now contains 28 tests;
the earlier full 25-test suite passed before this environment extension.


## Save and resume from the native executable

`orbiter --save <scenario-name>` invokes the existing scenario writer after the
requested step count or a normal SDL window close. The save goes through the
configured scenario path (by default `Scenarios/<scenario-name>.scn`) and includes
the simulation date, focus, camera, vessels, NASSP systems/AGC state, and plugin
save callbacks. A write failure produces an error exit instead of reporting a
successful run. No save occurs unless `--save` is supplied.

Example:

```sh
build/portable/orbiter --root build/portable/runtime-solar --scenario NasspInsertion --graphics WgpuClient --window --vc --steps 36000 --save MyApolloSession
build/portable/orbiter --root build/portable/runtime-solar --scenario MyApolloSession --graphics WgpuClient --window --vc --steps 36000 --save MyApolloSession
```

The new regression launches three separate native processes: advance and save,
reload without advancing and save again, then resume and advance. It compares
AGC saved state, both DSKYs, mission clock/stage/equipment/view, date, and orbital
position/velocity, then verifies that the resumed AGC executes and clocks advance.
It uses unique temporary scenario names and removes only its own test saves.

NASSP intentionally does not reload `CYCLECOUNTER` because its CDU pulse FIFOs
are not serialized. The port preserves this existing behavior; the regression
excludes that field from exact AGC comparison. This is therefore a NASSP scenario
resume, not a bit-exact checkpoint of all emulator queues. The scenario mission
clock comparison allows sub-nanosecond formatting/parse roundoff, and orbital
vectors are compared to scenario precision. Both builds succeed; the normal and sanitizer
save/resume regressions pass. The suite now contains 29 tests.


## Shared keyboard flight controls

The native simulation now runs Orbiter's existing default flight-input handler
after the vessel's direct-key callback. That handler has moved into
`FlightInput.cpp`, compiled by both the original and native core targets. The
native host clears per-frame control requests, fills them from unconsumed DIK
keys, and applies the existing thruster-group overrides before module pre-step.
This also makes manual translation requests visible to NASSP through
`GetManualControlLevel`. Native MFD dispatch remains disabled until its manager
exists; the original host retains that dispatch.

NASSP already owns its rotational hand-controller keys and removes them from
the key buffer. They remain owned by NASSP and are not applied twice. Keypad
Divide selects rotation/translation through the existing vessel callback.
Default translation keys are keypad 2/8 (up/down), 1/3 (left/right), and 6/9
(forward/back); Ctrl requests 10% level. Actual Apollo jet activation still
depends on its electrical power, controller switches, and SCS/CMC mode.

Regression evidence includes a real core thruster producing angular velocity
from a native keyboard press and stopping on release. In the real solar NASSP
fixture, full/reduced translation levels reach the manual-control API, released
keys clear them, and consumed rotation keys produce no second core request.
At this checkpoint this covered only the API request; the free-flight CSM
regression below now verifies actual Apollo jet firing and propellant use.

Validation: both builds succeed. Core dynamics, real solar NASSP controls, SDL
input, and the powered solar cockpit regressions all pass under ASan/UBSan.

## Free-flight CSM and real Apollo RCS

`tools/stage_nassp_smoke.py --csm` stages `NasspCSM` in a separate
`runtime-csm` directory with native Sun/Earth/Moon ephemerides. It uses Apollo
11's saved MCC-2 T-15m state (stage 30), omits the unported LM and its docking
reference, selects SCS control, powers the THC and both RHC direct supplies,
and moves any OFF automatic RCS jet selectors to their main-A supply
while retaining the already-selected A/B supplies. It preserves the saved
propellant, electrical systems, AGC, and orbital state. This is a standalone
manual-flight probe, not a complete docked Apollo mission.

The insertion fixture is still attached to the S-IVB (stage 21) and has its
automatic RCS enable relays disabled. That explains why proving the native
manual-control request there did not prove an actual CSM burn.

The new `native_rcs` regression holds each of the six translation commands for
one second. It checks the actual NASSP thruster resultant, propellant consumption,
and all jets stopping after release. Lateral thrust is about 874 N; forward/back
thrust is about 1749 N in this state. Ctrl's 10% Orbiter request remains below
Apollo's THC breakout threshold and does not fire the jets; it is not a 10%
Apollo thrust setting.

Native cockpit click regions now follow `VESSEL::ShiftCG`, matching the existing
Orbiter pane behavior. Sphere/quad picking remains invariant when both the eye
and the vessel reference frame shift. This matters in the free-flight CSM,
whose mass distribution changes as NASSP updates its systems.

Run from the engine repository:

```sh
python3 tools/stage_nassp_smoke.py build/portable --csm
build/portable/orbiter --root build/portable/runtime-csm --scenario NasspCSM --graphics WgpuClient --window --vc --steps 36000 --save MyCSMSession
```

Use keypad Divide to select translation, then keypad 2/8 (up/down), 1/3
(left/right), and 6/9 (forward/back). Ctrl-arrow changes cockpit seats; right-drag
looks around. Desktop inspection can list apps but still times out when selecting
the cockpit app; GPU readback and injected SDL input provide automated coverage.

Validation: both normal and sanitizer builds succeed. All 32 CTest cases pass
with ASan/UBSan enabled for C++ (Rust is not sanitizer-instrumented), including
powered CSM DSKY input, texture updates, animated cockpit controls, seat changes,
and the prelaunch/insertion regressions. The normal build also passes the new
RCS and reference-frame picking tests. No changes have been pushed.


## Rotational flight controls and momentary input

The CSM regression now verifies all six rotational directions in both normal
RHC mode and Alt hardover: integrated jet torque, actual angular-velocity change,
propellant consumption, and subsequent SCS damping. Normal proportional-rate
control may pulse its jets, so the test integrates torque over the command rather
than requiring a jet to be on at one arbitrarily sampled frame.

A reproduced NASSP keyboard bug kept the previous RHC deflection when switching
from rotation to translation, or when modifiers prevented RHC input. The direct
keyboard callback now recenters the six keyboard deflections every frame before
applying eligible keys. With the fix temporarily disabled, the regression fails
at “mode change releases commanded rotation.” With it enabled, mode changes
release the command. The regression also covers Ctrl and Shift transitions.
The SCS may briefly fire opposing jets to remove residual angular velocity after
release; that is expected controller behavior.

In rotation mode, keypad 2/8 controls pitch, 1/3 yaw, and 4/6 roll. No modifier
requests normal proportional-rate control; Alt requests hardover/direct control.
The standalone CSM probe now powers both direct RHC supplies through their actual
panel switches so hardover is available. All changes remain local.

Validation after the RHC fix: normal build and RCS regression pass; sanitizer
build and all six selected fixture/control/cockpit checks pass (`native_sdl`,
`native_solar`, `native_rcs`, and `wgpu_csm_cockpit`, plus their staging fixtures).
The previous full 32-test pass predates this RHC change. No pushes or commits.

## Native Sketchpad brushes and shapes

The wgpu client now provides brush creation/selection/release and filled or
outlined rectangles, polygons, and ellipses. NASSP's EMS roll-indicator drawing
uses the ellipse/polygon APIs; they previously reached missing native methods.
Rectangle edges are half-open; polygon fill uses the even-odd rule at pixel
centers, including concave and self-intersecting shapes. Origin offsets and
clipping are applied before integer conversion. Ellipse fill and its pen-width
outline use scanline spans, with angular dashes for dashed pens. These are
non-antialiased raster operations. Basic text/font support was added subsequently
(see Portable Sketchpad text below).

All generated spans go through the existing C++ surface command queue and its
batched Rust submission. No per-primitive FFI calls were added. GPU readback
checks brush/pen colours, clipped rectangles, both polygon windings, concavity,
self-intersections, closed outlines, circles/ellipses, disabled fill, and extreme
polygon coordinates without integer overflow.

Validation: both builds succeed. Expanded GPU shape readback and the powered CSM
cockpit regression pass under C++ ASan/UBSan. No commits or pushes.

## Portable Sketchpad text

`WgpuClient` now embeds eight licensed TrueType faces and rasterizes them with
stb_truetype using the same C++ code on every OS. Arial/Helvetica/Sans map to
Roboto; Courier/Courier New/Fixed/MOCR map to JetBrains Mono. Unknown faces use
the proportional/fixed flag, and Serif currently falls back to Roboto. Font
files and notices are in `Extern/NativeFonts`; a CMake build step embeds them,
so the renderer does not depend on an OS font installation or runtime font path.

The basic font API now supports regular/bold/italic/bold-italic faces, underline,
strikeout, character and string measurements, kerning, UTF-8 with a Windows-1252
fallback, alignment, origins, clipping, text colours, transparent/opaque
backgrounds, and rotated text. Glyphs cache crisp horizontal runs and emit into
the existing surface command batches. Antialiasing flags currently still render
crisp coverage; shaping/ligatures, a true Serif substitute, and the extended
font-creation API remain outstanding.

Native TextBox wraps at spaces/hyphens and handles CRLF, respecting explicit
lengths and UTF-8 boundaries. This replaces the inherited routine for native
Sketchpads, whose loop could read past an explicitly bounded input string.

GPU readback tests cover proportional/fixed aliases, visible glyphs, matching
UTF-8 and legacy degree symbols, Greek delta, non-terminated input buffers,
real style changes, decorations, background preservation, alignment/origin,
right-angle rotation, clipping, word wrap, and CRLF. All drawing remains batched
per surface across the Rust FFI; no per-character FFI calls were introduced.

Validation: both builds succeed; the new text test passes in both. All four
selected sanitizer cases (text, general surface rendering, CSM staging, powered
CSM cockpit) pass. The suite now contains 33 cases; it was not rerun in full
after this font addition. No commits or pushes.

## Interactive app entry point and visual color-key correction

The shared thin host now runs windowed sessions until the window closes, unless
`--steps` explicitly supplies a limit. Headless sessions retain their 600-step
default; `--headless` explicitly selects that mode. The `nassp_poc` target builds
`NASSP PoC.app` on macOS from the same host source, with defaults for the staged
free-flight CSM, wgpu, and virtual cockpit. `stage_nassp_poc` explicitly stages
its dependencies/assets; normal builds do not overwrite runtime configuration.
Startup errors in windowed sessions are shown in an SDL message box. This is a
local build entry point with an absolute build-runtime path, not a relocatable
distribution bundle. See [the quickstart](macos-poc-quickstart.md).

Desktop inspection now recognizes the app and successfully captures its live
3D cockpit, though individual inspection calls remain slow. The first capture
exposed magenta transparency masks on the DSKY and EMS, which earlier tests had
missed. Three causes were corrected:

- The native host advertised zero color depth before its first window, causing
  `oapiGetColour` to pack startup colors and keys as RGB565. Native surfaces now
  advertise 32-bit color from host construction.
- SDK `oapiBlt` discarded color-key arguments. A native graphics callback now
  records explicit/predefined/no-key behavior per command without changing the
  source's saved key.
- NASSP depends on D3D9's automatic use of predefined source keys, including
  digit blits that omit the key argument. Native SDK declarations now default
  those calls to the predefined key; explicit `SURF_NO_CK` remains unkeyed.

The corrected desktop screenshot shows the EMS scroll and DSKY with no magenta
mask rectangles. Tests now cover the actual SDK dispatch path, color conversion
before a window exists, key snapshots across queued blits, scaled copies, and
absence of leaked magenta in the real DSKY redraw. The CLI regression verifies
bounded headless runs, explicit limits, and a windowed process remaining alive
past its old automatic exit point. A separate test launches the app entry point.

Validation: both builds succeed and all 35 CTest cases pass under C++ ASan/UBSan
(Rust is not instrumented). Desktop inspection also visually confirms the
corrected live cockpit. The normal app was left open for local use. No commits
or pushes.

## Cockpit rendering overhead (local, September 29)

Added `--profile` to the native host. It reports mean simulation, render, and
whole-frame wall times after ten warmup frames. Render time includes cockpit
redraws, mesh preparation, and presentation; it is not GPU timestamp time.

A CPU sample of the real `NasspCSM` scene found repeated cockpit topology rebuilds
and full vertex validation/uploads. The graphics client now ignores redundant
visibility notifications, applies device vertex edits to the affected unanimated
groups, and recomputes their poses without rebuilding unrelated geometry.
Animation and lighting changes upload affected vertex ranges through one FFI batch
per mesh/frame. Invalid batches are checked completely before any buffer writes.
Lighting dirtiness is tracked by mesh/material. Texture bindings, the mesh sampler,
and the white fallback texture persist; DDS promotion explicitly rebuilds affected
bindings so live instrument surfaces remain visible.

The normal Debug build's 90-frame windowed CSM run (80 measured frames) went from
615.803 ms mean render time to 289.400 ms. These are exploratory measurements with
another cockpit app open and some build/test activity, not an isolated benchmark
or a claim of real-time performance. Logs are in `build/portable/cockpit-profile-*.log`;
the sampled stacks are in `build/portable/cockpit-cpu-sample.txt`.

Regression coverage now checks steady NASSP frames retain the cockpit mesh,
partial vertex updates render correctly, rejected range batches leave the mesh
unchanged, and CLI profiling respects its warmup period. Existing live tests cover
DSKY texture promotion, interaction, and visible glare-shade animation.

Final validation: both normal and C++ ASan/UBSan builds succeeded. All 12 targeted
CTest entries passed in `build/portable-sanitize/profile-validation.log`, including
four live cockpit scenarios, stable topology, exact material-light restoration,
partial-upload rejection, promoted texture bindings, animation poses, and CLI
profiling. Rust is not sanitizer-instrumented. The normal app was rebuilt; reopen
it to load the updated renderer. All changes remain local and uncommitted.

## Optimized interactive PoC and wall-clock timing (local, September 29)

The earlier C++ diagnostic tree had an empty `CMAKE_BUILD_TYPE` (unoptimized),
while Corrosion selected Rust's dev profile. `CMakePresets.json` now includes
`native-poc`, using `RelWithDebInfo` and Rust release, with its own runtime and app
in `build/native-poc`. The actual build commands confirm `-O2 -g` for C++ and
`--release` for Rust. Floating-point contraction remains disabled.

A 190-frame CSM run averaged 23.9463 ms render time and 24.5921 ms total frame time
for 180 measured frames, versus the previous unoptimized 289.4 ms render time.
Another PoC instance remained open; these are local observations, not isolated
performance guarantees. See `build/native-poc/cockpit-profile.log`.

Unbounded interactive sessions now drive fixed simulation ticks from elapsed
wall time, independently of rendering. Catch-up is capped at 32 ticks per frame;
a single pause contributes at most 250 ms, and excessive tick debt is discarded.
This keeps input responsive after machine sleep or debugger pauses. Finite
`--steps` runs remain deterministic by default. `--realtime` explicitly paces a
finite windowed run. A real CSM run advanced 190 simulation ticks in 138 rendered
frames (128 after warmup), with 20.2546 ms mean rendering time; see
`build/native-poc/cockpit-realtime-profile.log`.

All 36 final optimized CTest entries passed in
`build/native-poc/realtime-full-tests.log`, including RCS, saved-state resume,
real cockpit interaction/pixels, and app launch. Realtime scheduling tests cover
20/41/60/144 fps, variable frame durations, suspension, and bounded catch-up. Both
new timing/CLI tests also passed under the existing C++ ASan/UBSan build. No
commits or pushes were made. The older running PoC was left untouched; the
quickstart now points to the optimized app.

## Laptop flight controls (local, September 29)

The PoC app now starts with an SDL laptop numpad mode, also available to CLI
launches with `--laptop-controls`. The physical `7 8 9 / U I O / J K L` grid maps
to DIK numpad 7–9 / 4–6 / 1–3, with M for zero, period for decimal, and slash for
divide. F6 switches back to ordinary keys. The title displays the active mode;
physical numpad keys remain available. This is shared native input code, with no
NASSP control shortcuts or per-platform renderer behavior.

The adapter records which DIK key each physical press produced, and counts
simultaneously held aliases. Releasing either a laptop or real numpad key cannot
cancel the other. Mode changes and focus loss release held controls; autorepeat
cannot silently rearm them. Window teardown also clears input.

`NativeSDL` verifies mappings, overlap in both release orders, mode changes,
autorepeat, focus loss, and recovery after host-side input clearing. `NativeRCS`
now also drives the actual CSM through SDL laptop events: all six translation
axes, slash mode selection, Option/Alt hardover pitch, focus-loss shutdown, and
mode-change release/damping passed. The focused tests passed in both optimized
and C++ ASan/UBSan builds. All 36 final optimized tests passed in
`build/native-poc/laptop-full-tests.log`. The quickstart includes the laptop
bindings. Changes remain local and uncommitted; reopen the optimized app to use
them.

## Catalog stars in the cockpit scene (local, September 29)

Added Orbiter's `Config/CSphere/Data/star.bin` to native runtime staging. The new
loader decodes the 14-byte little-endian records explicitly, validates truncation
and coordinates, and follows the existing `CelSphereAPI.cpp` coordinate, magnitude,
and spectral-colour mapping. The default `StarPrm` selects 41,057 records.
Missing or invalid catalog data is logged and disables stars without stopping the
cockpit.

The Rust renderer uploads one instance buffer for the catalog and draws it in one
batch before cockpit/planet geometry. Camera rotation and projection are supplied
without astronomical position. Star sprites keep their pixel size on resize, sit
at reversed-Z depth zero, and never write depth. Normal scene geometry occludes
them. Atmosphere, nebula/sky-background images, and constellation overlays are not
part of this pass.

GPU readback tests verify forward/rear clipping, off-axis projection, camera
translation invariance, rotation, fixed pixel size, distant-geometry occlusion,
and rejection of invalid uploads without losing the previous catalog. Catalog
tests exercise synthetic axes/colours/magnitudes, malformed records, and the real
file. Live cockpit tests check catalog loading. Visual inspection of the rendered
CSM window confirms stars behind its geometry; the capture is
`build/native-poc/csm-stars-window.png`. Set `ORBITER_COCKPIT_CAPTURE` to a PPM path
when running a live cockpit test to request panel and upward-view captures.

All 38 optimized tests passed in `build/native-poc/stars-full-tests.log`.
The catalog and GPU-star checks also passed in the C++ sanitizer build. A parallel
sanitizer cockpit run hit a picking assertion while another test suite opened
windows; running that cockpit alone passed (`stars-cockpit-recheck.log`). Desktop
test suites should run sequentially across build trees to avoid focus contention.
Rust is not sanitizer-instrumented.

A paced 190-tick CSM run with stars averaged 18.1082 ms render time in this run
(`build/native-poc/stars-profile.log`); this confirms interactive operation, not a
controlled performance comparison. All changes remain local and uncommitted.

## Saved cockpit viewpoints (local, September 29)

Native cockpit loading now distinguishes restoring the saved viewpoint (`-1`)
from explicitly selecting a seat. NASSP reports the resolved seat to the host,
keeping navigation and redraw routing consistent. Explicit selection works before
the first simulation tick. Invalid saved seats fall back to the left seat.

NASSP's explicit scenario `VIEWPOS` now takes precedence over the packed
`MAINSTATE`, regardless of line order; older scenarios without `VIEWPOS` still
restore their packed seat. This fixes nonzero seats silently returning to the
left seat during loading. Existing camera-angle math was retained.

The new native viewpoint test loads actual CSM scenarios in fresh processes,
checks both field orders and the legacy fallback, navigates before the first
tick, and verifies camera position/direction restoration for all 12 views after
mouse movement. It also saves the side-hatch viewpoint and reloads that save in
another process. All 39 optimized tests passed in
`build/native-poc/views-full-tests.log`; the focused viewpoint checks passed under
C++ ASan/UBSan in `build/portable-sanitize/views-tests.log`. Rust remains outside
sanitizer instrumentation. Changes remain local and uncommitted.

## Cockpit point and spot lights (local, September 29)

The shared renderer now samples the vessel's active cockpit-visible point and
spot emitters every frame, including referenced positions and intensities. Up to
eight emitters travel in the existing scene-draw batch; no per-light FFI calls or
cockpit mesh rebuilds are needed. NASSP's three floodlights and flashlight fit
within this limit. External-only, inactive, zero-intensity, and invalid emitters
are excluded. Coordinates stay vessel-local alongside the animated mesh vertices.

The shader applies diffuse and ambient colour, constant/linear/quadratic distance
attenuation, range cutoff, and spotlight cones. Apertures use the SDK's full cone
angles, matching the existing D3D9 conversion; equal angles produce a hard edge.
Material Light/Emission properties and directional sunlight remain supported.
This is still approximate lighting: no specular response, shadowing/occlusion,
HDR tonemapping, or illumination of other vessels/planets by these emitters.
Bright nearby lights can saturate the current RGBA8 output.

GPU readbacks check normal direction, intensity scaling, distance attenuation,
range, inner/outer spotlight behavior, equal-angle cones, local ambient colour,
and removal of lights. Native tests check visibility/activation filtering, live
reference updates, malformed values, and aperture conversion. Live NASSP cockpit
tests set its three actual flood emitters on/off, verify changed pixels, restore
the previous image exactly, and confirm the mesh allocation stays unchanged.

All 40 optimized tests passed in `build/native-poc/lights-full-tests.log`.
The final adapter/capture follow-up also passed. The light adapter, GPU mesh,
and live CSM tests passed under C++ ASan/UBSan in
`build/portable-sanitize/lights-tests.log` (Rust remains uninstrumented).
The framebuffer captures
`build/native-poc/csm-floods-on.png` and `csm-floods-off.png` were visually checked;
the test deliberately forces intensity 2 for a strong lighting contrast, so the
on image is overexposed. `ORBITER_COCKPIT_CAPTURE` now also writes paired floodlight
captures. Changes remain local and uncommitted.

## Camera-relative flashlight (local, September 29)

The flashlight is now exercised through SDL's actual F-key path. The initial
integration check exposed a roughly 2.6 mm position mismatch in the startup
fixture: NASSP sampled the camera during the simulation step, before subsequent
camera movement. Merely converting the camera's global position back to vessel
coordinates also unnecessarily loses precision at astronomical distances.

Native `LightEmitter` now offers `SetCameraRelative`: position and direction are
camera-local and the renderer resolves them into the vessel frame using the
current cockpit position and rotation, every rendered frame. NASSP's CSM
flashlight uses a zero camera offset and forward direction. Its existing F-key
toggle and activation state remain in NASSP; the native path skips the old
per-timestep global-coordinate conversion. Point floodlights stay vessel-local.

The cockpit integration test checks F on/off, visible beam pixels, camera look
changes without a simulation tick, and active flashlight recreation when moving
to the center seat. The adapter test separately checks rotated offsets and
directions and changes of camera position. The quickstart lists F and the
existing mouse-wheel/trackpad cockpit zoom controls.

All 40 optimized tests passed in `build/native-poc/flashlight-full-tests.log`,
including the four live cockpit scenarios. The startup and standalone CSM
flashlight tests and the light adapter checks also passed under C++ ASan/UBSan
in `build/portable-sanitize/flashlight-tests.log`. Rust remains uninstrumented.
Changes remain local and uncommitted.

## Native cockpit inspection pause (local, September 29)

F5 now requests pause/resume from the SDL adapter. The native host applies the
request at a simulation-tick boundary and sends the existing plugin `clbkPause`
notification once per transition. The SDK's `oapiSetPause` uses the same path.
Repeated identical requests are idempotent; two toggles before a tick cancel.
Held keys and captured cockpit mouse controls are released on transitions.

Paused ticks advance system time but skip vessel/plugin simulation callbacks
and physics. Camera updates and rendering continue; the title displays PAUSED.
The main loop consumes wall-clock ticks while paused without counting them toward
`--steps`, so resuming does not replay the paused interval. The flashlight, seat
navigation, and camera look remain available without running the AGC or mission
clock. The quickstart documents F5 and the need to press flight keys again after
resuming.

The native pause test uses the real CSM and verifies exact preservation of its
position, velocity, fuel, and complete serialized vessel/NASSP state across 120
paused ticks. It checks resume timing, SDL repeat suppression, key release, zoom,
the title, SDK requests, and plugin callback counts. Live cockpit tests now
exercise the flashlight, look, and seat changes while actually paused.

All 41 optimized tests passed in `build/native-poc/pause-full-tests.log`.
Focused pause/SDL and live CSM checks passed under C++ ASan/UBSan in
`build/portable-sanitize/pause-input-tests.log` and `pause-cockpit-tests.log`.
Rust remains uninstrumented. Changes remain local and uncommitted.

## Native lunar module cockpit (local, September 29)

The optimized preset now builds `Saturn5;LEM`. LEM uses its existing CMake source
list and links the shared native core, PanelSDK, resource map, and null sound
backend. Its original LGC and AGS engines and flight systems are compiled; no
replacement simulation was introduced. DirectInput and telemetry sockets are
excluded for now, while internal telemetry and MCC-buffer processing remain.

LM bitmap resources and panel backgrounds use the generated LM resource table
and surfaces. Extended cockpit material updates now use the shared SDK API;
the missing ordinary `oapiSetMaterial` forwarding export was also added to the
core. Native view restoration uses the same saved/default versus explicit seat
contract as the CSM, and the LM flashlight uses camera-relative lighting.
Mechanical fixes include an MCC forward declaration, a null pointer initializer,
portable min/max calls, and AGS integer literals and 64-bit scenario formats.
The embedded AGS no longer tries to change stdin's socket flags.

`tools/stage_nassp_smoke.py --lm` stages a separate `runtime-lm` with Eagle from
Apollo 11 PDI minus 15 minutes, using native Sun/Earth/Moon modules. Columbia is
omitted from this fixture. A finite native windowed run completed, and the
framebuffer was inspected (`build/native-poc/lm-cockpit.png`). The geometry,
FDAI, timers, gauges and DSKY render. Lighting is currently overbright in places;
the lunar surface is still a coarse untextured sphere.

The new integration test selects all twelve LM viewpoints and operates DSKY
VERB 35 / ENTER through SDL mouse events and native ray picking. The running LGC
echoes the command, shows PROG/VERB/NOUN 88, and changes the DSKY texture. A fresh
process reload check preserves AGS clocks above 2^32 exactly, along with mission
time and the DSKY viewpoint, then resumes simulation. AGC cycle-count resume has
the same existing NASSP limitation noted for the CSM; this is not a bit-exact
checkpoint of the entire computer execution state.

All 44 optimized tests passed in `build/native-poc/lem-full-tests.log`. The LM
cockpit and fresh-process reload tests also passed under C++ ASan/UBSan in
`build/portable-sanitize/lem-tests.log`, with no sanitizer diagnostics in the
test output. Rust remains uninstrumented. A complete
LM descent/ascent, staging, rendezvous, and combined CSM/LM scenario are not yet
validated. The quickstart includes a separate Eagle launch command. Nothing has
been committed or pushed.

## Combined CSM/LM cockpit session

`tools/stage_nassp_smoke.py --pair` now stages Columbia and Eagle together from
Apollo 11 PDI minus 15 minutes in `runtime-pair/Scenarios/NasspPair.scn`. F4 cycles
eligible vessels and reloads the active 3D cockpit; the window title names the
focused spacecraft. Each vessel restores its own selected seat. F5 pauses both
spacecraft, while cockpit selection and viewing remain available.

Focus handoff releases keyboard and mouse capture, clears manual-control levels,
and refreshes the departing spacecraft's hand controllers. This prevents the old
RHC/ACA command and AGC channel 031 inputs from remaining latched after switching.
The LM also recenters keyboard deflections when modifiers or attitude mode stop
those keys from applying. The integration test holds actual pitch inputs in both
vessels, checks that they reach their computers and release immediately on focus
loss, operates both DSKYs independently through SDL picking, and switches seats
and vessels while paused. It checks visual replacement and skips focus-disabled
vessels.

A separate fresh-process check reloads both spacecraft, retaining Eagle as focus,
both viewpoints and each computer's DSKY display, then advances the simulation.
It caught a pre-existing save loader precision loss: both addons parsed mission
time through a 32-bit float. Mission time now loads directly as a double, preserving
the saved fractional tick at lunar mission times. This does not remove the AGC
cycle-count limitation described above.

This verifies a combined cockpit session, not docking, descent/ascent or complete
mission staging. Lighting and lunar surface detail remain limited as above.

Verification: all 47 optimized tests passed in
`build/native-poc/pair-full-tests.log` (59.09 seconds). The combined cockpit and
fresh-process reload tests also passed under C++ ASan/UBSan in
`build/portable-sanitize/pair-tests.log` (42.57 seconds), with no sanitizer
errors in the test output. Rust is not sanitizer-instrumented. Changes remain
local; nothing was committed or pushed.

## LM manual descent propulsion

The real LM cockpit integration test now selects ENG ARM / DES and THR CONT / MAN
through SDL mouse events and native ray picking. It then starts the descent
engine, raises and lowers TTCA throttle, checks propellant consumption, and stops
the engine. The original NASSP DPS and control electronics produce 10% thrust
after ignition and about 53.4% after the tested throttle increase; the test does
not set thruster levels directly. Losing window focus releases throttle motion
without extinguishing the running engine.

Laptop numpad mode now maps right bracket to numpad add and left bracket to numpad
subtract. Together with M and period, these expose LM engine start/stop and throttle
without an external keyboard. F6 restores the ordinary bracket keys. The quickstart
includes the panel settings for this manual burn demonstration. This is not a
validated powered descent, landing, or automatic LGC guidance sequence.

The affected SDL, LM cockpit/propulsion and fresh-process reload checks all passed
in the optimized build (`build/native-poc/lm-propulsion-tests.log`, 3.85 seconds)
and C++ ASan/UBSan build (`build/portable-sanitize/lm-propulsion-tests.log`, 19.18
seconds), without sanitizer diagnostics. Rust remains uninstrumented. No changes
were committed or pushed.

## LM separation and descent-stage module

The native build now includes the original Sat5LMDSC module whenever LEM is
selected, including with older cached module lists. The CMake dependency ensures
that building the LM also builds the module it creates during separation. No
replacement descent-stage simulation was introduced.

The cockpit integration test arms the explosive devices, opens the guarded STAGE
switch with a right click and operates it through native SDL ray picking. It checks
that Eagle creates its descent-stage vessel, loses descent-stage mass, retains a
visible ascent cockpit and saves both vessels. The descent stage remains disabled
for focus switching. A fresh process reloads the separated state and advances both
vessels without creating duplicate stages.

This exposed an SDK crash: GetMeshVisibilityMode dereferenced a null mesh slot
left by DelMesh during staging. It now reports empty slots as invisible, preserving
the indices of surviving meshes. The integration test exercises this path through
the actual NASSP staging transition and renderer. This is in-orbit manual separation;
it does not verify ascent-engine ignition or a lunar-surface ascent.

All 47 optimized tests passed (`build/native-poc/staging-full-tests.log`, 64.55
seconds). The LM separation and separated-state reload checks passed under C++
ASan/UBSan (`build/portable-sanitize/staging-tests.log`, 27.94 seconds), with no
sanitizer diagnostics after the mesh-slot fix. Rust remains uninstrumented.
Everything remains local and uncommitted; nothing was pushed.

## LM ascent propulsion

The cockpit integration sequence now pressurizes the ascent helium system through
the left panel, separates the stages, selects ENG ARM / ASC and operates the
original manual engine controls. It confirms that the latched ENGINE STOP
inhibits restart, then releases it and verifies full ascent thrust, propellant
consumption and a relative velocity change away from the spent descent stage.
TTCA throttle commands leave ascent thrust unchanged, and ENGINE STOP shuts the
engine down. No thruster state is injected by the test.

A save made during the ascent burn reloads in a fresh process and resumes with
the APS armed, commanded on and its actuator open. The quickstart explains ascent
helium preparation and the need to release the latched stop button before a
restart. This validates in-orbit ascent-engine operation, not a lunar-surface
launch or an automatic guidance sequence.

The expanded LM cockpit and reload checks passed in optimized and C++ ASan/UBSan
builds (`build/native-poc/ascent-tests.log`, 4.48 seconds;
`build/portable-sanitize/ascent-tests.log`, 24.21 seconds), with no sanitizer
errors. Rust remains uninstrumented. Nothing was committed or pushed.

## Nearby spacecraft visible from the cockpit

The wgpu client now retains a visual for each vessel instead of keeping only the
focused cockpit. Mesh copies, GPU geometry, animation poses, material edits and
visibility caches belong to each visual. Device-mesh callbacks resolve the owning
vessel independently of the vessel being prepared for drawing. Focus changes
select cockpit or external mesh visibility without destroying the other vessel's
visual. Vessel deletion and session closure release the corresponding resources.

External vessel transforms subtract global positions in double precision before
converting to camera-relative floats. Their meshes use the same texture/material,
animation, lighting and reversed-Z scene path as the cockpit. All scene meshes
are submitted together; this does not introduce a separate renderer.

The combined integration test temporarily places the actual Columbia model about
90 metres outside Eagle's window while paused. It verifies 12,941 changed pixels
when Columbia's external visibility is toggled, exact image restoration, and
material changes routed to the non-focused vessel. Both original vessel positions
are restored before saving. The captured image was inspected at
`build/native-poc/nearby-csm.png`: the textured CSM is visible through the LM
window. This deliberately arranged rendering test does not validate rendezvous or
docking. The normal pair fixture retains its original PDI scenario positions.

Existing limitations remain: the LM panel lighting is overbright, the Moon is
untextured, and an external-camera UI is not implemented by this change.

Verification: all 47 optimized tests passed
(`build/native-poc/multivessel-full-tests.log`, 61.52 seconds). The expanded
non-focused-material and reload checks also passed
(`build/native-poc/multivessel-pair-tests.log`, 7.26 seconds). Focused CSM, LM
staging, combined visuals and reload tests passed under C++ ASan/UBSan
(`build/portable-sanitize/multivessel-tests.log`, 103.57 seconds) without sanitizer
diagnostics. Rust remains uninstrumented. Nothing was committed or pushed.

## Cockpit emission atlases and panel contrast

Mesh textures now load optional `_emis` companion atlases through the same VFS
and DDS path. The base surface owns the companion, including for writable panel
textures. Each mesh group carries both texture IDs across the C ABI; Rust retains
both texture allocations and rebuilds bindings correctly when a surface is
promoted to a writable allocation. Existing assets without an emission atlas
retain their previous material-only fallback.

The fragment shader applies material emission to the atlas instead of making the
entire diffuse texture self-illuminating. Diffuse illumination is bounded before
texture multiplication, preserving printed panel detail under overlapping bright
lights instead of clipping it to white. This remains a simple non-HDR lighting
model, without shadows or physically complete material rendering.

The LM framebuffer was inspected at `build/native-poc/lm-emission.png`. Grey panel
surfaces, labels and cyan timer/quantity digits are now distinct where the earlier
capture was washed out. GPU tests exercise masked emission, dynamic emission
updates, retained texture lifetime and diffuse contrast at high light intensity.
The flashlight integration test now disables floodlights while checking the beam:
a fully lit diffuse surface is no longer expected to brighten beyond its texture.
Its mouse-look, seat-change and paused-camera checks still run.

The full optimized run passed 46 of 47 checks; the remaining CSM flashlight check
passed after its lighting setup was corrected (`build/native-poc/emission-full-tests.log`
and `build/native-poc/emission-flashlight-tests.log`). No production changes followed
that full run. This supersedes the earlier reports of washed-out LM panel lighting;
other visual limitations such as the untextured lunar sphere remain.

Focused GPU, DDS, CSM cockpit/flashlight and LM cockpit/staging/ascent checks passed
under C++ ASan/UBSan (`build/portable-sanitize/emission-tests.log`, 67.21 seconds),
with no sanitizer diagnostics. Rust remains uninstrumented. Nothing was committed
or pushed.

## Lunar surface start and manual liftoff

`tools/stage_nassp_smoke.py --liftoff` stages Eagle from the original Apollo 11
Lunar Liftoff T-1m scenario into a separate `runtime-liftoff`, retaining the saved
spacecraft systems, surface position and native lunar ephemeris. Columbia and
mission-support vessels are omitted from this standalone fixture.

The new desktop integration check verifies initial ground contact, stable height
and near-zero surface speed with the engine off. Orbiter may serialize the
spacecraft as “Orbiting” during active ground-contact dynamics; this does not by
itself indicate flight. No simulation change was needed for that behavior.

SDL clicks operate MASTER ARM, ascent helium pressurization and ENG ARM / ASC.
The laptop engine-start key then triggers the original LM staging and ascent
control sequence. After twenty seconds, Eagle is about 299 metres above the
surface and no longer in ground contact, while its separate descent stage remains
in ground contact. The cockpit renders after the transition and both vessels save.
A fresh-process test checks continued ascent, mission time and stable descent-stage
height after reloading. This verifies a short manual surface launch, not a complete
guided ascent, orbital insertion or rendezvous. The lunar visual remains a coarse
untextured sphere.

The first sanitizer run reported an uninitialized collision-speed flag when the
scenario started already in ground contact. The three collision-update flags now
have explicit false defaults in VesselBase, covering both constructors. The
validation rerun uses `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` so this
class of diagnostic fails the test instead of appearing alongside a passing exit.

After the collision-flag fix, all 50 optimized tests passed in 78.96 seconds
(`build/native-poc/liftoff-full-tests.log`). The focused lunar launch/reload suite
also passed with ASan/UBSan and UBSan halt-on-error enabled (3/3, 23.09 seconds,
`build/portable-sanitize/liftoff-tests.log`), with no sanitizer diagnostics.
Rust was not instrumented. Nothing was committed or pushed.

The lunar liftoff check also reads the rendered frame after staging, hides Eagle's
meshes, and requires more than 10,000 changed pixels before restoring and checking
an identical cockpit image. This distinguishes visible cockpit geometry from a
bright lunar background. `ORBITER_LIFTOFF_CAPTURE=/absolute/path.ppm` optionally
writes the verified ascent frame for inspection.

This stronger check exposed a second empty-mesh-slot bug: SetMeshVisibilityMode
unconditionally dereferenced slots left empty by DelMesh during staging. UBSan
identified the null access at Vessel.cpp:3445. The setter now ignores deleted
slots, matching the getter's existing handling. The regression exercises both
visibility changes and restoration on the real staged LM.

Final verification after the visibility-setter fix: all 50 optimized tests passed
in 79.89 seconds (`build/native-poc/liftoff-visual-tests.log`), followed by all three
focused lunar launch/reload tests under C++ ASan/UBSan in 25.17 seconds
(`build/portable-sanitize/liftoff-visual-tests.log`) with no diagnostics and UBSan
halt-on-error enabled. The actual post-launch frame was inspected at
`build/native-poc/lunar-ascent.png`: cockpit geometry, the 3D attitude indicator,
switches and digital readouts remain visible. This checks rendering continuity,
not the flight accuracy of every instrument. No files were committed or pushed.


## ImGui over the native cockpit

The native build now compiles Orbiter's pinned Dear ImGui 1.92.5-docking into
liborbiter, sharing one context and allocator across the host, SDL adapter and
C++ graphics plugin. Its SDL3 platform backend handles mouse/keyboard events and
framebuffer scaling. The host composites ImGui after the 3D scene and before
presentation, including while paused. F1 opens a flight-controls panel with the
current spacecraft, pause/resume, focus switching and cockpit shortcuts. Its font
is the same bundled Roboto family used for Sketchpad's Arial/Sans mapping.

The graphics callbacks are implemented. All ImDrawData lists are flattened into
one C ABI geometry batch per output frame; Rust wgpu renders indexed triangles
with texture selection, vertex color/alpha, clipping and scene-preserving alpha
blending. Display origins and Retina framebuffer scale are applied separately.
16-bit ImDrawIdx input is widened, with per-list and per-command vertex offsets
retained so lists can exceed 64K vertices. The reset-state callback is a no-op in
this immutable batch pipeline; arbitrary custom draw callbacks are explicitly
unsupported. Multi-viewport platform windows are not enabled.

ImGui's dynamic RGBA font textures use ordinary Orbiter surfaces. Creation,
updates and destruction are handled through the 1.92 texture-status protocol;
dirty atlases currently upload in full. Orbiter surfaces can also be used in
ImGui image widgets. The Rust boundary validates index ranges, finite vertices,
texture handles and target feedback before submitting geometry.

SDL suppresses captured cockpit input, releases previously held flight controls,
and preserves global F1/F4/F5/F6 shortcuts. The displayed panel rectangle also
blocks pointer events before ImGui's next hover update, preventing a quick move
and click/scroll from reaching controls behind it. Closing the panel clears its
keyboard focus. The renderer remains shared across platforms; this addition
contains no per-OS graphics path. Existing Windows Orbiter dialog implementations,
HUD/MFD integration and broader mission functionality remain separate work.

Validation: all 52 optimized tests passed in 85.48 seconds
(`build/native-poc/imgui-full-tests.log`). The new GPU test checks alpha over an
existing scene, clip bounds at 2x framebuffer scale, multiple draw lists, nonzero
display origins, surface image widgets, more than 64K vertices, dynamic font
updates and renderer/font-texture restart. Invalid indices, unknown textures,
feedback and nonfinite clips are rejected without changing the output.

The desktop test uses real SDL events to open the panel over Columbia, click
pause, use F5 while the UI is focused, suppress flight controls and close the
panel. It also verifies that the first scroll over the panel cannot zoom the
cockpit through stale hover flags. The inspected framebuffer is
`build/native-poc/flight-controls.png`. A subsequent strengthened check confirmed
that clicking outside the still-open panel returns flight keyboard focus (2/2,
3.16 seconds, `build/native-poc/imgui-focus-tests.log`).

Eight focused tests covering SDL, ImGui, CSM cockpit interaction and lunar
liftoff/reload passed under C++ ASan/UBSan with halt-on-error enabled (88.87 seconds,
`build/portable-sanitize/imgui-focused-tests.log`). Their complete output is saved
in `build/portable-sanitize/imgui-focused-detail.log`; no sanitizer diagnostics
were present. Rust remains uninstrumented. Nothing was committed or pushed.

The strengthened click-outside focus test also passed under ASan/UBSan (2/2,
18.89 seconds, `build/portable-sanitize/imgui-focus-tests.log`), with no diagnostics.

### Native Orbit and Surface instruments

F2 now opens two supplemental MFDs over the 3D cockpit. The native host compiles
Orbiter's original Orbit and Surface instrument implementations and uses the
existing batched Sketchpad renderer for their graphs, text, horizon and tapes.
The original selection/input dialogs use the shared ImGui context. Modes belong
to each vessel, power cycling restores the previous mode, and focused-vessel
left/right MFD parameters use the original scenario format.

The SDK MFD2 registration, construction, drawing, button dispatch and destruction
path is connected and tested with an addon callback. This does not yet build
NASSP's Project Apollo MFD. Other built-in modes are omitted from the native menu;
legacy GDI MFD rendering and HUD output remain unsupported. The instruments are
floating windows, not newly installed textures in the historical Apollo panels.

Images recorded by ImGui retain their surface until submission finishes, even
when the instrument is replaced or unregistered during that frame. Selectors
close before their owning instrument disappears or focus changes. The private
SDK no longer packs MFDBUTTONMENU pointers into a 17-byte stride, which the Apple
Silicon linker rejected; its layout now uses natural pointer alignment.

The inspected capture is `build/native-poc/flight-instruments.png`.

All 54 optimized tests passed in 122.19 seconds
(`build/native-poc/mfd-full-tests.log`). The MFD integration test checks original
Orbit/Surface pixels, button-driven altitude mode, reference-selection callbacks,
per-vessel ownership, power-cycle mode restoration, registered MFD2 lifecycle,
and replacement after ImGui has recorded an image. A second process loads the
saved scenario, verifies Moon/altitude settings and reads back both rendered
instrument surfaces.

Fourteen focused tests passed under C++ ASan/UBSan with halt-on-error enabled
(160.94 seconds, `build/portable-sanitize/mfd-focused-tests.log`). These cover SDL,
LM controls, two-vessel focus/rendering, lunar liftoff, scenario reload, the flight
controls UI and both MFD tests. The complete output is retained in
`build/portable-sanitize/mfd-focused-detail.log`; no sanitizer diagnostics were
present. Rust remains uninstrumented. Nothing was committed or pushed.

### Original NASSP MFD plugin on the native host

The native build now includes NASSP's original ProjectApolloMFD plugin, including
Project Apollo Checklist. The macOS app loads it by default; the generic runner
accepts repeatable `--module <name>` arguments. Use `--module ProjectApolloMFD`
when launching or restoring an addon-MFD scenario from the CLI. F2 exposes both
registered mode names alongside Orbit and Surface.

This uses the existing MFD2 implementations and vessel connector. The original
Apollo 11 workbook supplies the flight-plan steps visible in the captured CSM
cockpit (`build/native-poc/nassp-mfd.png`). Both Columbia and Eagle can instantiate
their own displays. An integration test changes the CSM's checklist flashing
setting through the MFD, then verifies the change in the vessel's saved state.
It also exercises checklist navigation, the GNC page, invalid launch-time input,
and a fresh process restoring both addon MFD modes.

Port changes include pointer-width MFD callback returns, correct integer formats,
a dependent-template typename, generated bitmap-resource lookup for the logo,
and the existing ImGui input-box implementation exposed through the SDK. Network
uplink functions report unavailable without opening sockets. The native plugin
links the CSM and LM libraries and defines its own InitModule/ExitModule exports;
otherwise symbol lookup could select a linked vessel library's modern callback
before the plugin's old opcDLL entry point. No binary compatibility is retained.

Accepted MFD button/key actions now request a redraw, including while paused.
The UI resolves addon mode names from the registry. HUD output, other built-in
MFD modes, and the separate Apollo RTCC MFD remain unfinished. Individual advanced
Project Apollo pages and checklist-driven mission execution are not comprehensively
validated by the navigation/controller test.

All 56 optimized tests passed in 95.95 seconds
(`build/native-poc/apollo-mfd-full-tests.log`), including app startup with the
plugin loaded, paused page changes, vessel-controller mutation and reload.
The initial addon-only C++ ASan/UBSan run also passed (3/3, 42.26 seconds,
`build/portable-sanitize/apollo-mfd-tests.log`). Rust is not instrumented.

The final broader sanitizer run passed app startup and the flight-controls UI,
but macOS entered clamshell sleep during the instrument tests. Power logs show
sleep at 21:37:15 and maintenance wakes at 21:52:25 and 22:08:17, coinciding with
the two roughly 935-second timeouts. Both tests had printed their functional
success messages before CTest timed out. This interrupted run is retained in
`build/portable-sanitize/apollo-mfd-timeout-detail.log`; it is not counted as a
passing run. An isolated rerun of the final NASSP instrument test then passed
(2/2 including fixture, 31.35 seconds, `apollo-mfd-recheck.log`) with no sanitizer
diagnostics in `apollo-mfd-recheck-detail.log` in that build directory.

The remaining final-code instrument and fresh-process reload checks subsequently
passed (3/3, 86.07 seconds, `build/portable-sanitize/apollo-mfd-reload-tests.log`),
with no diagnostics in `apollo-mfd-reload-detail.log`. The full optimized suite
and all selected sanitizer checks are now verified across those runs. Nothing
was committed or pushed.

### CPU rendering for the original 2D FDAI

The updated priority is the 2D FDAI and the remaining L2/visual/mission work. HUD
integration is not the current focus. Dynamic VC texture updates, mouse picking
and basic local/emission lighting already have native implementations; terrain,
particles, sound, joystick support and broader mission verification remain open.

FDAI::PaintMe now runs on the native build. Its sphere is ray traced on the CPU
at the original 180x180 resolution using the original CSM or LM texture. The eye,
sphere radius, perspective and rotation order match the old OpenGL setup. GLU's
longitude/latitude convention is documented at
https://xorg.freedesktop.org/archive/X11R6.9.0/doc/html/gluSphere.3.html .
The native ball uses bilinear sampling of the original markings without the old
fixed-function lighting or 24x24 sphere faceting. Each changed image uploads
through oapiUpdateSurface; the existing bezel, rate/error needles, roll pointer
and LM power flag use the batched surface/Sketchpad path.

A shared CPU image loader uses stb_image and the core VFS. The two FDAI ball
assets are BMP files named with .dds extensions, so decoding uses their contents.
Native FDAI drawing resources are created lazily; ordinary 3D cockpit/headless
construction does not allocate unused ball surfaces. Existing attitude slew,
power gating, frozen needle values and scenario state remain in NASSP's FDAI.
Pixel coordinates now use int rather than LP64 long where the SDK expects int32.

`build/native-poc/fdai-cpu.png` shows CSM (top) and LM (bottom), each at level
attitude, tilted with displaced needles, and then without power. The integration
test runs the actual FDAI::PaintMe, uploads through wgpu and reads back the
composite. It verifies attitude/needle changes, frozen CSM indications and the
LM OFF flag. A separate CPU check uses a coordinate-encoded texture to verify
longitude, poles, revolution wrapping and invalid attitude handling, then loads
and renders both original assets.

This completes the native CPU instrument rendering path, not the full 2D panel
layout/presentation. The app still starts in the 3D cockpit; displaying the
original 2D panel layouts remains separate integration work.

Validation: all 58 optimized tests passed in 85.44 seconds
(`build/native-poc/fdai-full-tests.log`). The two new FDAI checks and their asset
fixture passed with C++ ASan/UBSan and halt-on-error (3/3, 2.49 seconds,
`build/portable-sanitize/fdai-tests.log`), with no diagnostics in `fdai-detail.log`.
Rust remains uninstrumented. The comparison image was visually inspected.
Nothing was committed or pushed.

### SDL joystick device backend (2026-09-30)

The native window now owns an SDL3 joystick backend. It enumerates attached
controllers, opens them, polls signed axes/buttons/hats, and closes disconnected
handles. Device names, GUIDs, serials and raw input snapshots are published in
`liborbiter`, so native vessel modules can consume one shared state without
linking their own static SDL instances. Shutdown clears the snapshot store.
These are raw main-thread snapshots, not authorized flight commands.

Validation: the complete optimized build passed. `native_joystick` uses an SDL
virtual controller to verify identity, both axis extremes, button press/release,
diagonal hats, hot unplug and shutdown cleanup. It and the existing `native_sdl`
keyboard/focus test passed (2/2). No physical controller was available for this
check.

Joystick flight support is still incomplete: NativeVesim still returns defaults.
Next work must connect named CSM/LM input bindings, axis calibration/deadzones,
and button callbacks. Before enabling those bindings, explicitly release input
on vessel/window focus loss, pause and UI capture, and verify reactivation does
not carry a held command into another spacecraft. Preserve keyboard controls and
avoid resetting the LM's manual throttle when no throttle axis is configured.

### Native VESIM binding engine (2026-09-30)

`NativeVesim` now implements named-input mapping instead of a defaults-only
container. The explicit native text format has one binding per line:

```
"RHC Roll" "device GUID or exact name" "optional serial" Axis 0 0 0.05
"Main DSKY PRO" "device GUID or exact name" "optional serial" Button 0
"THC Move CW" "device GUID or exact name" "optional serial" Hat 0 2
```

Use an empty quoted serial when not selecting by serial. Axis arguments after
the index are inversion (0/1) and the fractional center deadzone [0,1). Hat masks
are up=1, right=2, down=4, left=8; adjacent bits can select a diagonal. Device `*`
selects only when exactly one device matches the serial criterion. Ambiguous
identities disable that binding. Indices are zero-based. Comments start with
`#` on their own line or after a complete binding. Invalid rows report a line
number and preserve the previous configuration; duplicate input bindings are
rejected. A missing file clears bindings.

Axes map to the full NASSP 0..65536 range, centered at 32768. Buttons emit change
callbacks once per edge. Held controls must return to neutral before activating
on first connection, reconnection, or after `releaseControls()` / disabled
polling. Unplug releases active buttons. `hasActiveBinding()` distinguishes an
unmapped or unavailable axis from a legitimate zero reading, which is necessary
to preserve the LM's manual throttle.

Validation: full optimized build passed; `native_sdl`, `native_joystick`, and
`native_vesim` passed (3/3). Binding tests cover real shared-core snapshots,
inversion/endpoints/deadzone, diagonal hat input, callback edges, release/rearm,
reconnection, ambiguous devices, malformed config and config removal. These are
not yet spacecraft flight tests. CSM/LM still force `enableVESIM=false`; config
loading, per-axis spacecraft overrides and host focus/pause/UI release hooks
must be connected and tested before declaring joystick flight support complete.

### Native joystick spacecraft integration (2026-09-30)

The CSM and LM now register their original VESIM input tables and load optional
`Config/ProjectApollo/Vesim/CSM.native.cfg` / `LM.native.cfg` from the runtime.
Native axes override individual keyboard-derived controller components only
while an unambiguous mapped device is connected and armed. Original spacecraft
systems still process the RHC/ACA/THC/TTCA values and button callbacks. LM throttle
is overridden only when an assigned throttle axis is active, preserving manual
keyboard throttle otherwise. The legacy nonnative input branches are retained.

The shared core maintains release listeners; VESIM unregisters before destruction.
Host input release (including spacecraft handoff and pause), SDL window focus
loss and ImGui keyboard capture release callbacks and require neutral before
reactivation. SDL publishes raw device readings separately from permission to
apply those readings. F1 now includes a controller inspector with names, GUIDs,
serials and live indices, plus GUID copying for handwritten configs. See the
quickstart for examples and the non-centering throttle rearm behavior.

Verification:

- Full optimized build succeeded and all **66/66 CTests passed** in 101.37 s,
  including existing desktop, GPU, cockpit, MFD, FDAI and propulsion regressions.
  Log: `build/native-poc/joystick-full-tests.log`.
- Focused C++ ASan/UBSan input/flight suite passed **9/9** in 27.08 s with no
  sanitizer diagnostics. Log: `build/portable-sanitize/joystick-tests.log`.
- `nassp_joystick` sends actual SDL virtual-device inputs through both original
  spacecraft modules. It checks AGC/LGC hand-controller channels, mode-toggle
  callbacks, focus handoff, window loss, pause/resume, UI capture, held-button
  behavior, unplug/reconnect and keyboard fallback. It also starts the original
  LM DPS, changes throttle through keyboard input with VESIM enabled but no
  throttle binding, and shuts the engine down.
- `nassp_joystick_throttle` additionally maps a physical-axis equivalent to TTCA
  throttle and verifies actual descent-engine response, preservation of throttle
  after unplug and subsequent keyboard control.
- `nassp_joystick_flight` verifies both directions on all three translation and
  rotation axes through original CSM jets: commanded force/torque, propellant
  consumption and settling after release. Fixtures have separate runtime roots.

No physical controller was connected for these tests. Hardware-specific axis
calibration, real USB/Bluetooth hotplug behavior and non-centering throttle feel
remain unverified. This is joystick support for the PoC, not full lunar-mission
verification. Terrain, particles, audio, atmosphere rendering, shadows and the
other scope listed at the top remain open. Nothing was committed or pushed.

### Shared miniaudio mixer foundation (2026-09-30)

Added `Platform/Audio.h/.cpp` and one miniaudio implementation in `liborbiter`.
`AudioMixer` supports device output and deterministic offline stereo float output
at 48 kHz. Clips use case/slash-insensitive VFS paths, share engine lifetime,
update volume/loop settings without restarting an active sound, restart completed
one-shots, and detach on destruction. Mixer pause freezes the playback cursor;
clip gain and master volume multiply. No addon contains a private miniaudio copy.

The dependency is pinned to miniaudio 0.11.25,
`9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`. Sanitizers found two issues in the
initial 0.11.22 trial: signed-offset arithmetic in hashing and a failed-resource
cleanup use-after-free. The newer release fixes the hash arithmetic. Its cleanup
still reads a freed resource node, so `cmake/PatchMiniaudio.cmake` reproducibly
writes a private header with notification cleanup moved before the free and the
failed output pointer cleared. The upstream dependency checkout stays unmodified.
The patch fails configuration if its expected pinned source no longer matches.

Validation: full optimized build succeeded. `native_audio` passes in optimized
and C++ ASan/UBSan builds (including the miniaudio implementation) with no
sanitizer diagnostics. It checks missing/corrupt files, VFS path normalization,
mono-to-stereo decoding, independent mixed voices, volume, loop/end/replay,
pause cursor preservation, bounded stop tail, clip/mixer lifetime, and nonzero
PCM from the original NASSP `LMSuitFan.wav`. Sanitizer detail is in
`build/portable-sanitize/audio-tests.log`. No sound was emitted during these
checks; offline output deliberately does not open a device.

This is the mixer foundation, not enabled cockpit sound: NASSP still includes
`NullSound.h`. Next work must connect its XRSound calls to shared mixer-backed
sound banks, initialize output before vessel sound loading, apply focus/view and
pause policies, stage the sound assets, and verify original cockpit sound events.
Legacy mission `SoundEvent` sequencing remains stubbed as well. The device-output
path has compiled but has not yet been exercised on hardware. Nothing was pushed.

### XRSound-based native cockpit audio enabled (2026-09-30)

NASSP's `soundlib.cpp` now uses the native XRSound subset implemented in
`NativeSound.h/.cpp` in the shared core. Each spacecraft owns its sound IDs and
clips while sharing one miniaudio mixer. Missing/invalid loads fail honestly;
a successful ID replacement stops the old clip, while a failed replacement
preserves it. File-existence checks as well as decoding use the case-insensitive
VFS. The old unused `NullSound.h` was removed; absence of an audio session still
provides null behavior through `IsPresent()==false`.

`NativeAudioSession` is created before vessels and destroyed after them. Windowed
CLI/app runs enable device output by default; `--no-sound` disables it and
`--sound` explicitly enables it for headless runs. Initialization failure reports
on stderr and leaves the simulator running without sound. Runtime staging now
links the original NASSP `Sound` assets, including the default app runtime.
Simulation pause freezes the mixer and resume restores it. Playback policy is
updated on simulation/render frames and focus changes: internal-only sounds
follow focus/cockpit view; radio follows vessel focus; global sounds remain
unrestricted; external close/medium/far sounds use simple distance and pressure
attenuation, while wind uses pressure. Automatic XRSound default engine and
callout generation is not implemented (NASSP's disable requests are honored).

Validation:

- Full optimized build succeeded; **68/68 CTests passed** in 113.22 s.
  Log: `build/native-poc/sound-full-tests.log`.
- The mixer plus original-vessel audio checks and asset fixture passed **3/3**
  under C++ ASan/UBSan in 10.77 s, without sanitizer diagnostics.
  Log: `build/portable-sanitize/sound-tests.log`.
- `native_sound` verifies independent sound banks, replacement/failure behavior,
  real CSM `SwitchClick` through its VC seat-control callback, real LM
  `ButtonClick` through its buffered-key callback, nonzero mixed PCM, focus/view
  muting, simulator pause/resume and silent cleanup after vessel destruction.
- The manual `native_audio_device_test` opened the Mac's output device and the
  device callback consumed an original NASSP click at 20% volume to completion.
  This verifies device advancement, not a subjective listening assessment.

The legacy `SoundEvent` mission-audio path remains stubbed, so this is not full
mission-audio coverage. Physical controller verification, terrain/elevation,
particles, atmosphere rendering, shadows, complete 2D panels and end-to-end
lunar-mission verification remain open. The existing running app was not
restarted, and nothing was committed or pushed.

### Portable planetary archive reader (2026-09-30)

Prepared the existing shared C++ `ZTreeMgr` for native terrain loading. Archive
paths now use the VFS. Header and TOC fields are decoded as explicit little-endian
values rather than relying on host structure layout; the original texpack format
uses a 48-byte header and 32-byte node records, including trailing padding.
Both deflated and raw payload flags are supported. Lookup validates tile
coordinates, bounds its traversal, and handles absent roots/branches. Header,
TOC, root/child indices, offsets, file bounds and inflated lengths are checked;
individual payloads are bounded to 256 MiB. Failed reads clear the output pointer.
A file-position mutex allows concurrent tile reads while decompression happens
outside the lock. Loose elevation directories and file reads in `elevmgr.cpp`
now also resolve case and Windows separators through the VFS.

Validation: full optimized build succeeded. `tile_archive` and `native_world`
passed in optimized and C++ ASan/UBSan builds, with no sanitizer diagnostics.
Archive tests cover both hemispheres, child lookup, sparse trees with empty
parents, raw and compressed data, malformed headers/indices/offsets, incomplete
inflation, isolated corrupt payloads and concurrent readers. A sparse-file test
also read a real compressed tile beyond the 4 GiB file offset boundary in both
builds, without allocating a multi-gigabyte buffer. Sanitizer logs:
`build/portable-sanitize/terrain-reader-tests.log` and
`build/portable-sanitize/tile-archive-large-tests.log`.

These fixtures follow the on-disk layout written by `Utils/texpack/texpack.cpp`;
no real planetary `.tree` archive was found in the inspected checkout/dependency
paths. This establishes portable tile access, not rendered terrain. Elevation
payload validation/sampling, quadtree selection, texture/mesh residency and
renderer integration remain next. Planet visuals still use low-resolution
spheres. No commit or push was made.


### Validated elevation payloads and sampling (2026-09-30)

Added a shared terrain elevation decoder with explicit little-endian reads for
ELE version 1. It supports flat, signed/unsigned 8-bit and signed/unsigned 16-bit
payloads, converts scale and offset to metres, and checks headers, dimensions,
padding, payload length and finite values. Bilinear sampling respects the
south-to-north row orientation and the one-sample border around the 257-point
interior grid.

The native original simulation elevation manager now uses this decoder for
loose files and archive payloads. Loose paths are case-insensitive through the
VFS; a malformed loose payload can fall back to an archive. Conversion into the
legacy simulation INT16 grid checks target resolution and rejects overflow.

Validation: full optimized build succeeded. `native_world`, `tile_archive` and
`elevation_tile` passed 3/3 in both optimized and C++ ASan/UBSan builds, with no
sanitizer diagnostics. Tests cover all supported integer formats, scale/offset,
analytic ramp interpolation and orientation, padded boundaries, malformed data,
extended headers, mixed-case paths, target-resolution conversion, overflow and
compressed archive fallback through the original simulation manager. Sanitizer
log: `build/portable-sanitize/elevation-tests.log`.

Elevation modification overlays still use the legacy loader. Real planetary
archive validation, quadtree selection, terrain meshes, texture residency and
renderer integration remain unfinished; planets still render as low-resolution
spheres. This checkpoint does not establish visible terrain or complete landing
behavior. No commit or push was made.


### Elevation modification overlays (2026-09-30)

The native simulation now validates loose and archived Elev_mod payloads through
the shared decoder. Original flat, unsigned-byte and signed-16-bit modification
formats are supported, including the 255/32767 masks that preserve base samples.
Patches convert metres to the simulation resolution and apply atomically: a
truncated payload or an overflowing replacement cannot partially modify a tile.
Loose modification paths use the VFS, with archive fallback on rejected files.

The three focused elevation/archive/world tests passed in optimized and C++
ASan/UBSan builds, with no sanitizer diagnostics. Added checks cover masked
samples, negative heights, scale/offset, flat replacement, late overflow,
truncation and mixed-case modification loading through the simulation manager.
Log: `build/portable-sanitize/elevation-overlay-tests.log`. Real terrain assets,
quadtree selection and visible terrain rendering remain outstanding. Nothing
was committed or pushed.


### Terrain patch geometry (2026-09-30)

Added a terrain mesh builder for Orbiter's level-4-and-higher tile coordinates.
It produces elevated spherical grids, outward triangles and normals, texture UVs,
and a double-precision body-local origin with local float vertices. Elevation
padding supplies central-difference samples at tile edges. The renderer must
transform and subtract each origin in double precision before GPU submission.

The elevation test now checks topology, elevated lunar radius with millimetre
precision at level 14, outward normals and winding, and matching adjacent edges.
It passes in optimized and C++ ASan/UBSan builds without sanitizer diagnostics
(`build/portable-sanitize/terrain-geometry-tests.log`). This builder is not yet
connected to frame rendering: tile selection, residency, textures and renderer
integration remain next. No visible terrain claim is made by this checkpoint.


### Camera-dependent terrain selection (2026-09-30)

Added a bounded quadtree selector using projected grid spacing and conservative
spherical patch bounds. It refines the highest-priority patch first, retains a
complete non-overlapping planet cover, and defaults to 512 leaves and level 18.
Selection uses body-local double-precision camera positions; distant planets
remain two root hemispheres. Deterministic ordering avoids frame-to-frame changes
for identical inputs. Horizon/frustum culling is deliberately still absent.

Optimized and C++ ASan/UBSan elevation tests pass without diagnostics. Checks
include distant roots, fine selection near a lunar landing camera, pole/seam and
inside-body views, tile budget, exact coverage and absence of ancestor overlap.
Log: `build/portable-sanitize/terrain-selection-tests.log`. This is not yet used
by the renderer. Mixed-LOD edge handling, tile residency, textures and actual
frame integration remain necessary before visible terrain is complete.


### Terrain edge skirt geometry (2026-09-30)

Terrain patches can now include a configurable radial skirt around their entire
perimeter. The original surface vertices remain unchanged; additional lower
vertices retain border texture coordinates. This supplies geometry for hiding
mixed-detail cracks, but renderer integration must still choose appropriate
skirt depths and verify coverage visually with actual elevation data.

Optimized and C++ ASan/UBSan elevation tests pass without diagnostics, checking
perimeter topology, valid indices, preserved surface vertices and the requested
radial depth to millimetre precision on the lunar fixture. Log:
`build/portable-sanitize/terrain-skirt-tests.log`. Tile residency, textures and
frame integration remain unfinished. Nothing was committed or pushed.


### Adaptive terrain renderer integration (2026-09-30)

The wgpu planet path now draws selected terrain patches instead of the fixed
32-by-64 planet sphere. Meshes are cached by planet and tile address, with unused
meshes released each frame and all meshes released at session close/destruction.
Body rotation and camera-relative origin subtraction remain double precision
until the final float projection. Sun direction is transformed to body-local
coordinates for matching lighting. Elevation is sampled through the simulation's
original elevation manager (including validated base and modification loading)
when sources exist, with a temporary 16-tile cache during mesh creation. Sampling
normalizes the longitude seam and bounds polar coordinates. Missing elevation
sources yield the spherical surface. Surface textures are still absent; Earth
and Moon retain their previous flat colours.

Targeted build succeeded. Optimized elevation and GPU world tests passed. The
actual paired CSM/LM cockpit rendering test also passed after the final seam fix,
including focus changes and pixel comparisons; log:
`build/native-poc/terrain-render-tests.log` (2/2 including staging, 5.93 seconds).
This integration has not yet had sanitizer or real planetary-asset validation.
Loading remains synchronous, skirts use a provisional depth heuristic, and
terrain texture loading, visual seam inspection, culling and broader regression
checks remain outstanding. No complete-terrain or mission claim is made. Nothing
was committed or pushed.


### Integrated terrain sanitizer verification (2026-09-30)

Rebuilt the renderer, original CSM/LM modules and terrain tests in the C++
ASan/UBSan configuration. `elevation_tile`, pair staging and `native_pair` passed
3/3 in 49.45 seconds, with no sanitizer diagnostics. The paired test exercised
actual GPU frames, separate flight computers, focus/seat changes and external
spacecraft pixel comparisons. Log:
`build/portable-sanitize/terrain-integration-tests.log`.

A process sample during the longer run showed active original LM cockpit texture
loading near the end of the test, not a stalled terrain task. Rust is not
sanitizer-instrumented in this configuration. This closes the integration's C++
sanitizer check but does not validate real planetary elevation assets, textured
terrain, mixed-detail seam appearance or a full mission. Terrain surface texture
loading is the next implementation step. No commit or push was made.


### Terrain DDS loading path (2026-09-30)

Terrain mesh creation now tries VFS-resolved loose Surf DDS files and Surf.tree
payloads, respecting configured tile-source flags. Failed/missing fine tiles
fall back through ancestors to level 4; UV offsets/scales select the correct
region of ancestor imagery. Successful textures use the existing Rust DDS upload
path and mesh texture groups, with white vertex modulation. Without imagery the
previous planet colour remains. Textures are released with their terrain meshes;
archive readers are retained per planet until session cleanup.

The optimized renderer build and paired CSM/LM cockpit regression passed (2/2
including staging, 9.25 seconds), log:
`build/native-poc/terrain-texture-regression.log`. The staged fixture has no
planetary imagery, so this verifies missing-image fallback and cockpit regression,
not successful terrain texture rendering. Asset-backed texture tests and visual
inspection remain required. Loading is synchronous and parent textures are
currently uploaded separately for each requesting mesh; shared texture residency
is still needed. Legacy pre-level-4 global texture fallback is not implemented.
Nothing was committed or pushed.


### Shared terrain texture residency (2026-09-30)

Terrain meshes now share GPU textures by planet and source tile. Weak cache
entries locate textures already held by meshes; the last owning mesh releases
the GPU surface. Expired entries, including unsuccessful lookups, are removed
after selection changes, and session cleanup releases meshes before the GPU.
This removes duplicate uploads when several fine meshes use the same ancestor
image. It does not yet add asynchronous loading or persistent negative caching.

The optimized renderer build and CSM/LM regression passed, 2/2 including staging
in 9.07 seconds (`build/native-poc/terrain-residency-regression.log`). The fixture
still lacks planet imagery, so positive texture loading/sharing remains unproven
by this regression and needs a dedicated asset-backed GPU check. Nothing was
committed or pushed.


### Positive terrain texture GPU fixture (2026-09-30)

Added `terrain_textures`, which stages a separate paired-vessel runtime and
creates four synthetic BC1 DDS root tiles for Earth/Moon. It renders through the
real client and checks resident terrain diagnostics: all 514 selected meshes
were textured using four distinct shared GPU textures. Higher-level meshes thus
successfully use ancestor imagery. The original paired cockpit pixel/focus tests
also pass with the synthetic planet textures present. No existing source assets
are modified; the fixture refuses to overwrite staged asset symlinks.

Optimized build and GPU test passed in 9.02 seconds. Log:
`build/native-poc/terrain-texture-fixture.log`. This proves positive loose-DDS
loading, binding and shared parent residency in actual frame submission. It does
not prove geographic UV orientation visually or successful Surf.tree texture
loading; those and real planetary imagery remain to verify. Diagnostic counts
represent currently resident textures, not cumulative upload counts. Nothing was
committed or pushed.


### Compressed terrain archive GPU verification (2026-09-30)

Extended the isolated terrain texture fixture with two compressed Surf.tree
hemisphere nodes per planet. After the loose-DDS run, it replaces its synthetic
loose files with invalid 128-byte DDS payloads and reruns the actual cockpit
renderer. All 514 selected meshes remain textured from four shared archive-backed
textures, proving malformed loose-file fallback through archive decompression,
DDS upload, ancestor selection and mesh binding. Original cockpit checks pass
for both runs. The synthetic archive follows the original 48-byte header and
32-byte TOC-node layout.

`terrain_textures` passed in 16.01 seconds; log:
`build/native-poc/terrain-archive-gpu-tests.log`. Real imagery and visual UV/seam
checks remain outstanding; this test uses constant-colour synthetic textures.
Nothing was committed or pushed.


### Patterned terrain source equivalence (2026-09-30)

The terrain DDS fixture now uses distinct BC1 pixel selectors across rows and
columns rather than solid colours. The paired rendering test saves its initial
RGBA framebuffer in the isolated runtime, and the fixture compares loose-file
and compressed-archive runs byte-for-byte. All 14,745,600 bytes match, with four
resident textures shared by 514 meshes in both runs. Cockpit regression checks
also pass. Diagnostic wording now distinguishes resident textures from cumulative
upload counts.

`terrain_textures` passed in 16.38 seconds; log:
`build/native-poc/terrain-pattern-tests.log`. This establishes source-path
framebuffer equivalence, not absolute geographic orientation or terrain-pixel
sensitivity: a targeted visible-ground view is still required for those checks.
Real planetary imagery and mixed-detail seam inspection remain pending. Nothing
was committed or pushed.


### Visible terrain pixel verification (2026-09-30)

A texture-change check exposed a weakness in the previous initial-cockpit capture:
changing imagery changed zero pixels in that view. The terrain test now aims the
camera at the Moon and temporarily hides the focused spacecraft for a clear
terrain capture, then restores its visibility and cockpit before regression
checks. PPM and RGBA captures are saved in the isolated test runtime.

The revised fixture renders patterned imagery from loose files and archives,
checks exact framebuffer equality, then replaces loose imagery with white BC1
textures. All 3,686,400 pixels change in the Moon-facing frame. A captured pattern
frame was visually inspected and shows the synthetic terrain filling the view.
The complete GPU fixture and subsequent cockpit checks passed in 25.67 seconds,
log: `build/native-poc/terrain-visible-tests.log`. This establishes visible texture
sensitivity, not real lunar geography or mixed-detail crack coverage. Those
remain open. Nothing was committed or pushed.


### Elevation-to-framebuffer integration check (2026-09-30)

Extended the isolated GPU fixture with signed-16-bit ELE root tiles containing
a smooth synthetic lunar height field. With white imagery held fixed, changing
only elevation changes 2,503,756 pixels in the Moon-facing view. This exercises
the original simulation elevation loader, low-resolution fallback, mesh sampling,
normals and actual GPU frames together. The fixture removes its own generated
height files before subsequent runs and refuses to replace source asset links.

The complete four-run terrain fixture passed in 41.91 seconds, including texture
source equivalence, texture sensitivity, elevation sensitivity and cockpit
regressions. Log: `build/native-poc/terrain-elevation-gpu-tests.log`. Synthetic
height sensitivity does not prove real lunar data alignment, contact accuracy,
seam appearance or complete landing behavior; those remain open. Nothing was
committed or pushed.


### Cubic elevation boundary repair (2026-09-30)

The expanded C++ ASan/UBSan terrain fixture found a heap-buffer-overflow in the
original cubic elevation sampler (`elevmgr.cpp`). An inclusive north/east tile
edge could choose cell origin 256, then the four-point stencil read beyond the
one-sample border. The sampler now uses the last valid cell (255) with weight 1
at that endpoint. Added a focused cached-tile edge/corner regression; it passes
under ASan/UBSan without diagnostics (`cubic-boundary-tests.log`).

The earlier optimized elevation frame test did not detect this memory error.
The complete sanitizer GPU fixture is being rerun after the fix; its result is
not yet established. Failure log: `build/portable-sanitize/terrain-complete-tests.log`;
rerun log: `build/portable-sanitize/terrain-complete-fixed-tests.log`. Nothing was
committed or pushed.


### Sparse elevation lookup caching (2026-09-30)

The post-boundary-fix sanitizer GPU rerun exposed very slow repeated missing-file
lookups when an Elev directory exists but has no tiles. A live process sample
showed mesh sampling repeatedly entering VFS directory iteration from the
simulation elevation loader. Added a per-manager cache of unavailable tile
addresses, bounded to 8192 entries. Elevation assets are treated as static for
the manager/session lifetime; recreating the manager permits rechecking files.

The optimized targeted build and native_world/tile_archive/elevation_tile tests
pass 3/3. The full optimized build had also passed before this cache change.
The earlier sanitizer GPU rerun is still live on the previous binary, so the
cache improvement and complete boundary repair are not yet GPU-sanitizer verified.
Log: `build/portable-sanitize/terrain-complete-fixed-tests.log`. Nothing was
committed or pushed.


### Terrain sanitizer test scope and runtime (2026-09-30)

Both the old uncached rerun and subsequent cached four-pass sanitizer fixture
reached their 240-second test deadline. The cached run progressed past initial
terrain rendering and through additional inputs without a sanitizer report, but
neither timeout is a pass. The focused elevation test continues to pass.

Separated terrain capture mode from the independent complete `native_pair`
regression. Each terrain process now captures its Moon-facing frame, verifies
texture residency, restores the cockpit, and exits through normal cleanup. The
ordinary `native_pair` invocation retains all flight-computer, focus and material
checks. Both configurations rebuild successfully. The revised sanitizer fixture
is running; log `build/portable-sanitize/terrain-focused-tests.log`. Its result
remains pending. Nothing was committed or pushed.


### Terrain sanitizer checkpoint passed (2026-09-30)

The focused four-input terrain fixture now passes under C++ ASan/UBSan in 182.37
seconds without sanitizer diagnostics. It verifies shared loose/archived DDS
residency, exact source-path framebuffer equivalence, 3,686,400 pixels changed by
imagery and 2,503,756 changed by elevation. This rerun includes both the cubic
boundary repair and bounded missing-tile cache. Log:
`build/portable-sanitize/terrain-focused-tests.log`.

The independent optimized `native_pair` cockpit regression also passed (2/2
including staging, 9.56 seconds), log:
`build/native-poc/terrain-final-pair-tests.log`. The focused terrain mode omits
repeated cockpit regression steps, while the ordinary pair test preserves them.
Rust remains uninstrumented. Real terrain data/geographic alignment, mixed-LOD
seams and landing/contact checks remain open, alongside particles, atmosphere,
shadows and end-to-end mission work. No test processes remain from this checkpoint.
Nothing was committed or pushed.


### Cold-cache polar and longitude boundaries (2026-09-30)

Elevation queries now normalize equivalent longitudes to a common seam and
clamp latitude to the poles. Tile indexing keeps inclusive south/east boundary
coordinates within the valid band range. Nonfinite native query coordinates
return the flat fallback rather than entering integer indexing. The existing
polar comment was corrected to use +/-PI/2.

New tests query both poles and equivalent +/-PI and +/-3PI longitudes from fresh
managers/caches; the original warm-cache test could hide an out-of-range initial
tile lookup. The elevation test passes in optimized and C++ ASan/UBSan builds
without diagnostics (`build/portable-sanitize/terrain-coordinate-tests.log`).
Real geographic alignment and landing checks remain outstanding. Nothing was
committed or pushed.


### Terrain normal unit and latitude correction (2026-09-30)

Corrected simulation terrain normals to convert stored height increments to
metres using ElevationResolution. Cubic normals now use separate east-west and
north-south sample distances instead of their average, respecting longitude
convergence with latitude. Missing-data queries initialize the normal to local
up instead of leaving caller-provided values unchanged.

Added an analytic ramp regression at 60 degrees latitude with half-metre height
units for both linear and cubic interpolation. Optimized and C++ ASan/UBSan
elevation tests pass without diagnostics; log:
`build/portable-sanitize/terrain-normal-tests.log`. This checks slope inputs to
contact physics, not a completed landing/contact validation. Nothing was committed
or pushed.


### Consistent cubic terrain derivatives (2026-09-30)

Replaced the cubic normal's averaged-slope approximation with derivatives of the
same bicubic height surface used by elevation queries. Longitude derivatives are
interpolated across latitude; latitude derivatives use the already interpolated
row heights. Unit and latitude-distance corrections remain in place.

A curved signed-height fixture checks both normal slopes against independently
computed central differences of elevation at a non-grid position. Optimized and
C++ ASan/UBSan elevation tests pass without diagnostics; log:
`build/portable-sanitize/terrain-cubic-normal-tests.log`. This validates local
height/normal consistency, not spacecraft contact dynamics. Nothing was committed
or pushed.


### Elevated lunar surface contact and liftoff (2026-09-30)

Added an isolated terrain_contact fixture with flat lunar elevation 1,234 metres
above mean radius. It runs the original LM, verifies the difference between
mean-radius and ground altitude, checks unpowered ground stability, operates
actual cockpit controls to stage and ignite ascent, and checks that the descent
stage stays grounded at the elevated datum. Existing ascent/cockpit visibility
checks remain part of this fixture.

The GPU test passed in optimized (5.70 s) and C++ ASan/UBSan (28.78 s) builds with
no sanitizer diagnostics. Logs: `build/native-poc/terrain-contact-tests.log` and
`build/portable-sanitize/terrain-contact-tests.log`. This verifies flat elevated
surface placement, stability and liftoff. Sloped/rough-ground dynamics, touchdown
from flight, real lunar data and end-to-end mission validation remain open.
Nothing was committed or pushed.


### Particle stream ownership groundwork (2026-09-30)

Inspection confirms graphics-client stream creation still returns null. Vessel
stream deletion detaches the emitter and expects the renderer to retain existing
particles until they expire. Prepared the SDK base class for that ownership:
ParticleStream now copies its specification and has a virtual destructor so
renderer-derived stream cleanup runs through base pointers. This intentionally
changes the private experiment's SDK ABI; no binary compatibility is promised.

The rebuilt core/client and native_world test pass. A lifetime check verifies
independent specification storage and derived destruction through base ownership.
Particle simulation, emission mapping, GPU billboards, exhaust and dust rendering
remain unimplemented. Nothing was committed or pushed.


### CPU particle simulation groundwork (2026-09-30)

Added ParticleSimulation with double-precision inertial positions/velocities,
rate-based birth timing, inherited emitter velocity, directional exhaust speed,
lifetime expiry, size growth, level/density opacity mappings, and bounded particle
storage. Detachment stops births while existing particles continue ageing;
zero simulation time leaves them unchanged. Long steps retain recent emissions
within the particle cap.

The optimized particle_simulation test passes, checking emission counts, birth
positions, inherited motion, growth, pause, detached expiry, long-step limits
and logarithmic density opacity. This CPU model is not connected to graphics
callbacks or GPU billboards yet. Random velocity spread and atmospheric slowdown
also remain unimplemented, so there is still no visible exhaust or dust claim.
Nothing was committed or pushed.


### Particle spread and atmospheric motion (2026-09-30)

Extended the CPU particle model with deterministic per-emission velocity spread
and a density-dependent linear drag approximation relative to supplied atmospheric
velocity. Analytic integration of velocity decay and displacement avoids basic
Euler step-size dependence; newly emitted particles advance from their individual
birth times. Emission IDs preserve the same spread when a timestep is subdivided.

The optimized particle_simulation test passes, including matching positions and
velocities for one one-second step versus ten smaller steps with spread and drag
enabled. This is a first motion approximation, not verified against legacy
Orbiter particle appearance. Graphics callbacks and GPU billboard integration
remain pending. Nothing was committed or pushed.


### Renderer simulation tick hook (2026-09-30)

The CPU particle simulation passes C++ ASan/UBSan without diagnostics, including
spread/drag timestep subdivision checks (`build/portable-sanitize/particle-simulation-tests.log`).
Inspection found the native host did not call GraphicsClient::clbkUpdate. It now
calls the hook once after each simulation tick and camera update, passing the
running state, so renderer-owned effects can advance independently of presentation
frequency and recognize pause. The rebuilt optimized native_world test passes.
No graphics client particle callback/rendering is enabled by this change alone.
Nothing was committed or pushed.


### Vessel particle source adapter (2026-09-30)

Added NativeParticleStream, a renderer-owned adapter between the SDK attachment
API and CPU particle simulation. It transforms source positions/directions to
global coordinates, includes vessel translation and emitter rotational velocity,
uses atmospheric velocity/density, and reads live source-level pointers only while
attached. After detachment, particles continue with the last atmospheric snapshot
until expiry; Finished identifies streams ready for renderer cleanup.

The actual native-world fixture verifies transformed emitter placement, variable
level suppression and detached expiry. It passes optimized and C++ ASan/UBSan
builds without diagnostics (`build/portable-sanitize/particle-stream-tests.log`).
The adapter is not yet installed in graphics creation callbacks or rendered.
Nothing was committed or pushed.


### Live graphics particle streams (2026-09-30)

The wgpu client now creates generic/exhaust stream adapters, advances them from
its simulation-update callback, and removes detached streams after expiry. Stream
storage is bounded; invalid zero-rate/lifetime specifications remain unsupported.
Session/destructor cleanup releases owned streams. Pointer-based thruster sources
reverse thrust direction for exhaust, while fixed sources preserve the direction
already supplied by the vessel API. Reentry remains unsupported.

Optimized native_world and wgpu_world tests pass 2/2, including real graphics
stream creation, births during a host simulation tick and vessel detachment.
Clock checks now measure elapsed time relative to their starting point because
the stream test advances simulation first. GPU particle drawing remains absent;
this enables simulation/ownership, not visible exhaust. Nothing was committed
or pushed.


### Wgpu particle pipeline (2026-09-30)

Added a Rust instanced billboard pipeline and a checked 40-byte C ABI record
for clip-space centre, projected extent and colour. One upload accepts an entire
particle batch. The scene pass draws particles after opaque meshes using alpha
blending, reversed-Z depth comparison and no depth writes. A procedural radial
falloff produces soft circular particles without an external texture dependency.

The Rust/C++ build and native_wgpu Metal test pass, validating pipeline/shader
creation and existing renderer behavior. No stream batch is submitted yet, so
this does not establish visible particles or depth/blend pixel correctness.
C++ camera-relative projection, sorting and submission are next, followed by
particle-specific GPU readback checks. Nothing was committed or pushed.


### Particle batch submission and GPU depth checks (2026-09-30)

The client now projects live stream particles from double-precision global
positions relative to the camera, culls behind-camera/offscreen particles,
sorts back-to-front and submits one bounded GPU batch per frame. Particles fade
with age. Initial emissive/diffuse shading uses white/grey constants; texture
atlases and scene lighting for diffuse particles remain pending. Session close
clears the GPU particle batch.

The optimized client build and wgpu_mesh GPU test pass. New readback checks prove
opaque meshes hide particles behind them and foreground particles alpha-blend
with the opaque scene. This establishes batch rendering/depth behavior, but an
end-to-end original NASSP emitter appearance test is still needed. Reentry,
particle texture handling and refined dust/exhaust appearance remain unfinished.
Nothing was committed or pushed.

### Particle specification fidelity (2026-09-30)

Compared native emission with the original D3D9 particle implementation. Flat level and atmospheric opacity now use `lmin` and `amin`; zero-level sources stop births even with flat opacity, while existing particles continue ageing. The original 0.01 opacity emission threshold is preserved. Billboard half-extent now uses the full specified particle size. Updated synthetic fixtures to provide explicit atmospheric opacity and added shutdown/restart regression coverage. Optimized `particle_simulation`, `native_world`, and `wgpu_world` pass (3/3). Actual NASSP exhaust/dust appearance, custom particle textures, and reentry remain unverified or unfinished.

### Lunar dust vacuum opacity (2026-09-30)

NASSP LM dust used ATM_PLOG with amin=-0.1, an undefined logarithmic range rejected by the native simulator. Changed the actual LM specification to ATM_FLAT=1: existing thrust/altitude logic continues to control level, and dust can emit in lunar vacuum. LEM rebuild succeeds; a matching particle-spec regression proves vacuum emission and shutdown. This is simulation evidence, not yet a visible LM dust capture; the custom ProjectApollo/dust texture is still ignored by the initial particle shader.

### Particle texture plumbing (2026-09-30)

Native streams now submit custom surface texture IDs in the particle batch. Rust binds textures for contiguous runs without reordering transparent particles; untextured streams retain the white/radial fallback. GPU pixel tests verify custom color modulation and transparent texels, alongside existing depth/blend checks (`wgpu_mesh` passes). The batch ABI is now 48 bytes per particle. Orbiter particle atlas quadrant/orientation selection is still missing: this implementation currently samples the whole custom image, so NASSP dust appearance is not yet faithful.

### Particle atlas selection (2026-09-30)

Added stable per-particle variants for Orbiter’s 2x2 atlas, including its four rotated variants. The native client submits the selector for custom textures; Rust selects the quadrant in the vertex shader. Untextured particles keep the fallback. The ABI is now 56 bytes. GPU readback checks cover quadrant selection for all eight selectors; particle simulation and GPU mesh tests pass (2/2). Rotation within a nonuniform quadrant and actual NASSP dust appearance still need visual verification.

### Real LM dust stream integration (2026-09-30)

Extended native_lm to reposition the real LM briefly to 10 m lunar altitude during its cockpit-commanded DPS burn, then restore its orbit. Renderer diagnostics verify nonzero live particles, resolution of the custom dust texture for every live particle, and expiry after returning above the NASSP altitude cutoff. The staging and propulsion checks continue afterward. native_lm plus its asset fixture pass (2/2, 6.83 seconds). This controlled repositioning test proves stream integration, not a flown descent, touchdown, or visual dust appearance.

### Ground-relative LM dust sources (2026-09-30)

Replaced the active dust sources’ fixed vessel-local -15 m offset with an origin 0.1 m above the local ground datum and horizon-tangent emission directions. This prevents tilted/low LM attitudes from placing the emission origins under the rendered ground. The real LM regression verifies all four origins within 1 cm of the intended height and tangent directions within 1e-4 vertical component (the initial 1e-8 check measured 1.88e-5 and was unnecessarily strict). Native LM plus staging fixture pass (2/2, 6.39 seconds). Particle ground collision, terrain slopes across the plume, and visible dust appearance remain unfinished.

### Visible real LM dust (2026-09-30)

The real LM regression now renders a frozen observer view during the low-altitude DPS burn, compares it with particles hidden through a diagnostic toggle, and restores the cockpit before continuing. Eight real dust particles change 28,473 pixels against lunar terrain. Inspected capture: faint dust patches beside the LM; terrain has no surface imagery in this fixture. Capture: build/native-poc/lm-dust.png (regenerate with ORBITER_DUST_CAPTURE pointing to a PPM). native_lm and fixture pass (2/2, 8.35 seconds). This verifies visible textured emission, not realistic lighting, ground collisions, a cockpit-window dust view, or a flown landing.

### Planetary shadowing for diffuse particles (2026-09-30)

Diffuse particle brightness now tests the finite ray to the Sun against planetary spheres in double precision, with positions made relative before projection. Sunlit sprites retain the initial 0.6 diffuse brightness; eclipsed sprites use 0.03 ambient. Emissive particles stay at 1.0. Tests cover lunar day/night at solar-system coordinates, grazing light, emissive bypass, and occluders beyond the Sun. particle_simulation and the real native_lm regression plus fixture pass (3/3, 8.24 seconds). This is isotropic diffuse lighting with hard spherical occlusion, not terrain/spacecraft shadows, penumbrae, or physical dust scattering.

### Particle integration regression sweep (2026-09-30)

Rebuilt all native targets and ran all 73 CTests serially. 71 passed; wgpu_client and wgpu_present crashed because particle preparation queried scene/camera state before a scenario existed. Added an early empty-stream/no-camera-target exit that clears the GPU particle batch. Both failing tests plus native_lm and its fixture then passed (4/4). The presentation retest spent about 100 seconds in a macOS crash-recovery window-restoration prompt; a stack sample and desktop inspection identified it, and dismissing it let the same process complete. Logs: build/native-poc/particle-integration-tests.log and particle-empty-scene-tests.log. This is a 71-pass broad run plus successful affected retests, not a fresh 73-test sweep after the fix. Particle simulation also passes ASan/UBSan with no reported diagnostics (build/portable-sanitize/particle-integration-tests.log); Rust remains uninstrumented.

### Diffuse particle ground contact (2026-09-30)

Diffuse streams now constrain particle centres above the local ground plane, using vessel ground altitude and the simulation surface normal. Inward velocity relative to the moving ground is removed while tangential and outward velocity are preserved. Detached streams retain a linearly moving plane until expiry. CPU tests cover tilted contact at solar-system coordinates, tangential velocity preservation, outward motion and repeated-contact stability. The real LM dust image/propulsion regression and particle tests pass (3/3, 8.39 seconds). This is a local plane approximation, not per-particle terrain sampling or curved-body collision; gravity and long-plume contact fidelity remain unfinished.

### Airborne touchdown before lunar liftoff (2026-09-30)

Extended the lunar surface regression to lift the real LM two metres above its landed position, preserve global attitude, apply 0.2 m/s initial downward velocity, and switch to free flight. One metre was insufficient because the extended contact probes still touched ground. After 15 seconds the baseline LM settled at 3.61931 m ground altitude (initial resting height 3.617 m), minimum 3.56278 m, groundspeed 0.000575 m/s. The synthetic +1234 m terrain case settled at 3.61949 m with comparable results. Both then completed existing cockpit-commanded staging/liftoff checks. All three tests including staging fixture pass (11.57 seconds); log build/native-poc/touchdown-tests.log. This verifies short unpowered touchdown/contact settling, not guided powered descent, rough terrain landing, or an end-to-end lunar mission.

### Mission recording seek support (2026-09-30)

Added AudioClip::PlayFrom/Position and native XRSound::PlayWavFrom/GetWavPosition, using source sample rate and frame counts rather than assuming 8 kHz. Invalid/nonfinite/out-of-range offsets are rejected before stopping existing playback. Offline PCM testing seeks into the negative-valued second half of an 8 kHz WAV while mixing at 48 kHz and verifies cursor advancement. Native sound-bank integration verifies audible seeked playback on a real NASSP asset. native_audio passes; native_sound and its fixture pass (2/2). SoundEvent parsing/scheduling and its actual callout playback remain stubbed; these APIs provide the required offset/cursor backend.

### Per-instance landing callout data (2026-09-30)

Added native landing-table parsing and connected SoundEvent::LoadMissionLandingSoundArray/isValid/makeInvalid to per-instance vectors. Original Apollo 11 (83 events) and Apollo 12 (73 events) parse successfully, including type2 recording offsets, phase/mandatory fields, exact feet-to-metres conversion and bounded altitude interpolation. Malformed rows fail without replacing an existing table. Parser regression passes; rebuilt LEM/Saturn5 and native_sound integration pass. Inspection found the legacy LM activation still behind DIRECTSOUNDENABLED and passing mode=-1, so scheduling, playback activation and linking callouts to actual descent guidance remain unfinished. No mission-callout playback is claimed yet.

### Native callout scheduler and playback (2026-09-30)

SoundEvent now selects due landing events by pre-ignition time, braking elapsed/P64 time, approach altitude, or explicit MET mode. Selection skips stale optional calls, preserves mandatory calls within a phase, bounds end-of-table access and maintains per-instance progress. Its dedicated XRSound radio bank implements seeked playback, status, stop and segment-finish checks. Parser/selection tests pass; a new test links the actual LEM SoundEvent implementation, loads the original Apollo 11 table, selects ap11landingmono81.wav at its MET, verifies nonzero mixed PCM and stops at an offset boundary. All three checks including fixture pass (3.09 seconds). The LM automatic trigger is still disabled pending valid guidance phase/time inputs; this does not yet prove automatic descent callouts. LoadMissionTimeSoundArray remains stubbed (the CSM uses a separate timed-sound path).

### LM automatic approach callout hook (2026-09-30)

The LM now owns its native SoundEvent instance, loads sound.csv after mission sound paths are configured, and calls UpdateLandingApproach from post-step. Approach scheduling requires a powered DSKY displaying P64/P65/P66 and an attached descent stage; leaving this state stops its dedicated radio recording. The integration test places a real LM near the Moon and calls that same entry point with explicit P64 input, proving original recording playback and shutoff, plus negative gates for P63, unpowered display and ascent stage. Native LM and sound playback checks plus fixtures pass (4/4, 9.24 seconds). A real LGC-driven P63-to-P64 flight transition is not verified. Pre-ignition and braking audio remain disabled because reliable guidance countdown/P64 prediction inputs have not been connected. Callout progress persistence across save/resume also remains unfinished.

### Landing callout save/resume (2026-09-30)

SoundEvent serializes its versioned next-event index, played state, active recording cursor, loop and volume. Restore validates bounds, finite values, and the recording path against the loaded event table before replacing its bank; invalid data leaves current playback intact. LEM writes NATIVE_LANDING_AUDIO and defers restoration until its mission table is loaded on first timestep. Tests restore an active original Apollo 11 recording at its advanced cursor, reject invalid progress without disrupting playback, and verify no replay of the consumed event. LM save/resume also passes and its saved file contains the new state line. Five checks including fixtures pass (10.13 seconds). Active-callout restoration is verified directly through SoundEvent; the scenario regression currently carries idle callout state, not a saved active P64 descent.

### Headless powered-descent investigation (2026-09-30)

Added manual native_descent_probe (not a passing mission CTest), which advances the original PDI-minus-15-minute LM scenario, logs altitude/groundspeed/DPS/contact and DSKY state every 30 simulated seconds, then saves NativeDescentCheckpoint. Isolated runtime: build/native-poc/runtime-descent. Three 900-second probes (no input, PRO, and PGNS AUTO + DES arm + PRO) stayed in P63/V50 N18 with DPS off; the prepared checkpoint confirmed ModeControlPGNSSwitch=2 and EngineArmSwitch=0. At 900 seconds altitude was approximately 12.393 km and groundspeed 1701.76 m/s. Waiting ten seconds after load before the controls, then probing 120 seconds, also retained V50 N18. Logs: descent-probe.log, descent-proceed-probe.log, descent-prepared-probe.log, descent-settled-probe.log in build/native-poc. No P64/P66 scenario exists in this checkout’s scenario files. The next flight investigation is guidance/PRO acknowledgement and attitude maneuver readiness; this is not yet a verified PDI or landing. The latest checkpoint is from the 120-second delayed-input probe.

### Powered-descent attitude acknowledgement (2026-09-30)

The 900-second prepared descent probe completed with no DPS thrust and the LGC still displaying P63 V50 N18. A second PRO repeated the attitude maneuver rather than advancing guidance. Channel 032 telemetry confirms the cockpit PRO press reaches the LGC (`1000` -> `21000` -> `1000`, octal). The earlier pulse-delay hypothesis was incorrect: the virtual-cockpit push-switch callback bypasses the delayed switch path.

Read the original `Apollo 11 LM Checklists.xls` with NASSP's own BasicExcel parser. PDI rows 91-93 specify PRO for final trim, then ENTER, then N62. Updated the manual `native_descent_probe --prepare-pdi` to send ENTER when V50 N18 returns after the first maneuver. This diagnostic accepts the attitude early; it does not reproduce the checklist's PDI-minus-four-minute timing.

Build succeeded (`build/native-poc/descent-enter-build.log`). A 60-second native run completed successfully and advanced from V50 N18 to **V50 N25, R1 00500** (`build/native-poc/descent-enter.log`). This is a new guidance request to investigate, not proof of ignition or landing. DPS remained zero. Next: identify the 00500 request from guidance documentation/source, implement the required cockpit sequence, and continue through real powered descent.

### LGC-controlled descent ignition verified (2026-09-30)

The Apollo 11 AC Electronics reference identifies V50 N25 / 00500 as a request for landing-radar antenna position 1 (https://www.ibiblio.org/apollo/Documents/AcElectronicsApollo11.pdf). The staged PDI scenario has `PGNS_LDG_RDR_CB 0` and `LandingAntSwitch 2`. The probe now closes the radar breaker via VC area 4181 and moves the antenna switch to DES via area 3303. Saved state verifies both changes (1, 1). This clears 00500 without bypassing the radar check.

A 900-second run advances to N62 with countdown and reaches V99 ignition confirmation (`descent-radar-long.log`). Added a single PRO response to V99 through the same VC key callback. The subsequent 960-second run completes with original LGC guidance commanding DPS: level 0.1 at probe elapsed 900 s, level 1 at 930 and 960 s; DSKY P63 V06 N63. Groundspeed decreases from about 1702 to 1547 m/s. This proves guided ignition and initial braking, not complete descent/landing. Logs: `build/native-poc/descent-ignition-build.log` and `descent-ignition.log`.

Remaining mission work includes later checklist controls, radar incorporation, P64/P65 progression, and touchdown. Probe elapsed time still excludes its explicit key-hold/preparation steps; use the DSKY countdown for mission timing. All changes remain local.

### Terrain receives exterior local lights (2026-09-30)

Unified exterior light collection for vessel meshes and terrain tiles. Light positions are transformed into each tile's local frame using double-precision astronomical subtraction and tile-origin subtraction before float conversion. Range rejection limits irrelevant lights; shader attenuation and spotlight cones remain the same as for spacecraft.

Actual downloaded Earth terrain verified in runtime-pad: with Saturn V and ML meshes hidden, temporarily aiming one original floodlight downward changes 794 terrain pixels between lights-on/off. Original upward light directions are restored before the final capture; those lights naturally produce no visible terrain illumination in this camera view. Rocket/tower lighting still changes 45,909 pixels. Log: build/native-poc/ground-lights-render.log. CPU local-light check passes. No NASSP source or original light configuration changed in this step. This is illumination only; light-cast shadows remain unfinished.

### Original pad floodlights and ground camera (2026-09-30)

Enabled the original Floodlight module via CMake and staged its original six-light scenario block. No changes to Floodlight.cpp or its automatic day/night logic. The renderer now transforms exterior light positions/directions between source and receiving vessel frames in double precision, filters by visibility and range, and submits up to eight local lights per exterior draw. This enables a separate ground-light vessel to illuminate Saturn V and the mobile launcher.

Pad capture now uses an Earth ground observer with level horizon; staging writes the corresponding original Orbiter camera fields. The scenario starts at T-4 hours before sunrise, so the dark sky is expected (atmosphere still absent). The actual-texture pad render passes: six original lights initialized, disabling them darkens 45,909 pixels, and hiding the rocket changes 14,556 pixels. Updated capture: build/native-poc/runtime-pad/Columbia-exterior.png; log: pad-lights-render.log. Terrain does not yet receive these local lights, and light-cast shadows remain absent.

### Exterior spacecraft and first launch-pad render (2026-09-30)

Removed the renderer's virtual-cockpit-only scene update gate. External cameras now prepare all vessel exterior meshes, including the camera target; cockpit/exterior transitions rebuild visibility selection. F1 offers Exterior view / 3D cockpit. Native exterior regression verifies visible pixels disappear when the target's meshes are hidden, for both Columbia and Eagle, then restores the cockpit. DDS and exterior checks plus staging pass 3/3.

Added --pad staging for the original Saturn V and ML scenario entries with downloaded Earth textures. Enabled the original ML module through native CMake, changed one mixed-type max expression for Clang, and restored Orbiter's animation-state serialization helpers in the native API. No launcher sequence equations or behavior were rewritten. The first pad render exposed an unaligned BC2/BC3 DDS rejection; the renderer now decodes partial edge blocks to RGBA (aligned BC uploads remain compressed), with partial-block tests for all three BC formats.

The pad render completes and shows the original rocket/tower geometry (60,824 visible rocket pixels), but the captured scene is very dark and tilted. This is initial geometry/texture verification, not finished launch-pad presentation: camera framing, illumination, terrain seams and atmosphere remain to improve. Crawler, VAB, MSS and floodlight modules are not staged. Capture: build/native-poc/runtime-pad/Columbia-exterior.png. All changes local.

### Downloaded standard planet and sky assets (2026-09-30)

Following the bundled NASSP Quick Start Guide revision 1.4.3, downloaded its linked Orbiter 2024 package from https://cdn.openorbiter.space/orbiter-releases/Orbiter-2024.zip (2,872,278,397 bytes). Extracted Textures and Config/CSphere into `<LOCAL_PATH>`, outside both source repositories. Includes Earth/Moon Surf.tree and Elev.tree, Earth cloud/mask archives, other planetary textures, and celestial-sphere/Milky Way textures. No Windows executables installed.

Added optional `--textures` to staging and connected these assets to the main runtime-csm. Isolated runtime-texture-assets passed native_pair_test --terrain-assets: 514 meshes, all 514 textured, 325 resident DDS textures. Actual lunar crater imagery is visible in terrain-frame.png; thin tile seams remain visible and need correction. Standard-resolution assets are installed; optional high-resolution packs have not been downloaded. Catalog stars already render, while celestial-sphere background textures are installed but not yet drawn by wgpu. No NASSP simulation edits were made for this work.

Renderer also now suppresses direct solar illumination for spacecraft inside planetary shadows using the existing double-precision sphere occlusion calculation. Local lights and emission remain independent. This is an approximate hard planetary eclipse, not spacecraft self-shadowing or penumbra rendering.

### Earth sky and daytime launch-pad rendering (2026-09-30)

Scene depth attachments now belong to their render surfaces and are initialized lazily on first 3D use (2026-10-01). Repeated frames reuse the Depth32Float texture view; releasing a surface releases its depth attachment, and differently sized targets have independent attachments. Ordinary panel and DDS textures allocate no depth attachment. This removes the prior full-frame depth allocation on every scene render, without changing depth clears or atmosphere sampling. A GPU regression alternates 32×32 and 17×9 targets over three cycles and checks that farther geometry remains visible after a previous near-depth frame.

The renderer now retains the sunlight shadow atlas, comparison sampler and receiver binding across frames. Previously each scene render allocated a new 4096×2048 Depth32Float atlas (32 MiB) when shadows were active. Active and disabled-size resources are initialized lazily and reused; every shadow pass still clears depth, so moving or removed casters cannot leave stale shadows. Resources are owned by the GPU context and released with it. This removes allocation churn, not the cost of redrawing the cascades; no frame-rate improvement is claimed without profiling. The native build and all seven selected mesh-shadow, CSM cockpit, LM, exterior and staging checks pass (`shadow-reuse-build.log`, `shadow-reuse-check.log`). Existing GPU tests cover removal/disabling of casters, cascade transitions and alpha cutouts over successive frames.

Spacecraft material opacity now selects the transparent mesh path instead of opaque alpha cutout. An asset audit found 27 partially transparent definitions, including CSM window/hatch glass, LM COAS glass, antenna and crew materials. Positive alpha below 0.1 is no longer dropped; zero-opacity groups remain omitted. Transparent groups receive local sort centres computed from their vertices, and animation/device-vertex updates refresh those centres in one material batch per affected mesh. They use existing depth-tested blending and do not cast opaque sunlight shadows. The graphics-client regression renders overlapping materials at alpha 0, .05, .11, .5, .75 and 1 and verifies the actual framebuffer blend against an opaque background. Full build plus graphics-client/CSM/LM/exterior and staging checks pass 7/7 (material-alpha-build.log, material-alpha-check.log). No NASSP source changes. Intersecting transparent surfaces still use approximate centre sorting; coloured transmission shadows are not implemented.

Original spacecraft specular materials now reach the shared wgpu renderer. OrbMeshVertex is 96 bytes with specular RGB/Phong power; OrbMeshDraw is 824 bytes with a local camera position and enable flag. Sun highlights use the existing shadow visibility and planetary-eclipse lighting; point/spot highlights use light colour, attenuation and cone falloff. EnableSpecularReflection disables them dynamically. SetMeshMaterialEx/Specular reads and writes RGB/power through the owned visual material, preserving the source mesh template. This is conventional Phong lighting, without environment reflections or a metalness model. GPU checks cover intensity, camera direction, shininess, disabled/back-facing/eclipsed sunlight and local-light colour. Actual exterior checks measure 23,905 CSM and 14,284 LM highlight pixels and exact restoration after toggling the setting. Full build and mesh/CSM/LM/exterior checks plus staging pass 7/7 (material-specular-build.log, material-specular-check.log). No NASSP source changes.

Neighbour texture padding resolves the observed daytime pad foreground seam. Square, single-level BC1 terrain images with eight compatible neighbours now receive a half-tile border assembled by direct compressed-block copies; mesh UVs address the original central image, including ancestor-texture subranges. Missing/incompatible neighbours, polar rows, clouds and authored mip chains retain the original path. Longitude neighbours wrap. The pad trace verifies bordered level-18 tiles on both sides of the camera's nearby boundary; terrain-border-pad.png shows the sharp foreground discontinuity removed, with the Saturn V visibility probe passing (30,152 pixels). GPU checks verify identical black/white edge filtering from either side of horizontal and vertical borders; CPU checks verify exact block preservation/orientation and fallback. Full build and DDS/border/LM/terrain checks plus staging pass 5/5 (terrain-border-build.log, terrain-border-check.log). The source assets and NASSP logic are unchanged. This initial approach uses four times the base compressed texture area for padded tiles and additional neighbour reads; cache/loading optimisation remains. It does not establish seamless mixed-resolution or missing-neighbour boundaries. `ORBITER_DIAGNOSTIC_NO_TERRAIN_BORDER` restores the previous sampling path for comparison.

Foreground seam isolation continued with three actual pad captures. Disabling visual elevation (`ORBITER_DIAGNOSTIC_FLAT_TERRAIN`) leaves the seam visible; bypassing generated terrain mipmaps (`ORBITER_DIAGNOSTIC_NO_TERRAIN_MIPS`) also leaves it visible; disabling clamp-sampler anisotropy as well (`ORBITER_DIAGNOSTIC_NO_ANISOTROPY`) leaves it visible. These are process-start diagnostic switches; defaults and simulation elevation are unchanged. All three pad probes pass. Captures/logs use terrain-flat-diagnostic, terrain-source-diagnostic and terrain-anisotropy-diagnostic under build/native-poc. A decoded 3x3 level-18 source neighborhood (rows 5587–5589, columns 9045–9047) is saved as pad-source-neighborhood.png. Camera position maps to atlas pixel (986.23,516.59), just south of the y=512 tile boundary. Neighboring base-level texels differ across that boundary, particularly on the bright road. Cross-tile filtering/padding is the next hypothesis to test; the source-boundary diagnosis and its fix are not yet proven. Do not disable elevation, mipmaps or anisotropy as a workaround.

Transparent ordering now operates on mesh groups, with an explicit local sort centre in the shared 64-byte OrbMeshGroup ABI. Each exhaust flame and nozzle flare supplies its own centre while geometry and material uploads remain batched per vessel. Rust transforms centres into camera depth and interleaves the individual groups with sorted particle runs. A GPU fixture places smoke between two flames in a single batch and verifies the same result in either group-submission order; the far flame is hidden while the near flame blends over smoke. Full build plus mesh/DDS/LM/exterior and staging checks pass 6/6 (exhaust-order-build.log, exhaust-order-check.log). Sorting is still an approximation for intersecting transparent geometry and large smoke/cloud sheets. No NASSP changes.

Engine exhaust flames now render from the original VESSEL exhaust definitions, independently of particle trails. The C++ client batches camera-facing flame and nozzle-flare geometry per vessel, reuses GPU vertex buffers while topology is unchanged, and submits transparent, self-lit, depth-tested draws through the shared wgpu mesh path. Original textures, thrust levels, nozzle offsets, directions and sizes are used. Flicker is deterministic and does not consume the simulation RNG. Flame groups do not cast shadows. CPU checks cover direction/offset and side-on/end-on/coincident views; the actual LM DPS test verifies 72,799 changed pixels, exact hide/restore, and no stale flames after shutdown. The first test camera looked down through the hull and correctly saw no flame; the regression now uses an explicit side view. Full build and geometry/mesh/LM/exterior/liftoff checks plus staging pass 8/8. Logs: exhaust-rendering-build.log, exhaust-rendering-check.log. This is a billboard approximation; plume scattering, plume lights and fine ordering of overlapping flames remain unfinished. No NASSP source changes.

EnableParticleStreams now suppresses the submitted dust/trail batch immediately while preserving simulated particles. The actual LM dust test verifies disappearance, unchanged particle counts and exact restoration at fixed simulation time. Build and LM/staging checks pass (particle-controls-build.log, particle-controls-check.log).

The full LM probe also passes in runtime-lm-render with the downloaded Orbiter 2024 Moon and celestial textures: 74,869 flame pixels and 72,213 dust pixels. The low-altitude test placement now includes local terrain elevation instead of assuming a round Moon. Capture: build/native-poc/lm-exhaust-textured.png; log: exhaust-textured-scene.log. The original cockpit, LGC, DPS and APS checks all pass in that run.

Daytime Saturn V/mobile-launcher regression also passes after exhaust integration, with 30,151 spacecraft pixels distinguished from the background. Capture: build/native-poc/exhaust-pad-regression.png; log: exhaust-pad-regression.log. The foreground terrain imagery seam remains visible; this run checks the pre-ignition pad scene, not Saturn V ignition.

Atmosphere visual controls implemented: PrepareAtmosphere passes existing EnableHorizonHaze/bHaze and EnableDistanceFog/bFog settings to the renderer. The shader independently suppresses scattering on background-depth pixels or over opaque geometry; zero-valued flags retain the prior default. Atmospheric physics and approximate Earth skylight are unchanged. GPU tests verify disabling sky leaves surface haze intact, disabling fog leaves sky intact, and fog-disabled geometry matches the atmosphere-disabled baseline. Full build and mesh/CSM/staging pass 3/3. Logs: atmosphere-controls-build.log, atmosphere-controls-check.log. No NASSP changes.

User-facing view-control integration now covered by native_ui: actual SDL pointer/button events click Exterior view, verify external camera mode and >10,000 changed framebuffer pixels, scroll outside the panel to change camera distance, then click 3D cockpit and verify interactive cockpit areas remain registered. Initial failures came from test input still pending in ImGui's trickle queue; the test now drains that bounded queue between distinct gestures rather than guessing a delay. No application behavior change was needed. native_ui plus CSM staging pass 2/2 (view-controls-check.log); test target builds. No NASSP changes.

Full native integration sweep after terrain-boundary, mipmap, shadow and sky-layer changes: all 76 registered CTest cases pass sequentially in 156.54 seconds. Coverage includes shared core/SDK/modules, SDL and virtual VESIM input, Metal drawing/presentation, actual CSM/LM cockpits and flight-operation probes, save/resume, MFDs, CPU/native FDAI, sound, particles, loose/archive terrain and terrain-contact integration. Log: build/native-poc/rendering-integration-suite.log. This is integration evidence, not whole-goal completion: no end-to-end lunar mission has been established; physical controller behavior remains unverified; full legacy 2D panels and remaining visual limitations (including the foreground imagery seam) remain open. No runtime or NASSP changes were required during this sweep.

Catalog-star control now respects CfgVisualPrm.bUseStarDots / EnableBackgroundStars. Disabling clears the uploaded catalog once and resets residency/count; re-enabling reloads it. Star-image and Milky Way layers remain independently configured. Actual CSM regression disables both images to isolate catalog dots, verifies 10,520 changed sky pixels on disable and exact framebuffer restoration on re-enable; the independent star-image check still verifies 76,717 changed pixels. Full build and CSM/staging pass 2/2. Logs: star-controls-build.log, star-controls-check.log. No NASSP changes.

Downloaded star-image layer implemented: the C++ client now loads StarImagePath independently from CSphereBgPath, respecting bUseStarImage and bUseBgImage separately. Both use the legacy galactic orientation and compressed sky atlas. Rust adds a separate additive fullscreen star-image pass after the Milky Way and before catalog stars/opaque geometry; the existing atmospheric composite attenuates the combined background. Resource release clears both layers. GPU checks verify additive colour, independent enable/disable and foreground occlusion. Actual CSM check with ORBITER_CHECK_STAR_IMAGE=1 verifies 76,717 changed pixels with the Milky Way disabled; inspected star-image.png. Mesh/CSM/exterior checks pass 5/5 including staging; full build succeeds. Logs: star-image-build.log, star-image-check.log. Adaptive celestial minification/mips remain unfinished; this supersedes earlier notes saying the star-image layer is unimplemented. No NASSP changes.

Elevation-boundary coverage extended: new regression exercises latitude ownership with a detailed northern or southern child and coarse-parent fallback on the other side. Both interpolation modes, both cache warming orders and cold lookups agree at the exact boundary. This specifically covers the child-quadrant cache logic changed in the previous fix; the prior regression only covered longitude ownership. elevation_tile passes in optimized native and ASan/UBSan portable builds. Logs: native-poc/elevation-boundary-coverage-build.log, portable-sanitize/elevation-boundary-build.log and portable-sanitize/elevation-boundary-check.log. No further runtime or NASSP changes in this verification step; foreground imagery seam remains unresolved.

Terrain edge determinism fixed in Orbiter: elevation-cache hits now require the same TileIdx ownership as a cold query, including child-quadrant selection. Previously inclusive bounds allowed either cached neighbour to supply an exact boundary height. Terrain mesh coordinates now evaluate latitude/longitude from a common global-grid expression; independently rounded tile origins had placed nominally shared points on opposite sides of elevation boundaries. Regression tests cover both interpolation modes/cache orders with different neighbouring heights and all shared vertices of a discontinuous-height mesh edge. Full build and elevation_tile pass; terrain_textures and terrain_contact pass 2/2. No NASSP code changes.

Added opt-in ORBITER_TERRAIN_TRACE path, recording created mesh/source addresses, UV ranges and four body-local corners, plus tools/analyze_terrain_trace.py. Real pad trace found 405 Earth meshes and several source-resolution fallbacks. Before correction, three corner pairs differed by about 2 m; cache ownership alone did not solve the rounding issue. With both corrections, all 1,121 compared corner pairs at levels 14+ are within 0.00113 m, with none exceeding 1 cm. This is corner agreement evidence, not a proof that every mixed-LOD edge is crack-free. Pad integration passes; elevation-grid-pad.png inspected. The diagonal foreground imagery seam still remains and is not claimed fixed. Logs: elevation-grid-build.log, elevation-grid-analysis.log, elevation-grid-pad.log, elevation-border-regression.log. Trace snapshots: pad-terrain-trace.csv and pad-terrain-grid-fixed.csv.

Terrain mip generation implemented: new orb_gpu_dds_create_mipped entry point completes missing power-of-two BC1/RGBA levels at load time. Surface/cloud tile loading uses it; other DDS consumers retain their original behavior. Supplied compressed blocks and authored mip levels are unchanged. Lower BC1 levels use alpha-weighted box reduction followed by bounding-box endpoint/nearest-palette compression, and remain BC1 on the GPU; reduction works from unquantized intermediate pixels to avoid repeated compression error. This is a simple runtime encoder rather than a high-quality offline compressor. BC2/BC3 and non-power-of-two assets retain their supplied levels; no generated chain for those formats yet. GPU tests verify a highly minified checker averages to opaque grey, original base pixels remain exact, and authored blue mips are preserved over a checker base. Mesh/DDS/CSM checks pass 4/4 with staging; preservation tests and full build pass. Real pad comparison/exact restoration passes; inspected terrain-mips-after.png and foreground seam still persists. Logs: terrain-mips-build.log, terrain-mips-check.log, terrain-mips-preserve-check.log, terrain-mips-pad.log. The terrain_textures integration also passes, including exact loose/archive equality and imagery/elevation sensitivity (terrain-mips-archive-check.log). No NASSP changes.

Terrain archive inspection: added read-only tools/inspect_planet_texture.py to resolve geographic coordinates through Surf.tree and report the actual DDS dimensions/compression/image-level count. Ran against downloaded Earth at pad latitude 28.6083, longitude -80.6075, archive levels 10–18: all present, 512×512 DXT1, exactly one image level. Output: pad-texture-metadata.log. Reference Tilemgr2.cpp uses full [0,1] texture ranges, matching our basic UV convention, and optionally requests D3DX-generated box-filtered mip chains during load. Native dds.rs uploads only levels present in each file. This establishes missing generated minification levels as a concrete rendering gap, but does not prove it causes the foreground seam. Next correction must preserve direct BC uploads while adding filtered lower levels; geometry/adjacent texture content remain possible seam causes. No renderer or NASSP changes in this inspection step.

Terrain filtering: enabled 16× anisotropy on the clamped mesh sampler, retaining linear min/mag/mipmap filtering and compressed texture uploads. Inspected the real pad comparison against terrain-filter-before.ppm: distant ground detail is clearer, but the diagonal foreground seam persists, so this is a filtering improvement, not a seam fix. GPU texture/sky-boundary checks plus actual CSM pass 3/3 with staging; full build and daylight pad with unlit/shadow comparison and exact normal-frame restoration pass. Logs: terrain-filter-build.log, terrain-filter-check.log, terrain-filter-pad.log. Capture: terrain-filter-after.png. Sampler is also shared by clamped clouds and celestial background; existing sky-boundary GPU checks remain green. No NASSP changes.

Terrain seam lighting isolation: ORBITER_CHECK_TERRAIN_SEAMS=1 alongside ORBITER_CHECK_PAD_SHADOWS=1 adds a pad-unlit-terrain.ppm capture. The temporary ORBITER_DIAGNOSTIC_UNLIT_TERRAIN mode sets terrain ambient to one and removes local lights, retaining geometry, textures and atmospheric composition. Test uses portable SDL environment setters, restores the mode, and the existing exact normal-frame restoration check passes. Full build and real daylight pad run pass (terrain-seam-build.log, terrain-seam-diagnostic.log). Inspected pad-unlit-terrain.png: the diagonal foreground seam remains with both shadows and terrain lighting removed. This rules out terrain illumination as its source; texture sampling/LOD and mesh boundaries remain the next investigation. The diagnostic also makes other terrain bodies unlit (including the Moon); it is not a product rendering mode or a visual fix. No NASSP changes.

Pad ground-artifact isolation: added an opt-in ORBITER_CHECK_PAD_SHADOWS comparison to native_pair_test --pad-day. The renderer now suppresses its focused sunlight-map setup when both EnableShadows and EnableVesselShadows are false; individual caster-category filtering remains unimplemented. The test captures pad-no-shadows.ppm, requires visibly brighter pixels with shadows off, then restores settings and verifies exact framebuffer equality. Real daytime pad passes: 85,669 shadow-darkened pixels and exact restoration; full build passes. Inspected pad-no-shadows.png: the diagonal foreground terrain seam persists without shadows, ruling out the sunlight shadow map as its cause. Terrain geometry, texture LOD and per-tile lighting remain candidates; no stronger cause claimed yet. Log: pad-shadow-diagnostic.log. No NASSP changes.

Shadow focus stabilization: the sunlight-map centre is now snapped to the outer cascade's texel grid using a planet-relative double-precision position. Both cascades share this centre, so a grid step is one outer texel/four inner texels. Only the correction is added to the already camera-relative focus, retaining precision at astronomical distances. Existing global sunlight basis keeps camera rotation independent of the grid; depth coverage is unchanged. CPU checks prove sub-texel focus movement leaves the snapped centre fixed, one-texel movement advances it exactly and correction has no sunlight-depth component. GPU shadows, real CSM cockpit and exterior checks pass 6/6 with staging; full native build succeeds. Logs: shadow-snap-build.log and shadow-snap-check.log. Daylight pad integration also passes with 30,151 spacecraft visibility-difference pixels; refreshed Columbia-exterior.png inspected (shadow-snap-pad.log). This addresses translational grid shimmer, not all temporal aliasing: Sun-direction changes, extent changes and terrain LOD still change shadow samples. Static pad terrain artifacts remain a separate investigation. No NASSP changes.

Sky tile boundary filtering: fullscreen background samples now reproject bilinear footprint taps onto the sphere when a footprint crosses a tile edge. This connects actual neighbouring longitudes, differently subdivided latitude bands and the rotated southern hemisphere, instead of stopping filtering at each patch boundary or reading unrelated atlas storage. Interior pixels keep the single native bilinear sample; boundary pixels use four reprojected samples, preserving compressed textures and one draw. New GPU tests use contrasting tile IDs to verify longitude-edge blending, equator blending and the level-eight 30-to-28 longitude-count transition. Existing orientation/occlusion tests and actual CSM/exterior integration also pass (5/5 with staging); full build succeeds. Logs: sky-seams-build.log, sky-seams-check.log. Adaptive minification/mips and the separate star-image layer remain unfinished. No NASSP changes.

Celestial detail follow-up: replaced the overview-only loader with highest-complete-level selection for legacy sky archives (levels 5–8, with level-3 overview fallback). The downloaded Milky Way archive contains 501 DDS records; its 364 level-eight tiles are packed into one 8192×3072 BC1 atlas, retaining compressed blocks and using one fullscreen draw. The shader maps camera rays to variable-width latitude bands, pole-to-equator tile ordering and the rotated southern hemisphere. Sampling clamps inside each tile to avoid atlas neighbours bleeding across edges. No per-frame tile uploads or NASSP changes. CPU checks verify all 364 packed indices and overview fallback; GPU checks verify brightness, foreground occlusion, all four longitude quadrants in both hemispheres, and every level-eight latitude band. Actual CSM background/cockpit checks pass 4/4 with staging; additional GPU/live-cockpit/exterior regression passes 5/5 with staging. Full native build succeeds. Logs: sky-tiles-build.log, sky-tiles-check.log, sky-tiles-regression.log. Actual background capture inspected at sky-tiles.png. Remaining sky limitations: separate star-image layer, adaptive minification/mip filtering and cross-tile seamless filtering are not implemented. This supersedes the overview-only limitation below.

Celestial background: the wgpu client now reads the third whole-sphere DDS image from the configured legacy `.tex` archive and renders it behind catalog stars and opaque geometry. Camera rays use Orbiter's galactic transform; brightness and enable/disable follow existing visual settings. The initial 256-pixel overview is deliberately coarse; higher-resolution patch rendering and the separate star-image layer remain unfinished. CPU archive/truncation checks, GPU intensity/foreground-occlusion checks and the actual CSM scene pass 4/4 including staging. Toggling the downloaded background changes 267,784 pixels in the look-away capture. Full native build succeeds. Logs: sky-background-check.log and sky-background-full-build.log. The daylight pad integration also passes, with 30,199 Columbia visibility-difference pixels; refreshed Columbia-exterior.png inspected. Log: sky-background-pad.log. No NASSP source or simulation changes for this work.

FDAI requirement revalidated against the current graphics client: native_fdai, fdai_rasterizer and staging pass 3/3. Inspected the freshly captured CSM/LM zero-attitude, tilted and unpowered comparison at build/native-poc/fdai-current.png. The native FDAI.cpp path calls FdaiRasterizer::Render and oapiUpdateSurface; the instrument is no longer stubbed. NativeFdai exercises the original PaintMe path, needles, attitude mechanism, CSM state retention and LM off flag. Full 2D panel presentation/navigation remains separate unfinished work: NativeHost loads virtual cockpits, while legacy panel registration requires g_pane and currently returns without registering in the native host. Therefore the rasterizer requirement has direct implementation/test evidence, but this does not establish usable full 2D panels or overall goal completion.

Real CSM hull integration check reports 188 exterior shadow-only groups in cockpit mode. The complete CSM cockpit test passes with that membership assertion and a captured commander view; image inspected at build/native-poc/cockpit-hull.png. This verifies actual mesh inclusion and intact cockpit rendering, while the separate GPU test verifies the shadow-only colour/depth behavior. Log: cockpit-hull-csm.log.

Cockpit exterior-hull shadow support: exterior-only vessel meshes are now retained as shadow-only groups when building the cockpit visual. They share the same cloned meshes, animation updates, material updates and visibility events, but are excluded from opaque and transparent camera colour/depth draws. The shadow pass includes them with existing texture cutouts. Visibility changes now account for exterior caster membership as well as cockpit display membership. GPU checks place a caster in front of the camera and verify that it shadows the receiver without painting over it, then disable casting and verify sunlight returns. Native CSM/live cockpit, exterior and GPU checks pass 7/7 with staging; full native build succeeds. No NASSP simulation changes. Shadow accuracy still depends on the exterior mesh's window cutouts, finite shadow resolution and bias.

After the transparency merge, native_lm (including real NASSP textured lunar dust and visible-pixel comparison), particle_simulation and LM staging pass 3/3. Log: build/native-poc/transparency-order-lm.log.

Cloud/particle ordering correction: transparent meshes and particles now share one back-to-front render pass. Particle uploads retain axial depths; unsorted callers are sorted before texture batching. Rendering merges mesh-centre depths with particle depths and splits texture runs only at intervening meshes, preserving batched FFI uploads and contiguous draw ranges. GPU checks verify a cloud over distant blue exhaust and nearer green exhaust over that cloud, including deliberately unsorted particles sharing one texture run. Existing mesh/atmosphere/shadow and exterior checks pass (3/3 with staging), and the full native build succeeds. This is object-centre sorting, so intersecting transparent geometry and very large cloud tiles still need per-pixel or finer-grained treatment; no claim of exact transparency ordering in every scene.

Cloud regression sweep passes 19/19, including GPU text/surfaces/presentation, mesh depth and alpha blending, DDS, live/CSM/solar/insertion cockpits, stars, ImGui, native exterior and loose/archive terrain checks. Log: build/native-poc/cloud-regressions.log.

Real cloud captures inspected from space and the pad. `cloud-earth.png` shows the textured Earth and white cloud layer from the CSM; cloud-pad.log completes with 30,199 Columbia visibility-difference pixels. The current launch-date cloud imagery is mostly clear overhead with thin distant cloud bands near the horizon. Transparent clouds and particles are separate passes, so ordering between those two classes is not yet fully correct.

Cloud layer implemented using loose Cloud DDS or compressed Cloud.tree, separate from surface texture/mesh caches. The shell uses Orbiter's CloudParam altitude, CloudRotationAngle and cloud visibility setting; no NASSP changes. It selects up to 128 tiles through level 9, shares parent imagery, and uses no elevation/skirt geometry. Added per-group alpha blending with a shared explicit pipeline layout, opaque-depth occlusion, no transparent depth writes, and far-to-near mesh ordering. Transparent groups do not cast opaque sunlight shadows. GPU blend/occlusion and native exterior checks pass (3/3); full native build succeeds. Original CSM cockpit verification with downloaded assets passes and clouds change 3,272 Earth pixels when toggled off. Image: build/native-poc/cloud-earth.png; log: cloud-csm.log. Initial limitations: flat cloud shell, no cloud shadows or volumetric scattering, sorting by mesh origin rather than individual triangles, and the cloud layer renders after atmospheric compositing without its own distance-dependent haze.

Actual outer-shadow pad capture inspected (`shadow-outer-pad.log`, 30,199 Columbia visibility-difference pixels including its shadow). The long foreground tower shadow continues beyond the former abrupt inner-volume cutoff; the outer region is visibly coarser. Full native build succeeds. Capture: build/native-poc/runtime-pad-day/Columbia-exterior.png.

Extended focused sunlight shadows to two nested orthographic volumes, packed into a 4096x2048 depth atlas. The inner 2048-square region retains close spacecraft detail; the outer region is four times wider/deeper. Receivers blend near the inner boundary and fade at the outer boundary. Sampling is clamped within each atlas region to prevent cross-region filtering. GPU checks verify the same occlusion at inner coordinates 0.9, 1.0, 1.1 and 2.0, plus lit geometry outside both volumes; existing cutout and slope checks pass. Local-light/projection and native exterior checks pass (4/4 with staging). This extends finite coverage; it is not unlimited terrain shadowing or moving-focus texel stabilization.

Shadow camera stability correction: the initial Sun frame chose its reference up vector in camera coordinates, so camera rotation also rotated the shadow texel grid. The renderer now selects that reference in the global frame and transforms it into camera space. A CPU regression applies a 90-degree camera roll (including perspective matrix, focus and up-vector rotation) and verifies that all sixteen shadow-transform elements remain unchanged. The existing translation, aperture/aspect and overhead-Sun checks pass. This removes camera-dependent basis rotation; moving-focus texel snapping and cascades remain unfinished.

Shadow regression sweep: 19/20 selected checks initially passed. The CSM cockpit test failed its legacy blue-sphere colour assertion with actual Earth imagery (centre RGB 72,114,120; runtime-csm has the downloaded Surf.tree). Changed that assertion to planet visibility, retaining the opposite-camera black-space check; the complete CSM cockpit test then passed with its staging fixture (2/2). Other passing checks included presentation, mesh/shadows, DDS, LM/solar/insertion cockpits, stars, ImGui, exterior spacecraft and loose/archive terrain rendering. Logs: shadow-final-build.log and shadow-cockpit-check.log. No end-to-end lunar mission claim.

Corrected shadow-pad capture (`shadow-bias-pad.log`) inspected: slope-dependent bias removes the pronounced terrain self-shadow bands, and rocket/tower shadows are visible with diffuse sky fill in shaded ground. The finite shadow volume can clip long shadows; no claim of complete shadow quality or cockpit exterior-hull occlusion. Updated image: build/native-poc/runtime-pad-day/Columbia-exterior.png.

Added a first focused sunlight shadow map: 2048-square reversed-Z depth, orthographic volume centred on the focus vessel (half-extent max(50 m, four vessel radii)), texture-alpha-aware casting, bilinear comparison filtering, and constant plus slope-dependent receiver bias. Orbiter converts camera-relative mesh transforms into the Sun frame; no NASSP source changes. Direct sunlight is shadowed, ambient/emission/local lights are independent. Earth terrain now also receives the diffuse sky approximation. Full native build succeeds. GPU regressions cover an off-camera caster, retained ambient, alpha cutouts and sloped receiver self-shadowing; CPU checks cover focus translation, aperture/aspect removal and overhead Sun basis. Initial actual pad capture exposed severe self-shadow banding, prompting the slope-dependent bias. Limitations: a single local volume, no cascades/texel stabilization, no local-light shadows, and only currently rendered geometry casts (cockpit mode does not add an otherwise invisible exterior hull caster).

Actual daytime pad aerial-perspective capture verified in `aerial-pad.log` (17,598 visible Columbia pixels). The previously black distant lunar disk is now veiled by foreground atmospheric scattering; the nearby rocket/tower remains distinct. The horizon still has a dark terrain band and tile-detail transitions remain visible. Capture: build/native-poc/runtime-pad-day/Columbia-exterior.png.

Atmospheric aerial perspective now uses the stored reversed-Z scene depth. Opaque geometry renders first; a separate atmosphere pass samples depth to limit the integration to the camera-to-surface distance, then particles render with the preserved depth attachment. This permits foreground air to scatter light over distant terrain and celestial bodies while keeping nearby spacecraft clear. The pass assumes the engine's 0.01 m infinite reversed-Z projection. Daylight catalog-star contrast suppression only applies to pixels without opaque geometry; geometry uses approximate scalar transmission. GPU checks cover near/far surface haze, existing night/limb behavior and particle occlusion; exterior/staging checks pass (3/3 total). Limitations: RGB transmission is averaged for alpha blending, particles do not yet receive their own distance-dependent haze, and clouds/Mie/multiple scattering remain unfinished. No NASSP changes.

Real daytime pad horizon-culling verification: `horizon-pad.log` completes successfully, and the complete 11,059,217-byte PPM capture is byte-identical to the saved pre-culling capture (zero changed bytes). This establishes preservation of this launch-pad view, not all possible cameras.

Added conservative horizon rejection before Earth/Moon tile geometry and texture loading. The test uses an inner occluding sphere and outer relief sphere with a +/-32 km bound, and an angular bound for the entire tile; it keeps cameras below the inner sphere and leaves other planetary bodies unchanged. Tile selection still covers the complete sphere, but wholly hidden selected tiles are not built or drawn. Elevation tests cover near/far tiles, underground cameras, and elevated visible points at tile centres, edges and corners for equatorial/polar/orbital viewpoints. Exterior and terrain integration checks pass. The terrain fixture now permits omitted hidden hemisphere textures while retaining coverage, shared-texture, loose/archive image equality and image/elevation sensitivity checks. Terrain integration ran in 14.94 s versus 20.95 s before this change (single runs, not a benchmark). Custom Earth/Moon relief beyond +/-32 km requires a larger bound; frustum culling and allocating the refinement budget only to visible terrain remain unfinished.

Terrain texture edge correction: independent planet tiles previously used the repeating spacecraft sampler, causing bilinear filtering at a tile boundary to include pixels from the opposite side of that image. Added a per-group clamp flag across the C ABI, a clamp-to-edge sampler for terrain, and preservation of the sampler choice when surface bindings are refreshed. Vessel groups retain repeating UVs. A GPU regression uses opposite red/blue image edges to prove terrain clamping and retained spacecraft repeat behavior; mesh/exterior/staging checks pass 3/3. The terrain_textures integration check also passes, covering loose/archive frame equality and imagery/elevation sensitivity. Actual daylight pad capture completes (17,598 Columbia pixels): thin wrap-bleed lines are reduced, but broad changes in texture detail between tiles remain visible. This addresses texture wrap bleed, not all elevation or LOD seams. Log: build/native-poc/terrain-clamp-pad.log.

Exterior vessel materials now receive approximate integrated Earth skylight: scalar ambient rises from 0.03 to 0.25 in daytime near the surface, fades exponentially with 8 km scale height, and transitions through twilight using solar elevation. This improves shaded rocket and launcher visibility without applying ground daylight fill in orbit or at night. It is a scalar diffuse approximation, not directional sky illumination or global illumination. CPU checks cover day, night, twilight and orbital altitude; local_lights plus exterior/staging pass 3/3. No NASSP logic changes.

Added a Rust/wgpu single-scattering Rayleigh atmosphere pass using camera-relative Earth coordinates in kilometres. It draws after catalog stars and before opaque geometry, supports a blue ground sky and orbital limb, and includes planetary occlusion of sunlight. Display-space star contrast is suppressed under a bright sky; this is an approximation pending shared HDR exposure. No NASSP simulation source changes in this step.

Added staging option `--pad-countdown` for NASSP's original Apollo 11 T-20-minute scenario, preserving its saved date and spacecraft state, and `native_pair_test --pad-day` for daytime exterior checks. The original daytime rocket/tower render completed with 17,514 visible Columbia pixels. GPU regressions cover blue daylight, dark night, stars visible at night but suppressed in daylight, orbital limb, foreground occlusion, and disabled atmosphere on airless scenes. Mesh and exterior tests pass, including staging.

Remaining visual limitations: no cloud rendering, atmospheric aerial perspective on distant meshes or terrain, Mie scattering, or cast shadows. Distant opaque celestial bodies therefore do not yet receive foreground atmospheric scattering. Terrain tile seams remain visible. The downloaded celestial-sphere backgrounds are not yet drawn. This is initial atmospheric rendering, not a complete atmospheric lighting model.

### Approach failure and radar comparison (2026-09-30)

Extended the guided run to 1650 seconds (`build/native-poc/descent-approach.log`). It reaches P64, but impacts the surface by elapsed 1380 seconds at roughly 137 m/s groundspeed while the LGC reports about 7916 ft altitude. Subsequent stable ground contact is **not a successful landing**. This probe had not yet performed the checklist's radar incorporation or later approach controls.

Added actual accumulated simulation time and horizon vertical-speed telemetry to `Tests/NativeDescent.cpp`, including time spent in cockpit key holds. Added the original checklist's V16 N68 input at roughly ignition +5 minutes to inspect radar/guidance disagreement before accepting radar updates. Build and 1230-second run complete (`descent-radar-comparison-build.log`, `descent-radar-comparison.log`). N68 R3 remains +99999 at actual ground altitudes 7587 and 6385 m; no V57 acceptance is sent. Next investigation: radar range/velocity validity, pointing and LGC data transfer; determine why the comparison is unavailable before approving incorporation. No guided touchdown has been verified.
