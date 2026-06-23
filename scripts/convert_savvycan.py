#!/usr/bin/env python3
"""
convert_savvycan.py  --  text CAN log -> CSV for analysis.

Handles two formats:
  1. This project's sniffer output:
       12345 ms  ID:0x180  LEN:8  DATA: 0A 1B 2C ...
     and marker lines:
       12345 ms  # MARKER: T=Cold
  2. SavvyCAN "GVRET"/generic text exports (timestamp,id,...,data bytes).

Output CSV columns: time_ms, id_hex, dlc, b0..b7, marker

Usage:
    python3 convert_savvycan.py input.txt output.csv
"""

import sys
import re
import csv

SNIFF_RE = re.compile(
    r'^\s*(\d+)\s*ms\s+ID:0x([0-9A-Fa-f]+)\s+LEN:(\d+)\s+DATA:\s*([0-9A-Fa-f ]*)'
)
MARKER_RE = re.compile(r'^\s*(\d+)\s*ms\s+#\s*MARKER:\s*(.*)$')


def parse_line(line):
    m = MARKER_RE.match(line)
    if m:
        t = int(m.group(1))
        return {'time_ms': t, 'id_hex': '', 'dlc': '', 'bytes': [], 'marker': m.group(2).strip()}
    m = SNIFF_RE.match(line)
    if m:
        t = int(m.group(1))
        idhex = m.group(2).upper()
        dlc = int(m.group(3))
        data = m.group(4).split()
        return {'time_ms': t, 'id_hex': idhex, 'dlc': dlc, 'bytes': data, 'marker': ''}
    # try generic comma format: time,id,ext,dir,bus,len,b0,b1,...
    parts = [p.strip() for p in line.split(',')]
    if len(parts) >= 6 and parts[0].replace('.', '').isdigit():
        try:
            t = int(float(parts[0]))
            idhex = parts[1].replace('0x', '').upper()
            dlc = int(parts[5])
            data = parts[6:6 + dlc]
            return {'time_ms': t, 'id_hex': idhex, 'dlc': dlc, 'bytes': data, 'marker': ''}
        except (ValueError, IndexError):
            return None
    return None


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 convert_savvycan.py input.txt output.csv")
        sys.exit(1)
    infile, outfile = sys.argv[1], sys.argv[2]

    rows = 0
    with open(infile, 'r', errors='replace') as f, open(outfile, 'w', newline='') as out:
        w = csv.writer(out)
        w.writerow(['time_ms', 'id_hex', 'dlc'] + [f'b{i}' for i in range(8)] + ['marker'])
        for line in f:
            rec = parse_line(line)
            if rec is None:
                continue
            b = (rec['bytes'] + [''] * 8)[:8]
            w.writerow([rec['time_ms'], rec['id_hex'], rec['dlc']] + b + [rec['marker']])
            rows += 1
    print(f"wrote {rows} rows to {outfile}")


if __name__ == '__main__':
    main()
