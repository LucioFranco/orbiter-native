# External step-indexed keyboard diagnostic

The subsequent [stock Windows comparison](native-v9-step-input-comparison.md)
is complete. This document preserves the preceding native validation phase.

The public Module driver and native adapter use the original scenario and
production libraries. They inject only numpad3 through the existing immediate
keyboard/ACA path: steps 302–319, released before 320, followed by 240 neutral
steps through 559. Logging records callback times/counts and key requests only;
it performs no physical-vector or NASSP private getters and no intermediate
scenario saves. No production equations, source pins or inputs are substituted.

Two fresh native runs with the driver absent and two with the neutral driver
produce the identical entire 559-step scenario SHA-256
`f7a5596cb7fe91a8aeaa5cac276f76372dc64b071b7896673657e56ed089812c`.
This is an exact serialized-state neutrality result for the 9.316666666666667 s
protocol, without a field waiver or numerical tolerance.

The longer no-driver repeatability check fails at 3,601 steps. Eagle's saved
global velocity, fractional IMU remainders/matrix, CDU, ASA, radar and two
chemical energy records differ between two runs of the same minimal executable
with no diagnostic Module loaded. Columbia's block is identical. This establishes
variation without an observer; its precise arithmetic cause remains unresolved.
The failed comparison is retained and the 60-second neutral-driver extension
was stopped. It cannot support a general exactness claim or attribution of the
earlier rich-observer differences solely to an observer.

Within the validated 559-step bound, the driver applies exactly 18 yaw input
steps, with zero faults. Its two pulse runs produce identical complete vessel
blocks. The aligned 319-step neutral and pulse snapshots verify actual response:
Eagle VROT Y changes from -0.0012 to -0.6028 degrees/s, its channel 031 saved
virtual input (`VICHAN025`) changes from 28671 to 12279 and RCS propellant is
consumed. At step 559 the channel returns to 28671 and VROT Y is +0.0080 degrees/s
versus -0.0012 in the neutral run. These are observations rather than future
acceptance limits.

The richer existing ACA probe remains an independent reference. Both vessels'
rounded trajectory tokens and complete AGC/DSKY blocks match that reference at
steps 319 and 559; complete vessel-block equality fails in inertial, IMU,
chemical, radar and EMS values. Those full-state differences remain in the raw
diffs without waiver. The callback-only driver's neutrality and repeatability
claims concern its own tested executable and protocol, not equivalence to every
older observer or full-precision cross-platform trajectories.

The Windows DLL cross-links with local Clang/LLVM, the unchanged official
ModuleAPI.h and AMD64 Orbiter.lib, and cached Kernel32 imports. No new install,
license acceptance or security-policy change is required. The 9,216-byte DLL
imports only Orbiter.exe/kernel32.dll; source and exact build hashes are recorded.
Independent parent review found no source/build findings. Windows execution is
still unvalidated and no guest run is requested by this native phase.

The next dependency is parent-coordinated isolated stock neutral validation:
two no-plugin and two Module-neutral fresh 559-step runs using ordinary exit
saves and complete serialized vessel comparisons. Exact instructions and the
source/DLL package are under `evidence/step-input` in the task workspace. The
instructions set environment variables inside the Scheduled Task's child `.cmd`
action; parent-shell environment does not reliably reach a directly scheduled
Orbiter process. Restore plugin/config/current-state/quicksave changes and retain
any failed neutral comparison before pulse claims.

Module callback counts prove requests, not vessel delivery. A dialog or another
consuming plugin can block standard keyboard processing. Subsequent guest pulse
validation must confirm the actual saved response as well as exact timing.
Known stock motor-loader, clock precision and display differences remain
explicit; broad AGC snapshot redesign stays paused.

The sources in [input-diagnostic](../upstream/input-diagnostic/StepInputParity.cpp)
are an external diagnostic, not CMake targets or production patches. Build
provenance and raw evidence remain outside the checkout. Removal is deleting
that source directory, its diagnostic docs/reports and the staged plugin/runner
only after preserving evidence or adopting equivalent upstream coverage. No
GPU, VM, push or hosted CI is used by the native worker.

Exact outcomes, package hashes and retained failed comparisons are in
[validation-v9-step-input-macos-2026-10-11.json](../upstream/validation-v9-step-input-macos-2026-10-11.json).
