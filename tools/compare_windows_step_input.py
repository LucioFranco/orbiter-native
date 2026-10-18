# Copyright (c) 2026 Lucio Franco and contributors
# SPDX-License-Identifier: MIT
"""Strict Windows controls: complete vessel blocks and trace gate, no tolerance."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import difflib

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--evidence-root', type=Path, required=True)
ap.add_argument('--output', type=Path, required=True, help='Fresh derived-output directory')
ap.add_argument('--require-neutral-gate', action='store_true')
ap.add_argument('cases', nargs='+')
args = ap.parse_args()
evidence = args.evidence_root.resolve()
root = args.output.resolve()
if root.exists() or args.output.is_symlink() or root.is_relative_to(evidence) or evidence.is_relative_to(root):
    ap.error('Output must be fresh and separate from raw evidence')
if any(Path(c).name != c or c in ('.', '..') for c in args.cases):
    ap.error('Case names must be single directory names')
root.mkdir(parents=True)
captures = {}
for case in args.cases:
    directory = evidence / case
    raw = (directory / 'saved.scn').read_bytes()
    normalized = raw.replace(b'\r\n', b'\n').replace(b'\r', b'\n')
    vessel = re.search(rb'^BEGIN_SHIPS\n(.*?)^END_SHIPS\n?', normalized, re.M | re.S)
    if not vessel:
        raise RuntimeError('Complete vessel section missing: ' + case)
    blocks = vessel.group(0)
    derived = root / case
    derived.mkdir()
    (derived / 'complete-vessel-blocks.lf.txt').write_bytes(blocks)
    mjd = re.search(rb'^\s*Date MJD\s+([\d.]+)', normalized, re.M).group(1).decode()
    launch = json.loads((directory / 'launch.json').read_text(encoding='utf-8-sig'))
    result = json.loads((directory / 'result.json').read_text(encoding='utf-8-sig'))
    counters = {}
    for name in ('Columbia', 'Eagle'):
        section = re.search(rb'^' + name.encode() + rb':[^\n]+\n(.*?)^END$', blocks, re.M | re.S).group(1)
        agc = re.search(rb'^AGC_BEGIN\n(.*?)^AGC_END$', section, re.M | re.S).group(1)
        counters[name] = re.search(rb'^\s*CYCLECOUNTER\s+(\d+)', agc, re.M).group(1).decode()
    capture = {'rawSHA256': hashlib.sha256(raw).hexdigest(), 'normalizedScenarioSHA256': hashlib.sha256(normalized).hexdigest(),
               'completeVesselBlocksSHA256': hashlib.sha256(blocks).hexdigest(), 'MJDToken': mjd, 'AGCCounters': counters,
               'launch': launch, 'result': result, 'hostGuestSaveHashMatch': result.get('SavedStateSHA256', '').lower() == hashlib.sha256(raw).hexdigest(),
               'trace': None, 'traceGatePass': launch['Mode'] == 'off'}
    trace = directory / 'trace.tsv'
    if trace.exists():
        rows = [line.split('\t') for line in trace.read_text().splitlines()]
        posts = {int(row[1]): row[2:] for row in rows if row[0] == 'post'}
        inputs = {int(row[1]): [int(x) for x in row[2:]] for row in rows if row[0] == 'input'}
        ends = [row for row in rows if row[0] == 'end']
        end = [int(x) for x in ends[-1][1:]] if len(ends) == 1 else None
        t = {'SHA256': hashlib.sha256(trace.read_bytes()).hexdigest(), 'start': rows[0], 'postRows': posts, 'inputRows': inputs, 'end': end}
        target = launch['TargetTotalSteps']
        if launch['Mode'] == 'neutral':
            capture['traceGatePass'] = rows[0] == ['start', 'neutral'] and target in posts and posts[target][1] == '0x3f91111111111111' and end == [target, target+1, 0, 0, 0]
        elif launch['Mode'] == 'pulse':
            down = [step for step, values in inputs.items() if values[0] == 1]
            capture['traceGatePass'] = (rows[0] == ['start', 'pulse'] and target in posts and posts[target][1] == '0x3f91111111111111'
                                        and end == [target, target+1, 18, 18, 0] and down == list(range(302, 320))
                                        and inputs.get(320) == [0, 0] and inputs.get(target+1) == [0, 0])
        capture['trace'] = t
    capture['endpointGatePass'] = mjd == ('40422.8318700056' if launch['TargetTotalSteps'] == 559 else '40422.8318237093') and set(counters.values()) == ({'795022'} if launch['TargetTotalSteps'] == 559 else {'453688'})
    capture['cleanupGatePass'] = all(result.get(key) for key in ['NormalExit', 'StateFresh', 'ConfigsRestored', 'CurrentStateRestored', 'QuicksaveUnchanged', 'TemporaryPluginRemoved', 'WrapperRemoved', 'TaskRemoved', 'CoreBinariesUnchanged'])
    captures[case] = capture
comparisons = []
reference = args.cases[0]
reference_text = (root / reference / 'complete-vessel-blocks.lf.txt').read_text()
for case in args.cases[1:]:
    text = (root / case / 'complete-vessel-blocks.lf.txt').read_text()
    diff = ''.join(difflib.unified_diff(reference_text.splitlines(keepends=True), text.splitlines(keepends=True), fromfile=reference, tofile=case, n=3))
    dest = root / (reference + '-vs-' + case + '.diff')
    dest.write_text(diff)
    comparisons.append({'reference': reference, 'case': case, 'completeVesselBlocksEqual': not diff, 'diff': str(dest.relative_to(root)),
                        'changedLineCount': sum(line.startswith(('+', '-')) and not line.startswith(('+++', '---')) for line in diff.splitlines())})
report = {'cases': captures, 'comparisons': comparisons, 'normalization': 'CRLF and bare CR to LF only; all complete serialized vessel records retained; no numeric tolerance',
          'fourNeutralRunsPresent': set(args.cases) == {'off-a', 'off-b', 'neutral-a', 'neutral-b'},
          'neutralGatePass': set(args.cases) == {'off-a', 'off-b', 'neutral-a', 'neutral-b'} and all(c['endpointGatePass'] and c['cleanupGatePass'] and c['hostGuestSaveHashMatch'] and c['traceGatePass'] for c in captures.values()) and all(c['completeVesselBlocksEqual'] for c in comparisons)}
target = root / ('comparison-' + '-'.join(args.cases) + '.json')
target.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'caseSummaries': {case: {key: c[key] for key in ['MJDToken', 'AGCCounters', 'rawSHA256', 'endpointGatePass', 'cleanupGatePass', 'traceGatePass']} for case, c in captures.items()}, 'comparisons': comparisons, 'neutralGatePass': report['neutralGatePass']}, indent=2))

checks = all(c['endpointGatePass'] and c['cleanupGatePass'] and c['hostGuestSaveHashMatch'] and c['traceGatePass'] for c in captures.values())
if args.require_neutral_gate:
    checks = checks and report['neutralGatePass']
raise SystemExit(0 if checks else 1)
