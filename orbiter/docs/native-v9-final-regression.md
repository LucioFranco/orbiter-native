# Final registered macOS v9 regression suite

A single unfiltered serial CTest invocation completed on source commit
`71b1393439d113a3dc9e6b74808f1a667735b7b8` with **99 passed, 1 failed, 0 skipped, 100 total**.
CTest returns **8**, retaining the failure. Total CTest wall time is 1173.46 s.
The configured v9 build was refreshed offline before the suite. All 91
inventoried executable/module/library hashes remain unchanged across execution;
no runtime source, RNG policy, equation or assignment timing changes in this
phase. The source checkout is clean before and after the suite.

The one failure is the existing `native_pdi_pilot` faithful-resume assertion:
`AGC cycle counter was not restored; faithful processor resume requires CDU FIFO persistence`.
Its saved counter is 124205511 and the fresh reload is 0, identical to the
previous recorded blocker. Full failure output, original/save/reload scenarios,
neutral-continuation telemetry and the failed verifier report are retained.
No expected failure annotation, disabled test, acceptance tolerance or assertion
change is applied. Every other registered check passes; no new regression is
observed.

The complete run includes the added `native_scenario_compatibility` regression,
original insertion/liftoff/descent, input/control/save checks, graphical checks
and both synthetic SDL PDI-window launches. `native_pdi_window` passes in
477.36 s. These automated windows do not establish human input or physical
controller calibration. The external original 60-second exact-repeatability
failure diagnosed at `71b1393` remains open and is outside the registered suite;
this 99/100 result does not waive it or alter production RNG behavior.

A separate sandbox adapter preflight failed with `No wgpu adapter available`.
The authorized complete suite then ran with actual desktop/Metal access, keeping
all GPU checks enabled. That environment preflight is not counted as a registered
regression. No suite result is reconstructed by combining partial invocations.

The full command is recorded in the machine report and was:

```sh
/nix/store/ak2ia3545lajx7np57i7fizgmiyycalf-cmake-4.4.2/bin/ctest \
  --test-dir <POC_CHECKOUT>/orbiter/build/native-v9 \
  --parallel 1 --output-on-failure \
  --output-junit <NATIVE_EVIDENCE_ROOT>/final-regression-20261012/suite-junit.xml
```

The native GPU/SDL session is released: a desktop process check at
`2026-10-12T02:04:38.297909+00:00` finds no active native simulator/regression
process. The coordinator owns any other desktop/VM scheduling. Two fresh
original-scenario runtimes and exact bounded launch commands are ready in
[native-v9-live-qa.md](native-v9-live-qa.md). Neither live QA case was launched by
this worker. Both use the tested original-demo physics defaults and retain
original scenario bytes. The worker should record actual visible cockpit,
exterior, controls and window-close evidence before calling these checks passed.

The clean original `9e8d452` checkout, raw stock Windows bundle and preceding
verified backup remain unchanged. No push, hosted CI or publication occurs.

[Machine-readable full result and artifact hashes](../upstream/validation-v9-final-regression-macos-2026-10-12.json)
includes all 100 results, binary hashes, exact configuration/launch records,
classification of the strict failure and the GPU release check. Raw JUnit,
CTest log, fresh scenario/telemetry/capture outputs and prior Testing data are
preserved under the task evidence root.
