#!/usr/bin/env python3
# Copyright (c) 2026 Lucio Franco and contributors
# SPDX-License-Identifier: MIT
"""Explicit, bounded UTM guest command/file transport; never starts a VM.

Derived from the retained reference helper. VM identity and executable path are
caller inputs. A transport success does not certify a guest script's outcome;
scripts must return a result/exit status for their own action.
"""
import argparse
import base64
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vm', required=True, help='Existing VM UUID from utmctl list')
    parser.add_argument('--utmctl', default='/Applications/UTM.app/Contents/MacOS/utmctl')
    parser.add_argument('--timeout', type=int, default=45)
    parser.add_argument('action', choices=('ps', 'pull', 'push'))
    parser.add_argument('path', help='Local PowerShell/input file, or guest pull path')
    parser.add_argument('destination', nargs='?', help='Guest push path or new local pull file')
    args = parser.parse_args()
    if not 0 < args.timeout <= 600:
        parser.error('--timeout must be 1..600 seconds')
    if args.action in ('pull', 'push') and not args.destination:
        parser.error('File transport requires an explicit destination')
    if args.action == 'pull' and (Path(args.destination).exists() or Path(args.destination).is_symlink()):
        parser.error('Preserve existing evidence; choose a new local destination')
    try:
        if args.action == 'ps':
            encoded = base64.b64encode(Path(args.path).read_text(encoding='utf-8-sig').encode('utf-16le')).decode('ascii')
            argv = [args.utmctl, 'exec', args.vm, '--cmd',
                    r'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',
                    '-NoProfile', '-NonInteractive', '-EncodedCommand', encoded]
            result = subprocess.run(argv, capture_output=True, timeout=args.timeout)
        elif args.action == 'push':
            result = subprocess.run([args.utmctl, 'file', 'push', args.vm, args.destination],
                                    input=Path(args.path).read_bytes(), capture_output=True, timeout=args.timeout)
        else:
            result = subprocess.run([args.utmctl, 'file', 'pull', args.vm, args.path],
                                    capture_output=True, timeout=args.timeout)
        # utmctl has returned zero on failed Apple Events; stderr is a failure.
        status = result.returncode or (1 if result.stderr else 0)
        if args.action == 'pull' and status == 0:
            with Path(args.destination).open('xb') as stream:
                stream.write(result.stdout)
            print('Retrieved', len(result.stdout), 'bytes to', args.destination)
        else:
            sys.stdout.buffer.write(result.stdout)
        sys.stderr.buffer.write(result.stderr)
        return status
    except (OSError, subprocess.TimeoutExpired) as error:
        print('UTM transport:', error, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
