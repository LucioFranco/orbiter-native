# Architecture research from the preserved baseline

Research should start from the reviewed native v9 source checkpoint and its
honest evidence, rather than immediately replacing the implementation. The
questions below are proposed investigations; no core rewrite, snapshot redesign
or release pipeline is adopted here.

## Research inputs and constraints

Use the [architecture map](architecture.md), [decision history](history-and-decisions.md),
[validation limits](validation-and-open-problems.md), exact pinned upstream
sources and removable patch inventories. Preserve the working bounded demo,
strict assertions and original data. Benchmark changes in isolated branches with
matching source, compiler/FP policy, scenario/config/ROM/assets and step protocol.

The baseline has a useful original C++ oracle and bounded stock Windows reference.
Neither is a complete correctness specification. Serialized equality is weaker
than full-precision equality; stock bugs and platform/observer variations must
be distinguished from intended behavior. An observer should not become a hidden
production input. Identify required contracts before setting tolerances.

## Questions to investigate

| Direction | Concrete question | Evidence needed before implementation selection |
| --- | --- | --- |
| Incremental upstream portability | Which host/SDK/build hooks can upstream adopt with minimal mission delta? | Per-group removal prototype, source reconstruction, complete registered results and matching original mission checks |
| Deterministic simulation boundary | What contract separates fixed-step evolution from wall time, display cadence and randomized scheduling? | Full-precision state traces, RNG/observer isolation, repeatability cause, documented FP/toolchain contract |
| Guidance state ownership | Can processor timing/CDU FIFOs become explicit per-instance state without changing guidance? | Ownership/snapshot design review and strict cycle/queue/channel/continuation comparison; implementation waits for approval of paused redesign |
| Graphics portability and fidelity | Which gaps are unavailable source tiles versus lighting/material/render path behavior? | Matched camera/state/assets native/stock captures, tile availability, material/normal-map and limb/eclipsing cases |
| C++ host modernization | Which global/lifecycle assumptions obstruct independent worlds and reliable teardown? | Ownership graph, module callback/resource lifetime inventory and small demonstrable isolation changes |
| Rust simulation or SDK redesign | Does a rewrite materially improve a measured bottleneck or correctness boundary over incremental C++ changes? | Scoped prototype, stable FFI/source-compatibility plan, differential tests, performance/maintenance costs and addon impact |
| Distribution and multi-OS builds | What is the smallest legally and technically complete package another machine can build/run? | Component audit, reproducible source/data recipe, clean native Windows/Linux results and relocation/install test |

The renderer is already Rust/wgpu. That does not select Rust for physics,
guidance, mission systems or the addon SDK. Evaluate incremental C++ services,
targeted Rust boundaries and a broader rewrite against the same evidence.

## Suggested first research deliverable

Write a source-backed decision brief comparing incremental upstream adoption,
targeted service extraction and a scoped Rust-core prototype. For each, identify
the measurable problem, ownership/API changes, addon/asset compatibility,
validation plan, removal path, costs and unresolved risks. Rank experiments by
how much uncertainty a small reversible prototype resolves, not by language.

First close source/publication provenance and define reference observability.
Then choose one bounded experiment with an explicit success/failure criterion.
Do not bundle physics changes, snapshot redesign, visual tuning and CI activation
into the same experiment. Preserve failed evidence and update the decision record
with what was learned before expanding scope.
