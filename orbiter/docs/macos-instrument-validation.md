# Native cockpit instrument validation

The LM X-pointer needles are visible again. The white dial background is original
NASSP artwork; the defect was a reflection that made its black needles white.
This change continues from clean-checkout commit `d621bd4` and is confined to the
native WGPU shader and engine tests. Original NASSP source, ROMs, equations,
scenarios, textures and both adaptation manifests are unchanged.

## X-pointer diagnosis and fix

The original `LM_VC.msh` has 686 groups. CDR needles are groups 677/679 and LMP
needles 678/680, as defined in `LM_VC_Resource.h`. They use material `Black`:
black diffuse RGB, white specular RGB and specular power zero. Native WGPU
previously evaluated a near-zero specular exponent, producing an almost uniform
highlight. The black pointers disappeared against the white dial. The same
problem altered other matte indicator colors, including lunar contact and LPD.

Orbiter's existing D3D9 client disables specular RGB for material powers below
0.1 in `OVP/D3D9Client/Mesh.cpp`, `D3D9Mesh::SetMaterial`. The native shader now
uses that threshold. It retains highlights for reflective materials, including
view-dependent highlights, material shininess and colored local lights. Neither
meshes nor vessel instrument state are modified to obtain the result.

`Tests/WgpuMesh.cpp` adds a GPU check for black zero/near-zero-power materials
under sunlight and a local light. It failed before the fix and passes afterward.
`Tests/NativeXPointer.h` independently checks the actual NASSP geometry in both
CDR and LMP views: it hides each device-mesh needle in turn, counts the black
pixels that disappear from the frozen frame, restores the needle, and requires
an exact image restoration. A bright Moon or blank white dial cannot pass.
Source mesh flags must remain intact. The test temporarily enables specular
rendering and restores the prior setting. It runs in the existing orbital LM
and lunar liftoff gates, including the unchanged ten-vessel original mission.

The new real-LM check was also run with the unchanged pre-fix candidate renderer.
It failed specifically with zero black CDR needle pixels. The corrected renderer
passes in both source/data profiles. Before/after captures and that negative
result are preserved alongside the final suite logs; see
`upstream/validation-visual-instruments-2026-10-07.json` for locations and hashes.

The first complete post-fix suite exposed an exterior-test assumption: the LM
view's required 100 highlight pixels included the incorrect matte reflections.
The corrected original LM has 60 highlight pixels in that view. The exterior
check now requires actual visible highlights, sets device-material powers to
zero and requires equality with the specular-disabled frame, then restores all
materials and requires exact image restoration. CSM highlights remain covered
as well. The source meshes stay unchanged.

## Covered instrument paths

The final baseline and October candidate suites check the new X-pointer pixel
behavior alongside existing real spacecraft tests. Those include CSM/LM DSKY
mouse input and AGC/LGC display responses, original instrument redraws reaching
mesh atlases, background restoration, view switching, and save/reload. The FDAI
checks cover original ball textures, attitude response, rate/error needle
movement, LM power-loss indication and the CSM's retained unpowered state.
These are native rendering and input checks. They do not establish numerical
agreement with a Windows reference or complete operational procedures.

To repeat the relevant gates after following
[clean-macos-demos.md](clean-macos-demos.md):

```sh
ctest --test-dir build/native-poc \
  -R '^(wgpu_mesh|wgpu_csm_cockpit|wgpu_insertion_cockpit|native_lm|native_original_liftoff|native_fdai|fdai_rasterizer)$' \
  --output-on-failure
```

Use `build/native-candidate` for the isolated October profile. CTest supplies the
required staging fixtures. The complete suite remains the final integration
gate. These automated checks create their own windows; no live CUA validation or
physical-controller session is claimed. Lighting and material matching beyond
the corrected matte behavior still need a Windows visual comparison. Complete
legacy 2D panel presentation remains separate work. Follow-up native MCC screen
bindings and the original-scenario guided-descent diagnostic are described in
[macos-demo-validation.md](macos-demo-validation.md). The earlier descent probe
omitted later pilot actions and original terrain; its recorded impact is not a
validated complete procedure or an independently established guidance defect.
