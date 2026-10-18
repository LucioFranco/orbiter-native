# Mac demo validation after the X-pointer fix

The tested X-pointer fix is committed as `e9cc62d96440569c8b844080ee993dfd65c753f3`
on `macos-clean-demos`, after clean-checkout commit `d621bd4`. Windows comparisons
remain deferred. These follow-up changes affect the native engine and tests;
NASSP source, ROMs, equations, meshes, scenario inputs and adaptation stacks are
preserved. Final build/test evidence is recorded separately in
`upstream/validation-demo-macos-2026-10-07.json` after both suites pass.

## MCC displays

The native host previously retained MCC's four `VCMFDSPEC` registrations but did
not expose or bind their instrument surfaces. The engine now supplies the
existing GraphicsClient MFD getters and maps the registered mesh/group to its
current display surface. Screen pixels emit their original instrument colors;
powered-off screens are black. Mode changes update the GPU group bindings
without rebuilding spacecraft geometry. Changing cockpit layout or focus
rebuilds the appropriate layout. Source meshes are untouched.

Default cockpit selection retries original view 0 for vessels such as MCC that
reject view -1. All registered displays appear in the F2 mode picker, in two
columns, with scrolling to keep extra displays reachable on a laptop. All twelve SDK slots can be restored from their original save headers,
including MCC's third/fourth screens; numeric headers match exactly and accept
CRLF. The display checks hide each original screen independently, require
powered pixels in that screen's own visible region, and require exact image
restoration. They also require four different Surface settings (SPDMODE 1/2/3/4), four
distinct surface handles and four different instrument images. Changing any one
slot to Orbit must change that physical screen while the other three slots keep
their settings and pixels; restoration must reproduce the original image. These
checks repeat after focus changes and fresh-process restoration.
The picker check settles initial ImGui layout, requires viewport fit and dispatches
an actual SDL wheel event to reach displays 3/4; top and scrolled captures are retained.
The preserved pre-binding renderer fails with zero powered display pixels.

To try MCC, launch the original insertion demo, use F4 to select MCC, select the
3D cockpit and open F2. Choose **Surface** or **Orbit** for each of the four
screens; scroll the picker to reach displays 3/4. The separate **Apollo RTCC MFD** plugin has not joined the native build;
MCC's original power-button shortcut targets that unavailable mode. This does
not affect the verified MCC vessel/mission backend or the F2 display picker.

## Demo audit coverage

Staging refreshes helper-owned links to the selected build and explicit asset
roots. It rejects regular/unknown asset files and foreign directory links before
copying into a reused output. `native_data_preflight` deliberately plants foreign
Modules/Resources links and a user-owned mesh, verifies the collision leaves them
untouched, then checks that stale symbolic links are refreshed. `mission-source.json`
and `demo-preflight.json` record actual resolved runtime bindings, verified asset
links, executable/module SHA256 hashes and generated resource SHA256 hashes.

`NativeDemoLaunch.py` is called by both original insertion/liftoff gates. It runs
the helper's documented graphical argv with `--steps 12 --profile` appended,
checks Main's actual window/VC/WgpuClient/ProjectApolloMFD/render/save path,
reopens in a fresh graphical process with zero steps to compare every vessel,
AGC/LGC memory/channels and mission time, and renders/continues another 0.2 s.
The exact three commands are retained in `graphical-launch-verification.json`.
This closes the bounded graphical CLI coverage gap. **Normal user window-close
interaction remains an open live CUA check** because this executor has no
computer interaction tool. Step-limit exit uses the same final save block but
does not prove a user's close gesture. Physical-controller/manual-flight QA is
also not claimed.

## Original lunar descent diagnostic

The old standalone probe used a simplified planetary configuration without the
explicit original-mission terrain package and omitted later pilot actions. Its
large N68 disagreement/impact cannot establish an engine guidance defect.
Telemetry now includes range/velocity validity, antenna position, channels
012/033, and the original range-beam geometry for both antenna positions.
The beam initially points away from the Moon; it subsequently obtains valid
range/velocity data. Full original mission/planetary data substantially reduces
the earlier altitude disagreement.

The diagnostic now permits explicit V57 radar incorporation while N68 is valid
and its displayed disagreement is inside a caller-selected bound, followed by
original HOVER antenna/KEY REL and approach PRO controls. Baseline observation:
P63 -> P64 -> P65; first contact at 1605.83 simulated seconds, vertical speed
-0.894745 m/s and total groundspeed 0.932137 m/s. The original cockpit engine
STOP button shuts down DPS. Direct fresh-process continuation retains physical
contact, about 3.51 m ground altitude and near-zero speed with DPS off. Orbiter
can serialize this active contacting vessel as `Orbiting Moon`; the gate checks
actual contact, height, velocity and thrust instead of requiring a label.

This remains a **simplified diagnostic pilot sequence**. It accepts the initial
attitude early, omits the checklist yaw/manual-landing exercise, and uses an
explicit 2000-foot radar bound. That bound is not an independently certified
operational limit. Soft contact is a Mac execution result; complete checklist
fidelity, historical landing-site accuracy and Windows trajectory parity are
not established. No guidance constants or scenario state are retuned.

The gate requires a soft *first* contact, shutdown, every original vessel,
LGC memory/channel preservation on fresh load and ten seconds of continued
physical contact. The visual gate reloads that checkpoint, checks both crew
X-pointers, retains P65 and stopped DPS, then executes the original DSKY V35
lamp test in an isolated process. It requires the real LGC response and changed
cockpit-atlas pixels; it does not assume a particular bitmap RGB color or save
over the flight checkpoint. A frozen-image check waits for initial atlas/terrain
caches to settle and still requires exact needle restoration.

## Repeat and capture

Follow [clean-macos-demos.md](clean-macos-demos.md) with the matching explicit
source/data profile and planetary texture root. From the `orbiter` directory:

```sh
cmake --build build/native-poc -j4
ctest --test-dir build/native-poc \
  -R '^(native_mcc|native_mcc_resume|native_original_descent|native_descent_visual)$' \
  --output-on-failure
```

CTest stages unchanged original scenarios through fixtures. Use
`build/native-candidate` for the isolated October source/data profile. For
captures, set absolute writable paths before running these same gates:

```sh
export ORBITER_MCC_CAPTURE=/absolute/path/mcc.ppm
export ORBITER_MCC_UI_CAPTURE=/absolute/path/mcc-controls.ppm
export ORBITER_DESCENT_CAPTURE=/absolute/path/landed-p65.ppm
```

The P65 capture is taken before the isolated lamp test. MCC captures use the
original default camera; right-drag looks toward the consoles. Full suite logs,
per-test output, original scenario hashes, descent telemetry, saved checkpoints
and source/data verification are retained in the evidence directory listed in
the validation record. The complete suite is the final integration gate.

The completed checks cover the native original insertion/liftoff demos and this
bounded descent diagnostic. Live CUA and physical-controller QA are not claimed
by these automated windows/captures. This selected executor exposes no computer
interaction tools. Complete legacy 2D presentation and RTCC MFD support remain
separate features; detailed visual/numerical Windows comparisons are deferred.
