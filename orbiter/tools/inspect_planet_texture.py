#!/usr/bin/env python3
"""Inspect archived planetary DDS metadata at a geographic coordinate, read-only."""
import argparse
import math
import pathlib
import struct
import zlib


def inspect(archive, latitude, longitude, first, last):
    with archive.open('rb') as stream:
        header = stream.read(48)
        if len(header) != 48 or header[:4] != b'TX\x01\x00':
            raise ValueError('Not an Orbiter texture archive')
        header_size, flags, data_offset = struct.unpack_from('<III', header, 4)
        data_size, count = struct.unpack_from('<QI', header, 16)
        if header_size != 48 or data_offset < 48 + count * 32:
            raise ValueError('Invalid texture archive index')
        roots = struct.unpack_from('<5I', header, 28)
        nodes = [struct.unpack('<QI4I4x', stream.read(32)) for _ in range(count)]
        for level in range(first, last + 1):
            bands = 1 << (level - 4)
            row = min(bands - 1, int((90 - latitude) / 180 * bands))
            column = int(((longitude + 180) % 360) / 180 * bands)
            index = roots[3 + column // bands]
            for bit in range(level - 5, -1, -1):
                if index == 0xffffffff:
                    break
                index = nodes[index][2 + ((row >> bit) & 1) * 2 + ((column >> bit) & 1)]
            prefix = f'{level:02}/{row:06}/{column:06}'
            if index == 0xffffffff:
                print(prefix, 'missing')
                continue
            offset, size, *_ = nodes[index]
            end = nodes[index + 1][0] if index + 1 < count else data_size
            stream.seek(data_offset + offset)
            packed = stream.read(end - offset)
            data = zlib.decompress(packed) if flags & 1 else packed
            if len(data) != size or len(data) < 128 or data[:4] != b'DDS ':
                raise ValueError(f'Invalid DDS record: {prefix}')
            height, width = struct.unpack_from('<II', data, 12)
            levels = max(1, struct.unpack_from('<I', data, 28)[0])
            print(prefix, f'{width}x{height}', data[84:88].decode('ascii'), f'image_levels={levels}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=pathlib.Path)
    parser.add_argument('latitude', type=float)
    parser.add_argument('longitude', type=float)
    parser.add_argument('--first', type=int, default=4)
    parser.add_argument('--last', type=int, default=18)
    args = parser.parse_args()
    if not (-90 <= args.latitude <= 90 and math.isfinite(args.longitude) and 4 <= args.first <= args.last <= 30):
        parser.error('Require finite coordinates, latitude -90..90 and levels 4..30')
    inspect(args.archive, args.latitude, args.longitude, args.first, args.last)
