#!/usr/bin/env python3
"""Summarize renderer tile provenance and coincident corner gaps from ORBITER_TERRAIN_TRACE."""
import argparse
import collections
import csv
import math

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('trace')
parser.add_argument('--body', default='Earth')
parser.add_argument('--minimum-level', type=int, default=14)
args = parser.parse_args()
rows = {}
with open(args.trace) as stream:
    for row in csv.reader(stream):
        if row[0] == args.body and row[1] == '0':
            rows[tuple(map(int, row[2:5]))] = row
if not rows:
    parser.error('No matching surface tiles')
print('Surface meshes:', len(rows))
print('Mesh/source level counts:', dict(sorted(collections.Counter((int(r[2]), int(r[5])) for r in rows.values()).items())))
maximum = max(address[0] for address in rows)
corners = collections.defaultdict(list)
for (level, latitude, longitude), row in rows.items():
    if level < args.minimum_level:
        continue
    scale = 1 << (maximum - level)
    for k, (dy, dx) in enumerate(((1, 0), (1, 1), (0, 0), (0, 1))):
        point = ((latitude + dy) * scale, (longitude + dx) * scale)
        corners[point].append(((level, latitude, longitude), tuple(map(float, row[11 + 3*k:14 + 3*k]))))
gaps = []
for entries in corners.values():
    for i, (a, p) in enumerate(entries):
        for b, q in entries[i + 1:]:
            gaps.append((math.dist(p, q), a, b))
print('Common-corner pairs:', len(gaps))
print('Gaps above 1 cm:', sum(gap[0] > 0.01 for gap in gaps))
for gap in sorted(gaps, reverse=True)[:8]:
    print('Gap metres / tiles:', gap)
