"""Expand Windows BI_RLE8 resources for stb_image without changing pixel indices."""
import struct


def normalize(data):
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("Invalid BMP header")
    offset = struct.unpack_from("<I", data, 10)[0]
    header, width, height, planes, bits, compression = struct.unpack_from("<IiiHHI", data, 14)
    if compression == 0:
        return data
    if header < 40 or planes != 1 or bits != 8 or compression != 1 or width <= 0 or height <= 0:
        raise ValueError("Unsupported compressed BMP format")
    if offset < 14 + header or offset > len(data):
        raise ValueError("Invalid BMP pixel offset")
    stride = (width + 3) & ~3
    pixels = bytearray(stride * height)
    x = y = 0
    pos = offset

    def take(count):
        nonlocal pos
        if pos + count > len(data):
            raise ValueError("Truncated RLE stream")
        value = data[pos:pos + count]
        pos += count
        return value

    # NASSP encoders include scanline padding in some RLE runs.
    def write(values):
        nonlocal x
        if y >= height or x + len(values) > stride:
            raise ValueError("RLE run outside bitmap")
        pixels[y * stride + x:y * stride + x + len(values)] = values
        x += len(values)

    while True:
        count, value = take(2)
        if count:
            write(bytes([value]) * count)
        elif value == 0:
            x = 0
            y += 1
            if y > height:
                raise ValueError("Too many RLE rows")
        elif value == 1:
            break
        elif value == 2:
            dx, dy = take(2)
            x += dx
            y += dy
            if x > stride or y >= height:
                raise ValueError("RLE delta outside bitmap")
        else:
            write(take(value))
            if value & 1:
                take(1)
    result = bytearray(data[:offset]) + pixels
    struct.pack_into("<I", result, 2, len(result))
    struct.pack_into("<I", result, 30, 0)
    struct.pack_into("<I", result, 34, len(pixels))
    return bytes(result)
