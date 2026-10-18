# Local NASSP cockpit proof of concept

For a fresh build and explicit external data roots, first follow
[clean-macos-demos.md](clean-macos-demos.md) and set `NASSP_DATA_ROOT` and
`ORBITER_TEXTURE_ROOT`. For original multi-vessel insertion/liftoff scenarios, see
[Original Apollo 11 mission demos](original-mission-demo.md).

Open the built macOS app from the engine checkout:

```sh
open "build/native-poc/NASSP PoC.app"
```

If an older PoC is still running, quit it first or use `open -n` to launch this
build separately.

It opens the standalone Apollo 11 CSM in its 3D cockpit and runs until you close
the window or quit the app. The CSM has SCS control and powered translation,
normal rotational, and direct rotational hand controllers. The outside view now
includes Orbiter's catalog stars, visible through the windows as you look around.

The FDAI CPU rasterizer is implemented: the original CSM and LM 2D instrument
drawing paths render their ball textures, needles and power-loss indications
through native surfaces. This does **not** mean complete 2D panel navigation is
available; the interactive host currently presents the 3D cockpits. Verify the
instrument path with `ctest --test-dir build/native-poc -R '^(fdai_rasterizer|native_fdai)$' --output-on-failure`.

The native build also includes **Eagle's 3D cockpit**, staged separately from the
CSM at Apollo 11 PDI minus 15 minutes. To try it:

```sh
python3 tools/stage_nassp_smoke.py build/native-poc --addon "$NASSP_DATA_ROOT" --lm
build/native-poc/orbiter --root build/native-poc/runtime-lm --scenario NasspLM \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls --save MyLMSession
```

The LM has twelve viewpoints and a working mouse-operated DSKY backed by its
original LGC. VERB 35 followed by ENTER runs its display test. This standalone
fixture omits Columbia; a complete descent, ascent, and rendezvous has not been
validated. Lighting remains approximate and the Moon has no terrain detail yet.

To fly both spacecraft in one session, stage the combined PDI minus 15 minutes
scenario and use **F4** to switch between Columbia and Eagle:

```sh
python3 tools/stage_nassp_smoke.py build/native-poc --addon "$NASSP_DATA_ROOT" --pair
build/native-poc/orbiter --root build/native-poc/runtime-pair --scenario NasspPair \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls --save MyApolloSession
```

Nearby spacecraft now render through the cockpit windows, including the other
Apollo spacecraft and separated stages. The PDI fixture retains its original
orbital positions; it is not a close-range rendezvous setup.

The window title identifies the active spacecraft. Each vessel remembers its
cockpit seat, and switching releases held flight controls. Both spacecraft keep
simulating; F5 pauses the whole session. Resume a save with `--scenario MyApolloSession`
and the same `runtime-pair` root. This fixture starts with the vessels undocked;
it does not establish that rendezvous, docking or mission staging works.

For a manual LM descent-engine burn in the staged PDI fixture, use the commander
view (Ctrl + arrow keys to change seats). On panel 1, click the lower half of
**ENG ARM** to select **DES**, and the lower half of **THR CONT** to select **MAN**.
The fixture already has **MAN THROT** on **CDR**. Press and release `]` to start,
hold M to increase throttle, hold period to decrease it, and press `[` to stop
with laptop mode enabled. Physical numpad equivalents are `+`, `0`, decimal,
and `−`. ENGINE STOP latches: press `[` again to release it before restarting.
The throttle stays at its selected position when released; stopping the
engine requires the stop control. This is a manual propulsion demonstration,
not a complete lunar landing procedure.

The staged LM also supports manual separation. From the commander view, Ctrl +
Left selects the left-side panel view. Set **MASTER ARM** on. To prepare for an
ascent-engine burn, operate **HE PRESS / ASC**. Then right-click the **STAGE**
switch cover to open it and move the switch on. Eagle becomes the
ascent stage and leaves a separate descent-stage vessel behind. The descent
stage is skipped by F4. Saves retain both stages.

After separation and ascent helium pressurization, return to the commander view
and move **ENG ARM** through OFF to **ASC**. Release the latched ENGINE STOP with
`[` if it was used to end a previous burn, then press and release `]` to start.
The ascent engine runs at fixed thrust; the throttle keys do not regulate it.
Press `[` to stop. In-orbit staging, ascent thrust and reload during a burn are
verified. A separate short lunar-surface launch is described below; guided ascent
to orbit remains unverified.

To start on the Moon one minute before Apollo 11 liftoff:

