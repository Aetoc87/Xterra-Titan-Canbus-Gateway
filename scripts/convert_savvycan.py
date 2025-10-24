#!/usr/bin/env python3
import sys,re,csv
LINE = re.compile(r'(?P<ts>\d+)\s*ms\s*ID:0x(?P<id>[0-9A-Fa-f]+)\s*LEN:(?P<len>\d+)\s*DATA:\s*(?P<data>([0-9A-Fa-f]{2}\s*)+)')
def parse_line(line):
    m=LINE.search(line)
    if not m: return None
    ts=int(m.group("ts")); cid=hex(int(m.group("id"),16)); ln=int(m.group("len"))
    data=" ".join(b.upper() for b in m.group("data").strip().split())
    return ts,cid,ln,data
if __name__=="__main__":
    if len(sys.argv)<3:
        print("usage: convert_savvycan.py input.log out.csv"); sys.exit(1)
    inp=open(sys.argv[1]); out=open(sys.argv[2],"w",newline="")
    w=csv.writer(out); w.writerow(["timestamp_ms","id_hex","len","data_bytes"])
    for L in inp:
        p=parse_line(L)
        if p: w.writerow(p)
    print("done")
