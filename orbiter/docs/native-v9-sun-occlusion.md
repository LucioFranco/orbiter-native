# Sunlight visibility below the mean lunar radius

In the original v9 Lunar Liftoff T-1m scenario, Eagle starts on terrain about
1,992.215 m below the Moon's mean-radius sphere. After the stock 1/60-second
initialization at MJD 40423.7519591566, the Sun is 22.021947 degrees above the
local radial horizon. The native graphics client's spherical segment test
incorrectly classified the Moon as an obstruction at the ray origin. External
vessel lighting then received a zero sunlight vector, leaving the existing
ambient/material/local-light contributions.

The correction in `Src/WgpuClient/ParticleLighting.h` skips an individual body
when its centre projects strictly behind the observer along the Sun ray. This
matches the sunlit-side condition `ca > 0` in the pinned OpenOrbiter D3D9
`OVP/D3D9Client/VPlanetAtmo.cpp::SunLightColor(VECTOR3,double)` for an airless
body. Each other body is still tested against the finite light segment. Exact
radial-horizon and forward-ray intersections retain the original behavior.
The helper supplies vessel, cockpit and diffuse-particle visibility.

No brightness floor, exposure, material, texture, terrain or simulation equation
changes accompany this correction. The core dylib and native app executable
remain byte-identical; the WgpuClient module and affected CPU tests are rebuilt.

`sun_occlusion` records all 13 checks independently. Before the correction, the
actual daylight fixture, shallow above-horizon sunlight and separate-body near
miss fail. Night, both elevated limb cases and a separate-body eclipse already
pass. Afterward all checks pass, as do `particle_simulation` and `local_lights`.
The separate original-scenario headless probe changes only its `SunVisible`
field from false to true; its epoch, vessel/body positions, solar elevation and
archive lookup results are unchanged.

Evidence is in
`<NATIVE_EVIDENCE_ROOT>/sun-occlusion-fix-20261012`.
The test fixture comes from the original-scenario probe, not a raised vessel or
edited mission. Earlier actual screenshot and failing evidence are preserved.

The renderer remains a binary, spherical Sun-occlusion approximation. It does
not reproduce D3D9's fractional solar-disc visibility, atmospheric extinction,
or a terrain horizon. A below-datum exact tangent ray is still blocked; elevated
rays clearing the mean sphere remain visible. Complete visual equivalence
remains unverified. The bounded subsequent comparison below verifies the
intended daylight restoration.

Terrain readability is separate: the installed archive supplies level-9 surface
textures at this site, approximately 333 m per pixel, and no site payloads at
levels 10 through 20. Archives and terrain policy are unchanged. Frame residency
is measured in the comparison below; material fidelity remains unaccepted.

An isolated comparison is staged under the evidence directory's
`render-comparison`. Its two fresh runtimes have identical inputs and separate
before/after renderer copies. The compiled helper captures two fixed exterior
cameras offscreen at the same paused initialization boundary, records camera
state and terrain residency, and saves raw PPM pixels and an ordinary scenario.
The Python wrapper defaults to hash verification without GPU initialization.
Run `run_compare.py --capture` only after the coordinator releases the GPU;
the two cases run sequentially with a 240-second cap each. No graphics were
launched during CPU implementation or staging.

The coordinated GPU runs completed normally on 2026-10-12. The first capture
pass exposed a diagnostic-harness error: `SetDistance` changed the serialized
distance but did not rebuild the target-relative camera position. Its default
and close images were identical. That pass, its source and metadata are retained
under `render-comparison/capture-20261012T051052Z` with an explicit limitation.
Only the external capture harness was corrected, using the existing camera API
to rebuild the pose and check the measured global camera distance within 1 mm.

The corrected pass is `render-comparison/capture-20261012T051455Z`. Its two
processes exited 0 in about 31 seconds total. Measured distances are
28.000001 m and 10.703977 m, with target-relative angles 0/0, vertical FOV 72
degrees and a 1280x720 viewport. Each pair has byte-identical camera/epoch
metadata at MJD 40423.751959156602 and simulation time 1/60 s. Both ordinary
saved scenarios are byte-identical, and all 1,732 prepared input hashes remain
unchanged. This is a frozen-state rendering check, not a save/reload guarantee.

Actual pixels show the pale ascent face, gold foil and legs illuminated after
the correction. A fixed face sample changes from mean RGB (5.97,5.97,5.00) to
(180.02,181.48,162.66). The sampled ground and sky pixels remain byte-identical;
no exposure or brightness adjustment accompanies the visibility fix. Raw PPM
captures, exact metadata and lossless PNG copies preserving every RGB byte are
retained. The default and close pairs change 7,243 and 56,290 RGB pixels,
respectively. This accepts sunlight restoration in these bounded views.

Terrain traces are byte-identical. The selected Moon tile containing the site
is requested at level 17 and uses its level-9 source. All four view metadata
records show 368 terrain meshes, 356 textured meshes and 95 resident textures.
Native ground remains flat dark gray, while the stock Windows reference ground
appears black. Terrain readability and that renderer discrepancy remain open.

The actual Windows images show a lit pale face and gold foil with the same
original scenario, nominal epoch and byte-identical Moon archives. This is
qualitative agreement only: viewport/aspect, Sun/material settings and full
camera transforms differ or were not measured. The Windows reference used
software D3D9 under ARM64 Windows emulating x64, and its logs retain three
surface-experiment module-load failures plus material and shutdown warnings.
No complete Windows-scenario or material/photometric parity claim follows.

Removal is confined to the helper's sunlit-side guard, the dedicated CPU test
and its CMake registration. Replace them with equivalent upstream native
visibility handling and coverage when available. The upstream relation is to
OpenOrbiter `bd1d101f348da66e8c5b986633127c05e18d42c8`; NASSP remains
`e731c28c0ebc1a4fe3b63f99e2006a1a8a2b42a9`.

HE PRESS/ASC, START/release/ascent, PDI, physical controllers, complete flight,
manual landing and strict AGC resume/repeatability remain unaccepted. The full
104-test registration was not run for this bounded graphics-helper change.