```sh
python3 tools/stage_nassp_smoke.py build/native-poc --addon "$NASSP_DATA_ROOT" --liftoff
build/native-poc/orbiter --root build/native-poc/runtime-liftoff --scenario NasspLiftoff \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls --save MyLunarAscent
```

In this fixture, select the left panel with Ctrl + Left, turn **MASTER ARM** on,
and operate **HE PRESS / ASC**. Return to the commander view, select **ENG ARM /
ASC**, then press and release `]`. The original LM control system performs staging
as the ascent engine starts. The verified manual launch climbs about 299 metres
in twenty seconds and leaves the descent stage on the Moon. This is a short
liftoff demonstration, not a validated guided ascent to orbit or rendezvous.
The renderer supports tiled Earth/Moon surface textures and elevation. To use
the standard assets linked by NASSP's bundled Quick Start Guide, extract the
`Textures` directory from `https://cdn.openorbiter.space/orbiter-releases/Orbiter-2024.zip`
into a separate local asset directory, then stage with:

```sh
python3 tools/stage_nassp_smoke.py build/native-poc --addon "$NASSP_DATA_ROOT" --csm \
  --textures "$ORBITER_TEXTURE_ROOT"
```

The same `--textures` option works with `--lm`, `--pair`, and `--liftoff`.
Staging links assets into the runtime without copying them into NASSP's source.
Power-of-two BC1/RGBA terrain textures get missing mip levels generated at load
time for distance filtering; original compressed images and supplied mip levels
are preserved.
Compatible BC1 terrain tiles also include neighbouring imagery around their
edges for continuous filtering. This removes the tested launch-pad foreground
seam; missing-neighbour and mixed-resolution boundaries still need validation.
The native renderer now draws the downloaded Milky Way background behind catalog
stars, planets and spacecraft, using Orbiter's background orientation and brightness
setting. It uses all 364 level-eight tiles in the downloaded legacy `.tex`
archive, uploaded as a compressed atlas in one draw. Boundary filtering follows
neighbouring tiles across latitude bands and hemispheres. Smaller archives fall back
to their highest supported complete level. The downloaded Hipparcos star-image
layer now renders additively alongside catalog stars, controlled independently by
`EnableBackgroundStarmap` and `StarImagePath`. Catalog dots are controlled by
`EnableBackgroundStars`; disabling them leaves the image layers available.

Engine flames now use NASSP's original exhaust textures and live thrust levels,
with depth testing against spacecraft and terrain. The LM descent-engine burn
and shutdown are verified in an exterior view. `EnableParticleStreams = FALSE`
hides particle trails and lunar dust; engine flames remain a separate effect.

Spacecraft materials use their original specular colours and shininess for Sun
and local-light highlights. `EnableSpecularReflection = FALSE` disables these
highlights. Environment reflections are still unfinished.
Original material opacity now blends windows, COAS glass and other partially
transparent spacecraft surfaces; animated glass updates its depth-sort position.

Press **F1** (or Fn + F1) to open the flight-controls panel over either 3D
cockpit. It shows the active spacecraft and has pause/resume and spacecraft-switch
buttons, plus an **Exterior view / 3D cockpit** toggle. F1 closes it; F4, F5 and F6 remain available while it is focused. Clicking
outside returns keyboard focus to the cockpit. Opening the panel releases held
flight controls; press them again after returning to the cockpit.

To stage the original Saturn V and mobile launcher at LC-39:

```sh
python3 tools/stage_nassp_smoke.py build/native-poc --addon "$NASSP_DATA_ROOT" --pad \
  --textures "$ORBITER_TEXTURE_ROOT"
build/native-poc/orbiter --root build/native-poc/runtime-pad --scenario NasspPad \
  --graphics WgpuClient --window
```

This scene includes the rocket, mobile launcher and original six floodlights,
viewed from a level ground camera. It begins four hours before launch, before
sunrise. The crawler, VAB, service structure and other launch-site modules are
not yet included. Earth has an initial Rayleigh sky and depth-limited haze over
opaque terrain and spacecraft. `EnableHorizonHaze` controls the sky/limb effect;
`EnableDistanceFog` independently controls haze over opaque surfaces. These are
visual controls and do not disable atmospheric flight physics. A focused sunlight shadow map covers nearby
geometry with two nested coverage regions; its centre moves in whole texels to
reduce shimmer as the spacecraft moves. Downloaded cloud tiles render as a
transparent layer at the configured altitude and rotation. Cloud shadows,
large-scale terrain shadows and local-light shadows remain unfinished.

For the original daytime countdown state, use `--pad-countdown` in place of
`--pad`, with `--output build/native-poc/runtime-pad-day`, then launch with
`--root build/native-poc/runtime-pad-day`. This uses NASSP's saved T-20-minute
scenario rather than shifting the time of the earlier scene.

