# Stock Windows and native step-indexed yaw comparison

The bounded original-scenario keyboard/ACA comparison is complete. Both
platforms request numpad3 for steps 302–319, release before 320 and settle
neutrally through step 559. Actual saved yaw, channel 031 and RCS consumption
confirm that stock NASSP receives the input. This is evidence for these
boundaries and this input protocol, not full mission or full-state parity.

The received Windows bundle SHA-256 is
`6ae7b917e326e5d3a195334c95db390ca4fd14f46763b61612aedcf52bfb6bde`.
All 153 listed files and seven raw-save hashes verify. Each capture exits
normally with result 0; launch records retain the original scenario hash,
full-precision fixed step, original physics configuration and reviewed DLL.
The four Windows 559-step neutral captures are byte-identical, SHA-256
`a467c9b4bb07618cbb5a0b2f818f51c8423c2f4f7b7c8b8b66cf95f0682e6df6`.
The four earlier native neutral captures also match exactly within their own
platform. The diagnostic's serialized-state neutrality is established for this
559-step bound on both platforms.

At neutral and pulse boundaries for steps 319 and 559, both vessels' STATUS,
RPOS, RVEL, AROT, VROT and PRPLEVEL tokens match cross-platform exactly. Complete
saved AGC and DSKY blocks match after CRLF normalization. MJD tokens are
`40422.8318237093` at 319 and `40422.8318700056` at 559; both AGC counters are
453688 and 795022 respectively. Input-request lists and final trace summaries
also match: 18 requested/active frames, correct release and zero faults.
These saved trajectory fields have finite precision; no full-precision inertial
trajectory equality or unsampled guidance equivalence is inferred.

The actual Eagle response agrees at saved precision, independently of callback
counts:

| Observation | Neutral | Pulse | Pulse minus neutral, equal on both platforms |
| --- | --- | --- | --- |
| Yaw rate at step 319, degrees/s | -0.0012 | -0.6028 | -0.6016 |
| Channel 031 saved VICHAN025 at 319 | 28671 | 12279 | Changed ACA input bits |
| Each RCS tank remaining fraction at 319 | Baseline | Lower | -0.000308 |
| Yaw rate at step 559, degrees/s | -0.0012 | +0.0080 | +0.0092 |
| Channel 031 saved VICHAN025 at 559 | 28671 | 28671 | Returned to neutral |
| Each RCS tank remaining fraction at 559 | Baseline | Lower | -0.000927 |

The direct input callback remains only a request mechanism: dialogs or another
consuming plugin could block delivery in another run. Here, channel, yaw and
propellant observations independently confirm response. No native reference
values were injected into stock and no equations or mission inputs were changed.

Complete vessel blocks still differ. The machine-readable comparison and raw
diffs retain every field; the residuals are not waived:

| Production pulse residual, native minus stock | Step 319 | Step 559 |
| --- | --- | --- |
| Maximum Columbia fractional PIPA remainder | 6.3e-11 pulse | 1.38e-10 pulse |
| Maximum Eagle fractional PIPA remainder | 1.70e-10 pulse | 8.39e-10 pulse |
| Columbia differing chemical records / maximum energy difference | 2 / 2.32e-10 J | 3 / 4.66e-10 J |
| Eagle differing chemical records / maximum energy difference | 66 / 0.005247588270 J | 68 / 0.007995881606 J |

The maximum Eagle fractional PIPA difference in the neutral control is
1.69e-10 pulse at 319 and 8.38e-10 at 559. Subtracting the corresponding neutral
residual from the pulse residual leaves exactly 1e-12 pulse in Eagle RPZ and
zero in RPX/RPY at both boundaries; Columbia's fractional remainder response
residuals are zero. This is arithmetic on saved fractional remainders, not a
claim about total PIPA response or an acceptance tolerance. The unresolved
absolute residuals remain in the report.

Native correctly restores the two saved running gimbal motors while stock's
known loader bug skips them. At settled pulse step 559, battery energy offsets
remain -0.3883 J (descent C), -0.1941 J (descent D) and -0.1849 J (ascent B).
The ATCA/inverter plate differs by +0.000096 K, and four other radiator tokens
by +0.000001 K. These observations are consistent with the previously isolated
startup effect; this phase introduces no new motor counterfactual or thermal
patch. The recorded water-separator token is 1384.228101 native versus
1384.228102 stock, and Eagle RR_RATE differs by +1e-12 in its saved token.
Precise causes beyond the earlier bounded diagnosis remain unisolated.

Other retained differences include 35 Columbia panel records and Eagle's helium
meter from headless/rendered display cadence, direct-double versus float-loaded
mission clocks (about 13.763 ms), extra native mass/CoG/throttle/audio records,
inertial LASTGLOBALVEL/LASTWEIGHT and ASA remainders. The remaining-field
inventory names and quantifies these, and full raw diffs preserve repeated and
container records. No tolerance, value substitution or full-state waiver is
introduced.

The earlier 60-second no-driver repeat failure and richer-observer comparison
failures remain unchanged. Shared AGC scheduling/pending CDU FIFO resume is
still incomplete and broad redesign stays paused. Physical devices, complete
flight, graphical-native equivalence and manual landing remain unverified.
No further guest capture is needed for this completed bounded comparison.

Windows restored original configs/current state/plugin state, preserved stock
binaries and quicksaves, removed temporary DLLs/tasks/wrappers and confirmed no
capture process remained. The native worker only reads that evidence and adds
comparison documentation; no runtime patch, GPU/VM launch, push or CI occurs.
Exact results and evidence hashes are in
[validation-v9-step-input-comparison-macos-2026-10-12.json](../upstream/validation-v9-step-input-comparison-macos-2026-10-12.json).
