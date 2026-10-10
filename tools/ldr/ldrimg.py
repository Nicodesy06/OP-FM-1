#!/usr/bin/env python3
"""Blackfin BF52x LDR parser + memory image builder for OP-1 OP1_vdk.ldr.

usage:
  ldrimg.py list  FILE.ldr                 # block table
  ldrimg.py image FILE.ldr OUT_PREFIX      # writes OUT_PREFIX.sdram (flat 0x00000000..0x04000000)
                                           # and OUT_PREFIX.map.json (block provenance)
Blocks flagged CALLBACK are processed by te-boot's boot callback (XTEA, key in
OTP page 0xD0, see op1hacks/docs); their payload is written *as stored*
(i.e. still encrypted) and marked encrypted=true in the map.
"""
import sys, struct, json, hashlib
FL = {0x10:'SAFE',0x20:'AUX',0x100:'FILL',0x200:'QUICKBOOT',0x400:'CALLBACK',0x800:'INIT',
      0x1000:'IGNORE',0x2000:'INDIRECT',0x4000:'FIRST',0x8000:'FINAL'}
SDRAM_BASE, SDRAM_SIZE = 0x00000000, 0x04000000

def parse(d):
    off = 0; blocks = []
    while off + 16 <= len(d):
        h = d[off:off+16]
        code, addr, cnt, arg = struct.unpack('<4I', h)
        x = 0
        for b in h: x ^= b
        fill = bool(code & 0x100)
        payload = b'' if fill else d[off+16:off+16+cnt]
        blocks.append(dict(idx=len(blocks), off=off, code=code, addr=addr, cnt=cnt, arg=arg,
                           sign=code >> 24, hdr_xor_ok=(x == 0), fill=fill, payload=payload,
                           flags='|'.join(v for k, v in FL.items() if code & k)))
        off += 16 + (0 if fill else cnt)
    return blocks

def main():
    cmd, f = sys.argv[1], sys.argv[2]
    d = open(f, 'rb').read()
    bl = parse(d)
    if cmd == 'list':
        for b in bl:
            print(f"{b['idx']:4d} off=0x{b['off']:07x} addr=0x{b['addr']:08x} cnt=0x{b['cnt']:06x} "
                  f"arg=0x{b['arg']:08x} sign=0x{b['sign']:02x} hdr_ok={int(b['hdr_xor_ok'])} {b['flags']}")
    elif cmd == 'image':
        img = bytearray(SDRAM_SIZE); m = []
        for b in bl:
            a, n = b['addr'], b['cnt']
            ent = dict(idx=b['idx'], addr=a, size=n, flags=b['flags'], encrypted='CALLBACK' in b['flags'],
                       sha256=hashlib.sha256(b['payload']).hexdigest() if b['payload'] else None)
            if SDRAM_BASE <= a < SDRAM_BASE + SDRAM_SIZE and n:
                end = min(a + n, SDRAM_BASE + SDRAM_SIZE)
                if b['fill']:
                    img[a-SDRAM_BASE:end-SDRAM_BASE] = struct.pack('<I', b['arg']) * ((end-a)//4) + b'\0'*((end-a)%4)
                else:
                    img[a-SDRAM_BASE:end-SDRAM_BASE] = b['payload'][:end-a]
            m.append(ent)
        open(sys.argv[3] + '.sdram', 'wb').write(img)
        # L1 / other regions written raw as separate files
        for b in bl:
            if b['payload'] and not (SDRAM_BASE <= b['addr'] < SDRAM_BASE + SDRAM_SIZE):
                open(f"{sys.argv[3]}.blk{b['idx']:03d}_{b['addr']:08x}.raw", 'wb').write(b['payload'])
        json.dump(m, open(sys.argv[3] + '.map.json', 'w'), indent=0)
        print("blocks", len(bl), "sdram image sha256", hashlib.sha256(img).hexdigest())

if __name__ == '__main__':
    main()
