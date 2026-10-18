"""Original LM contact and ascent on an isolated elevated lunar surface."""
import pathlib
import struct
import subprocess
import sys
build, stage, executable, plugin = map(pathlib.Path, sys.argv[1:5])
addon_args = ["--addon", sys.argv[5]] if len(sys.argv) > 5 else []
runtime = build / "runtime-terrain-contact"
subprocess.run([sys.executable, str(stage), str(build), "--liftoff", "--output", str(runtime)] + addon_args, check=True)
header = bytearray(100)
header[:4] = b"ELE\x01"
struct.pack_into("<6i", header, 4, 100, 0, 259, 259, 1, 1)
struct.pack_into("<9d", header, 28, 1, 1234, 0, 0, 0, 0, 1234, 1234, 1234)
for hemisphere in range(2):
    destination = runtime / "Textures/Moon/Elev/04/000000" / f"{hemisphere:06d}.elv"
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink():
        raise RuntimeError(f"Refusing to overwrite source elevation: {destination}")
    destination.write_bytes(header)
subprocess.run([str(executable), str(plugin), str(runtime), "--terrain"], check=True)
