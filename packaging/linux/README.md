# Linux runtime artifact recipe

The separate `linux-runtime.yml` job builds the native host, pinned NASSP v9
modules and Rust/wgpu client together on Ubuntu 24.04 x64. It runs the existing
headless capability selection and uploads reports. SDL is built with Linux
desktop backends available. No window, GPU, input device, audio device or
relocated installation is exercised by that job.

`packaging/linux/package.py` is an opt-in staging recipe. Its default review
contract in `clearance.json` blocks binary artifact creation. The integration
owner may enable it only after recording the specified source/media, resource,
notice, and Linux graphical/relocation reviews. A manual GitHub Actions run may
request packaging; an uncleared request fails before copying files or uploading.
The current general Mac packaging contract remains separate.

When cleared, the artifact contains the `orbiter` CLI, `liborbiter.so`, Lua,
NASSP and celestial modules, `WgpuClient.so`, and reviewed generated NASSP BMP
resources. It keeps the build-relative layout and rejects ELF paths back to the
CI build or Nix store. It includes the exact committed repository source, six
verified pinned dependency checkouts, locked Cargo vendor source, applicable
notice files, source provenance and file hashes. The staging step refuses
modified or missing source, incomplete builds, unresolved ELF dependencies and
failed headless tests. Reviewers must confirm that the collected notice texts
cover the resolved target graph; copying files alone does not clear that gate.

The artifact contains **no original release434 NASSP data or planetary
archives**. After extracting to its final directory, independently acquire the
pinned NASSP data under its own terms and run:

```sh
python3 stage_external_data.py --nassp-data /absolute/path/to/pristine-nassp-v9
./orbiter --root runtime-original-insertion \
  --scenario 'Apollo - Mission Scenarios/Apollo 11/Apollo 11 - 02 - Insertion' \
  --graphics WgpuClient --window --vc
```

The helper verifies the exact v9 data and every recorded asset before creating
a fresh runtime. It preserves the original scenario bytes and binds external
data by path. The runtime needs a Vulkan-capable Linux desktop and installed
SDL/X11 or Wayland, audio and graphics system libraries. Ubuntu 24.04 is the
build ABI baseline; compatibility with older glibc or other distributions must
be checked on the target system. The historical strict AGC save/resume failure
remains a separate acceptance issue.