Press **F2** (or Fn + F2) for two flight instrument displays over the cockpit.
Choose **Orbit** or **Surface** and use the buttons below each display. These use
Orbiter's original instrument calculations and drawing code. Columbia and Eagle
keep separate modes while switching focus; scenario saves preserve the focused
spacecraft's left and right displays and their settings. F2 hides the window.
These are supplemental displays; the NASSP 3D cockpit remains the primary view.
The macOS app also loads **Project Apollo** and **Project Apollo Checklist**.
Choose them from either display's mode list. The checklist shows the spacecraft's
original flight plan; **NAV** browses checklists, and **FLSH** toggles its switch
highlighting setting. Its automation controls operate the original NASSP systems.
The CLI enables these modes with `--module ProjectApolloMFD` (also use it when
reloading their saved scenarios). Network uplinks are unavailable. HUD output,
other built-in MFD modes and the separate RTCC MFD remain unfinished.

The app starts in **laptop numpad mode**. F6 toggles between laptop and standard
keys; use Fn + F6 if macOS assigns F6 to a system control. The window title shows
the current mode. Physical numpad keys work in either mode. CLI launches can
select laptop mode with `--laptop-controls`.

| Action | Physical numpad / mouse | Laptop mode |
| --- | --- | --- |
| Open / close flight-controls panel | F1 (Fn + F1 if needed) | Same |
| Open / close flight instruments | F2 (Fn + F2 if needed) | Same |
| Switch spacecraft | F4 (Fn + F4 if needed) | Same |
| Pause / resume simulation | F5 (Fn + F5 if needed) | Same |
| Look around | Right mouse drag | Same |
| Zoom cockpit view | Mouse wheel / trackpad scroll | Same |
| Toggle cockpit flashlight | F | Same |
| Operate cockpit controls | Left mouse button | Same |
| Change cockpit seat | Ctrl + arrow keys | Same |
| LM engine start / stop | Numpad + / − | ] / [ |
| LM throttle increase / decrease | Hold Numpad 0 / decimal | Hold M / period |
| Toggle rotation / translation | Numpad Divide | `/` |
| Pitch up / down (rotation mode) | Numpad 2 / 8 | K / 8 |
| Yaw left / right (rotation mode) | Numpad 1 / 3 | J / L |
| Roll left / right (rotation mode) | Numpad 4 / 6 | U / O |
| Direct hardover (rotation mode) | Hold Option/Alt with a rotation key | Same |
| Translate up / down | Numpad 2 / 8 | K / 8 |
| Translate left / right | Numpad 1 / 3 | J / L |
| Translate forward / back | Numpad 6 / 9 | O / 9 |

Laptop mode maps the physical `7 8 9 / U I O / J K L` grid onto the numpad,
plus M → 0, period → decimal, [ → subtract and ] → add. F6 restores these keys' ordinary functions.
Changing modes or losing focus releases held controls; press them again to resume.

F5 freezes simulation time while keeping the cockpit view, zoom, flashlight, and
mouse controls available. The window title shows **PAUSED**. Pause/resume releases
held flight controls; press them again after resuming. Paused time is not replayed
on resume, and paused ticks do not count toward an explicit `--steps` limit.

SCS may briefly fire opposing jets to damp rotation after you release a command.
Ctrl's reduced translation request is below Apollo's THC breakout threshold;
it does not produce a 10% thrust setting.

To save on normal exit, launch the shared CLI with a save name:

```sh
build/native-poc/orbiter --root build/native-poc/runtime-csm --scenario NasspCSM \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls --save MyCSMSession
```

The save is `build/native-poc/runtime-csm/Scenarios/MyCSMSession.scn`. Resume with
`--scenario MyCSMSession`. Explicit `--steps N` still limits a run; headless runs
without it default to 600 steps. Interactive sessions use fixed simulation ticks
at wall-clock speed, independently of rendering. A machine sleep or debugger
pause is bounded rather than replayed in full. Finite `--steps` runs are
reproducible by default; add `--realtime` to pace a finite windowed run.

To configure the optimized build, enter the Nix development shell (or use the
existing direnv environment), then select the native preset:

```sh
nix develop path:./nix
cmake --preset native-poc -DNASSP_DATA_DIR="$NASSP_DATA_ROOT" \
  -DNASSP_DATA_PROFILE=baseline -DORBITER_TEXTURE_DIR="$ORBITER_TEXTURE_ROOT"
cmake --build --preset native-poc -j4
```

This uses C++ `RelWithDebInfo` and Rust release optimization. The existing
`build/portable` and `build/portable-sanitize` trees remain available for diagnosis.

To explicitly stage or refresh the initial CSM assets:

```sh
cmake --build build/native-poc --target stage_nassp_poc -j4
```

Routine builds do not restage assets. The staging target refreshes the baseline
scenario and copied configuration, so preserve any manual configuration edits
before explicitly rerunning it. Named saved scenarios are not removed.

This app is a local build entry point, dependent on this checkout and its build
runtime. It is not a standalone distributable. The default CSM fixture omits the LM; terrain,
full mission staging, legacy mission-audio sequencing, and other remaining port work are tracked in
[macos-port-status.md](macos-port-status.md).

To measure frame time, add `--profile --steps 90` to a windowed CLI launch. The
report skips ten warmup frames and includes CPU preparation and presentation in
`render_ms`. Use the same scenario, viewport, and build configuration when comparing runs.
The unoptimized diagnostic build is substantially slower than this preset.
On the development machine, the optimized CSM scene measured about 24 ms per
rendered frame (roughly 41 frames/s), versus 289 ms unoptimized. These are local
measurements with another cockpit app open, not performance guarantees.

## Joysticks and gamepads

Open **F1 → Controllers** to see attached SDL devices, copy a device GUID, and
identify axis/button/hat indices from their live readings. Native mappings are
loaded at spacecraft creation from these files under the active runtime root:

- `Config/ProjectApollo/Vesim/CSM.native.cfg`
- `Config/ProjectApollo/Vesim/LM.native.cfg`

For the default app the root is `build/native-poc/runtime-csm`. Create the CSM
file there, replace the GUID placeholders and choose the indices for your device:

```text
"RHC Roll" "paste GUID here" "" Axis 0 0 0.05
"RHC Pitch" "paste GUID here" "" Axis 1 0 0.05
"RHC Yaw" "paste GUID here" "" Axis 2 0 0.05
# Optional button:
# "Rotation/Translation toggle" "paste GUID here" "" Button 0
```

The axis arguments are index, inversion (0 or 1), and fractional center deadzone.
An exact device name can replace the GUID; the following empty quoted field can
contain a serial number to distinguish identical controllers. Multiple matching
devices leave the binding inactive. Device `*` is allowed when exactly one device
is connected. Button indices start at zero. A hat binding ends with `Hat index
mask`, with up=1, right=2, down=4, left=8 (add adjacent bits for a diagonal).

For Eagle use `ACA Roll`, `ACA Pitch`, and `ACA Yaw`. Separate translation axes
are `THC X/Y/Z` on the CSM or `TTCA X/Y/Z` on the LM. The rotation/translation mode
switch reuses the RHC/ACA axes for translation, following NASSP's existing logic.
An optional LM axis named `TTCA Throttle` controls manual descent throttle; use
zero deadzone for this axis. Its maximum raw reading is minimum thrust, so invert
it if needed. Existing engine-arm, throttle-mode and start/stop controls still
apply.

Restart the scenario after editing a config. Recenter axes and release buttons
after first connection, focus changes, pause, or UI capture before commanding
again. This includes moving a non-centering throttle through its midpoint.
Unassigned, unavailable or disconnected axes retain keyboard control. Disconnecting
a throttle preserves its last manual setting; it does not shut down the engine.
No config means ordinary keyboard operation. Malformed configs report their
filename and line number during startup.

Virtual SDL controllers have verified CSM translation/rotation, fuel consumption,
LM guidance-computer input and actual DPS throttle response. Physical USB/Bluetooth
controller behavior has not yet been checked on this machine.


## Native cockpit sound

Windowed runs, including the PoC app, now enable miniaudio output by default.
NASSP's existing XRSound calls load assets from the runtime's `Sound` directory.
The staging script links the original NASSP sound assets there. Switches, buttons,
fans, alarms and other existing XRSound-based calls use the native backend.

Pass `--no-sound` to disable device output. Headless runs are silent by default;
`--sound` explicitly enables output. If the output device cannot initialize, the
simulator continues with sound unavailable and reports the error on stderr.
Sound pauses with the simulation. Internal-only sounds follow the focused vessel
and cockpit view; external sounds use pressure/distance attenuation. Global and
radio playback follow their respective XRSound visibility rules.

The original CSM switch and LM button callbacks have been verified through mixed
audio output, and a short NASSP click completed through the Mac's audio-device
callback. This does not verify every mission callout: the legacy `SoundEvent`
mission-audio path remains stubbed, and automatic XRSound engine/callout defaults
are not generated by this backend.
