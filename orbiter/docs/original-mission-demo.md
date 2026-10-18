# Original Apollo 11 mission demos

Run from the `orbiter` directory of your checkout. Follow
[clean-macos-demos.md](clean-macos-demos.md) to build and set `NASSP_DATA_ROOT`
and `ORBITER_TEXTURE_ROOT` to matching external data. These demos retain the
supplied upstream scenario bytes. Staging writes runtime directories and records
provenance. Native compatibility is verified; Windows trajectory parity remains
unverified.

## Original insertion

```sh
python3 tools/nassp_demo.py stage --nassp-data-root "$NASSP_DATA_ROOT" \
  --orbiter-textures "$ORBITER_TEXTURE_ROOT" --mission insertion
build/native-poc/orbiter --root build/native-poc/runtime-original-insertion \
  --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion' \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls \
  --save MyOriginalInsertion
```

All five vessels load: Columbia, MCC, Floodlight and the discarded S-II/interstage.
During the tested 900-second coast, original callbacks remove the two discarded
stages. To replay that automated coast/save/reload check:

```sh
python3 Tests/NativeOriginalInsertion.py build/native-poc/orbiter \
  build/native-poc/runtime-original-insertion \
  "$NASSP_DATA_ROOT/Scenarios/Project Apollo - NASSP/Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion.scn" \
  --coast-seconds 900
```

## Original lunar liftoff

```sh
python3 tools/nassp_demo.py stage --nassp-data-root "$NASSP_DATA_ROOT" \
  --orbiter-textures "$ORBITER_TEXTURE_ROOT" --mission liftoff
build/native-poc/orbiter --root build/native-poc/runtime-original-liftoff \
  --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 11 - Lunar Liftoff T-1m' \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls \
  --save MyOriginalLiftoff
```

Ctrl + Left opens the left panel. Set **MASTER ARM** on and operate **HE PRESS /
ASC**. Ctrl + Right returns to the commander view; select **ENG ARM / ASC**.
Press and release `]` to start. Original LM controls separate the descent stage
and Eagle climbs. The automated test verifies a 20-second climb, all eleven
vessels, LGC save/reload and fresh-process ascent. Guided orbit/rendezvous is
unverified. `[` operates latched ENGINE STOP; press again to release before restart.

F1 opens flight controls, F2 instruments, F4 switches vessels, F5 pauses, and F6
changes the key layout. Use Fn with function keys if macOS reserves them. Close
the window normally to write the named save, then relaunch with the same root and
`--scenario MyOriginalInsertion` or `--scenario MyOriginalLiftoff` respectively.

## Original PDI through P64: uninterrupted workflow

The first bounded PDI demo runs the supplied **Apollo 11 - 10 - PDI T-15m**
scenario continuously through original radar incorporation and P64, then checks
a short original ACA yaw command and neutral release. It stops above 1,200 ft;
this is an automated control-path demonstration, not a landing demonstration.
Use a matching build/data profile from the clean-build guide. On Lucios-Air the
validated build directories are `build/native-pdi-protocol` and
`build/candidate-pdi-protocol`; a clean preset build can use `build/native-poc`.

To reproduce the **headless** automated same-process demonstration, substitute
your build:

```sh
ctest --test-dir build/native-pdi-protocol \
  -R '^native_pdi_uninterrupted_aca$' --output-on-failure -j1
```

The fixture stages original assets. The probe starts the original T-15m
scenario, follows the bounded pilot procedure, selects original PGNS **ATT
HOLD** at the P64 handoff, observes one second of neutral control, sends a
0.3-second proportional yaw command, then observes four seconds with the stick
centered. All control passes through SDL/native VESIM and the original ACA,
LGC/DAP, ATCA and RCS. It changes no pose, rates, memory, throttle or jet levels.
The report is `runtime-pdi-uninterrupted-aca/pdi-uninterrupted-aca-verification.json`;
the detailed event stream is `PDIProtocol.jsonl` in that runtime.

To **watch the automated sequence and retain the same live host**, run from
the engine root:

```sh
python3 tools/nassp_pdi_window.py --build build/native-pdi-protocol
```

Use `--build build/candidate-pdi-protocol` for the other pinned profile. The
helper reads the matching source/data/planetary paths from that build's CMake
cache, checks the source adaptations and original data, then stages a separate
`runtime-pdi-window`. It refuses existing LM/CSM native controller maps.
No data is downloaded. The build must already contain the diagnostic probe,
WgpuClient, ProjectApolloMFD and the original mission modules/resources.

The actual WgpuClient/SDL cockpit displays the accelerated prefix every five
simulated seconds and each 1/60 s ACA step. Keyboard/mouse flight inputs and
OS focus changes are blocked during automation; closing the window cancels it.
At completed neutral release it pauses **the same host**, removes the owned
virtual stick and temporary map, clears in-memory bindings through the existing
release API, and enables the usual cockpit input loop. It never loads the
archival handoff save.

- **F5:** resume/pause; continuation stops at 12 simulated seconds, below
  1,200 ft calculated guidance altitude, or ground contact. A reached guard
  keeps the simulation paused until exit.
