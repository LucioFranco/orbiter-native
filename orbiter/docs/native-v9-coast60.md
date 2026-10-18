# Sixty-second original-scenario coast

The subsequent [stock Windows comparison](native-v9-coast60-comparison.md)
is complete. This document preserves the preceding native-only capture phase.

This extends the matched original PDI T-15m protocol from 300 neutral steps after
one startup step to 3,600 neutral steps after startup, or 3,601 total at 1/60 s.
Source pins, original scenario bytes and force settings remain unchanged.
Correct native loader behavior is retained; no motor, clock or display changes
are applied to force stock parity.

The headless native capture completed in 5.14 s with simulation time
60.01666666666454 s. Final MJD is `40422.8324568111`; both Columbia and Eagle AGC
counters are `5121422`. The observer records 61 focused points and six raw saves
at load, startup, the existing five-second boundary, 10 seconds, 30 seconds and
the 60-second coast boundary. All sampled vectors/forces are finite; sampled
force flags match the original configuration and nonspherical gravity stays
active. The saved 301-step trajectory tokens and complete AGC/DSKY blocks still
match the earlier stock reference after newline normalization. No full-precision
Windows inertial-vector claim follows from equal rounded scenario tokens.

The exact stock Windows capture instructions are at
`evidence/coast-60s/windows-capture-request.md` in the task workspace. Use a fresh
original scenario with `--maxsimtime=60.00833333333333`; compare the actual
raw MJD/counters and launch configuration. The stock 60-second capture is pending,
so this native result alone does not establish extended cross-platform parity.

Known [gimbal startup differences](native-v9-motor-load-comparison.md), double
mission-clock loading and headless display cadence remain visible in the raw
results. The pending comparison will check trajectory, AGC and DSKY blocks and
quantify Columbia chemical and IMU/inertial residuals without an automatic
small-error waiver. Shared AGC resume failure and full graphical/physical-control
acceptance remain unresolved. No broad snapshot redesign, GPU, VM, push or CI is
performed by this native phase.

The external observer source and exact build command are retained with evidence;
its only changes are a bounded 3,601-step limit, sampled output and boundary
saves. It uses public SDK observations and unchanged runtime libraries, and
adds no registered test or production patch. Remove that external diagnostic
directory only after preserving its evidence. Exact native results and hashes
are in [validation-v9-coast60-native-macos-2026-10-11.json](../upstream/validation-v9-coast60-native-macos-2026-10-11.json).
