"""Isolated planetary DDS fixture exercising actual CSM/LM frame rendering."""
import pathlib
import struct
import subprocess
import sys
import zlib
import math

build, stage, executable, plugin = map(pathlib.Path, sys.argv[1:5])
addon_args = ["--addon", sys.argv[5]] if len(sys.argv) > 5 else []
runtime = build / "runtime-terrain-textures"
subprocess.run([sys.executable, str(stage), str(build), "--pair", "--output", str(runtime)] + addon_args, check=True)
# Remove only this fixture's known synthetic height files from earlier runs.
for hemisphere in range(2):
    elevation = runtime / "Textures/Moon/Elev/04/000000" / f"{hemisphere:06d}.elv"
    if elevation.is_symlink():
        raise RuntimeError(f"Refusing to replace elevation asset link: {elevation}")
    if elevation.exists():
        elevation.unlink()
# Four patterned BC1 blocks: parent imagery is intentionally coarse, so the normal
# adaptive terrain selection must share it across higher-resolution meshes.
payloads = {}
for body in ("Earth", "Moon"):
    for hemisphere, color in enumerate((0xf800, 0x07e0)):
        header = bytearray(128)
        header[:4] = b"DDS "
        for offset, value in ((4,124),(8,0x81007),(12,4),(16,4),(20,8),(76,32),(80,4),(108,0x1000)):
            struct.pack_into("<I", header, offset, value)
        header[84:88] = b"DXT1"
        destination = runtime / "Textures" / body / "Surf/04/000000" / f"{hemisphere:06d}.dds"
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.is_symlink():
            raise RuntimeError(f"Refusing to replace existing asset link: {destination}")
        # Distinct rows/columns make texture-coordinate differences observable.
        selectors = sum(((x + 2*y) % 4) << (2*(4*y+x)) for y in range(4) for x in range(4))
        payload = header + struct.pack("<HHI", color, 0x001f, selectors)
        payloads[body, hemisphere] = payload
        destination.write_bytes(payload)
subprocess.run([str(executable), str(plugin), str(runtime), "--terrain"], check=True)
loose_frame = (runtime / "terrain-frame.rgba").read_bytes()

# Now force the same renderer to obtain imagery from a real compressed archive.
# Corrupt loose DDS files exercise archive fallback rather than mere absence.
for body in ("Earth", "Moon"):
    compressed = [zlib.compress(payloads[body, i]) for i in range(2)]
    archive = bytearray(112)
    archive[:4] = b"TX\x01\x00"
    struct.pack_into("<IIIQI", archive, 4, 48, 1, 112, sum(map(len, compressed)), 2)
    struct.pack_into("<IIIII", archive, 28, 0xffffffff, 0xffffffff, 0xffffffff, 0, 1)
    offset = 0
    for i in range(2):
        struct.pack_into("<QI4I", archive, 48+i*32, offset, len(payloads[body, i]), *([0xffffffff]*4))
        offset += len(compressed[i])
        loose = runtime / "Textures" / body / "Surf/04/000000" / f"{i:06d}.dds"
        loose.write_bytes(bytes(128))
    destination = runtime / "Textures" / body / "Archive/Surf.tree"
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink():
        raise RuntimeError(f"Refusing to replace archive asset link: {destination}")
    destination.write_bytes(archive + b"".join(compressed))
print("Verifying compressed Surf.tree fallback", flush=True)
subprocess.run([str(executable), str(plugin), str(runtime), "--terrain"], check=True)

archive_frame = (runtime / "terrain-frame.rgba").read_bytes()
if not loose_frame or loose_frame != archive_frame:
    raise RuntimeError("Loose and archived patterned terrain produced different framebuffers")
print(f"Loose/archive framebuffer equality verified ({len(loose_frame)} bytes)", flush=True)

# A different image must change actual pixels, not merely residency metadata.
for body in ("Earth", "Moon"):
    for hemisphere in range(2):
        payload = payloads[body, hemisphere][:128] + struct.pack("<HHI", 0xffff, 0, 0)
        destination = runtime / "Textures" / body / "Surf/04/000000" / f"{hemisphere:06d}.dds"
        destination.write_bytes(payload)
subprocess.run([str(executable), str(plugin), str(runtime), "--terrain"], check=True)
changed_frame = (runtime / "terrain-frame.rgba").read_bytes()
changed_pixels = sum(loose_frame[i:i+3] != changed_frame[i:i+3] for i in range(0, len(loose_frame), 4))
if changed_pixels < 100:
    raise RuntimeError(f"Terrain imagery changed only {changed_pixels} visible pixels")
print(f"Terrain image sensitivity verified: {changed_pixels} changed pixels", flush=True)

# Keep white imagery fixed while changing only lunar elevation. A smooth field
# across both hemispheres exercises the simulation elevation loader and mesh
# sampling together, including inherited low-resolution heights.
for hemisphere in range(2):
    header = bytearray(100)
    header[:4] = b"ELE\x01"
    struct.pack_into("<6i", header, 4, 100, -16, 259, 259, 1, 1)
    struct.pack_into("<9d", header, 28, 1, 0, -math.pi/2, math.pi/2,
                     -math.pi+hemisphere*math.pi, hemisphere*math.pi, -3000, 3000, 0)
    heights = []
    for y in range(259):
        lat = -math.pi/2 + (y-1)*math.pi/256
        for x in range(259):
            lon = -math.pi + hemisphere*math.pi + (x-1)*math.pi/256
            heights.append(round(3000*math.cos(lat)*math.sin(8*lon)))
    destination = runtime / "Textures/Moon/Elev/04/000000" / f"{hemisphere:06d}.elv"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(header + struct.pack("<"+"h"*len(heights), *heights))
subprocess.run([str(executable), str(plugin), str(runtime), "--terrain"], check=True)
elevated_frame = (runtime / "terrain-frame.rgba").read_bytes()
elevated_pixels = sum(changed_frame[i:i+3] != elevated_frame[i:i+3] for i in range(0, len(changed_frame), 4))
if elevated_pixels < 100:
    raise RuntimeError(f"Elevation changed only {elevated_pixels} visible pixels")
print(f"Terrain elevation sensitivity verified: {elevated_pixels} changed pixels", flush=True)
