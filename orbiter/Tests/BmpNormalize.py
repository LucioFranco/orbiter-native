import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from bmp_normalize import normalize


def bitmap(stream):
    data = bytearray(54 + 1024)
    data[:2] = b"BM"
    struct.pack_into("<I", data, 10, len(data))
    struct.pack_into("<IiiHHI", data, 14, 40, 5, 3, 1, 8, 1)
    # Stable palette bytes ensure conversion does not modify the colour table.
    data[54:] = bytes(range(256)) * 4
    return bytes(data) + bytes(stream)


# Encoded run with padding, absolute odd-length run and its alignment byte,
# delta skipping pixels and rows, end-of-line and end-of-bitmap.
source = bitmap([8, 7, 0, 0, 0, 3, 1, 2, 3, 0, 0, 2, 1, 1, 1, 9, 0, 1])
result = normalize(source)
assert result[54:1078] == source[54:1078]
assert struct.unpack_from("<I", result, 30)[0] == 0
assert struct.unpack_from("<I", result, 2)[0] == len(result)
assert result[1078:] == bytes([7]*8 + [1,2,3,0,0,0,0,0] + [0,0,0,0,9,0,0,0])
assert normalize(result) == result
for stream in ([1], [9, 1, 0, 1], [0, 2, 0, 3, 0, 1], [0, 3, 1]):
    try:
        normalize(bitmap(stream))
    except ValueError:
        pass
    else:
        raise AssertionError("Malformed RLE stream accepted")
print("RLE8 conversion and malformed input checks passed")
