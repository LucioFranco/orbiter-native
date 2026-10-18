# External repeatability diagnostics

These sources are excluded from production CMake targets and acceptance tests.
`CadenceObserver.cpp` is the existing external off/neutral/pulse adapter plus
read-only `Body::updcount` observations. This diagnosis uses only `off` mode;
there is no loaded input plugin or physical-vector observer.

`SeedTrace.c` is an external macOS dyld interposer. `RNG_TRACE` selects its trace
file. With `DIAGNOSTIC_C_RNG_SEED` absent, it preserves requested seeds. Setting
that variable explicitly creates a seed counterfactual; never use that setting
to claim an acceptance result or silently configure production.

The ledger in `../validation-v9-repeatability-macos-2026-10-12.json` records
exact external compile/link commands, source/binary hashes and run environments.
Use those commands with the pinned native build; the interposer compiles as a
separate dylib with the recorded Clang/macOS SDK. Select it only in the launched
diagnostic process's `DYLD_INSERT_LIBRARIES`. Normal launches use no interposer.

No dependency installs, core/NASSP patches or new acceptance fixtures are needed.
Remove this directory and external builds after the investigation; preserve
raw saves and the artifact ledger for the unresolved strict failure.