- **L/J:** yaw right/left with laptop controls enabled; **F6** changes to/from
  standard numpad controls. **F1** opens flight controls; **F2** opens instruments.
- **Close window:** exit at any phase. **Esc:** exit after manual handoff.

The annotation and title show automation, paused handoff, manual input, or guard
status. Manual stepping uses fixed 1/60 s ticks with rendering; GPU frame cost
can slow wall-clock playback. This bounded transport does not provide a landing
policy or validate human control, physical calibration or landing. This launcher hands off to keyboard/mouse;
it does not configure a physical-controller map for the retained host.

`window-captures/original-pdi.ppm` and `p64-paused-handoff.ppm` are actual
renderer readbacks. `PDIWindow.jsonl` records lifecycle observations. After
normal exit, `pdi-window-verification.json` checks the original uninterrupted
PDI/ACA telemetry and unchanged mission inputs. For programmatic lifecycle QA:

```sh
python3 tools/nassp_pdi_window.py --build build/native-pdi-protocol --self-test --repeat 2
```

The registered `native_pdi_window` test runs this command. Its injected SDL
events check flight-input isolation during automation, frozen pause state,
same-processor unpause, keyboard-to-original-ACA ownership and neutral release,
focus-loss release, the continuation guard, normal window close, Esc, and a
second fresh launch after cleanup. These are programmatic checks, not CUA or
human mouse/keyboard evidence.

For a **fresh, manually flown graphical T-15m session**, stage only and launch
the original scenario. This command does not display the automated PDI sequence
and does not attach to its live P64 state:

```sh
ctest --test-dir build/native-pdi-protocol \
  -R '^nassp_stage_pdi_uninterrupted_aca$' --output-on-failure -j1
build/native-pdi-protocol/orbiter \
  --root build/native-pdi-protocol/runtime-pdi-uninterrupted-aca \
  --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 10 - PDI T-15m' \
  --graphics WgpuClient --module ProjectApolloMFD --window --vc --laptop-controls
```

Follow the original matching LM workbook through PDI; keep this process open
through P64. See [the scoped procedure and source references](macos-pdi-protocol.md#uninterrupted-p64-aca-demo-2026-10-10).
The automated diagnostic uses the crew dictionary's manual ATT HOLD option
during P64. Its early short pulse at the below-2,000 ft handoff is not the flown
timeline's nominal TIG +10-minute manual-control decision. It never activates
ROD, selects a landing site or claims P66/manual landing.

**Save/resume warning:** `NativePDIPilotHandoff.scn` is archival evidence.
Fresh reload loses the processor/pending timing needed for faithful continuation.
Do not restart that save to reproduce this ACA result. The existing strict
`native_pdi_pilot` resume gate stays enabled and failing; this separate green
demo gate does not certify resume. Restart the original T-15m scenario for a
fresh demonstration. This limit also means the earlier memory/channel reload
checks are narrower than complete processor-state persistence.

Graphical mouse hit testing and a physical controller remain manual checks.
In the manually flown graphical session, the smallest useful check is one
mapped ACA pulse and center return after reaching P64 without restarting,
confirming the ATT HOLD readback, rate response and neutral release in the
visible cockpit. The automated virtual-stick evidence does not establish
device calibration, human piloting or normal window closure.

### Live interaction still to check

No CUA/desktop-control tool is available in this execution environment. The
exact remaining check is to run the command without `--self-test`, wait for the
paused live P64 handoff, inspect the visible controls/instruments, press F5,
apply one short mapped yaw input and release it, pause again, then close through
the native window. Graphical mouse hit testing and physical-controller
setup/calibration remain separate unvalidated work. Framebuffer captures establish rendered
content; they do not establish physical desktop visibility, human input or
landing success.

At `4c31c55` this workflow was missing. The opt-in diagnostic transport now
provides it without engine/NASSP changes or saved-state reconstruction. Its
headless gate remains separate from the strict failing resume gate.

## Repeat the gates or try the isolated candidate

```sh
ctest --test-dir build/native-poc -R '^(nassp_stage_original_(insertion|liftoff)|native_original_(insertion|liftoff))$' --output-on-failure
```

The October candidate uses a separate source/data root and
`build/native-candidate`. Configure it as described in
[clean-macos-demos.md](clean-macos-demos.md), then add
`--build build/native-candidate --profile candidate` to the helper commands and
select that data root. [upstream-native-adoption.md](upstream-native-adoption.md)
records the exact revision and verified patch stack.

The original mission gates also execute the documented graphical command with
bounded steps, save, reopen in a fresh graphical process, compare AGC/LGC memory
and clocks, then continue rendering. Normal user window-close interaction
remains an open live CUA check in this executor. Automated rendering/input checks and
static captures pass. The X-pointer needle reflection defect is resolved and
verified in both CDR/LMP views; see
[macos-instrument-validation.md](macos-instrument-validation.md). Complete legacy
2D panels and the separate RTCC MFD remain unfinished. Native MCC screen
bindings and a bounded original-scenario P65/soft-contact/shutdown/resume
check are covered by [macos-demo-validation.md](macos-demo-validation.md);
that diagnostic does not reproduce the complete pilot checklist.
