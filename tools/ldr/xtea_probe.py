#!/usr/bin/env python3
"""Probe: decrypt the first 8 bytes of every 24-byte group of a CALLBACK block
with a *given* key (standard XTEA, 32 cycles, little-endian words) and report
statistics. Used only to show what a blank OTP (all-zero key) yields; no key
is guessed or derived here."""
import sys, struct, collections, math
def xtea_dec(v0, v1, k, n=32):
    delta, mask = 0x9E3779B9, 0xffffffff
    s = (delta * n) & mask
    for _ in range(n):
        v1 = (v1 - ((((v0 << 4) ^ (v0 >> 5)) + v0) ^ (s + k[(s >> 11) & 3]))) & mask
        s = (s - delta) & mask
        v0 = (v0 - ((((v1 << 4) ^ (v1 >> 5)) + v1) ^ (s + k[s & 3]))) & mask
    return v0, v1
def H(b):
    c = collections.Counter(b); n = len(b); return -sum(v/n*math.log2(v/n) for v in c.values())
d = open(sys.argv[1], 'rb').read()
key = [int(x, 16) for x in sys.argv[2].split(',')] if len(sys.argv) > 2 else [0, 0, 0, 0]
out = bytearray(d)
for j in range(0, len(d) - 7, 24):
    v0, v1 = struct.unpack_from('<2I', d, j)
    struct.pack_into('<2I', out, j, *xtea_dec(v0, v1, key))
enc8 = bytes(x for j in range(0, len(d) - 7, 24) for x in d[j:j+8])
dec8 = bytes(x for j in range(0, len(d) - 7, 24) for x in out[j:j+8])
pl16 = bytes(x for j in range(0, len(d) - 24, 24) for x in d[j+8:j+24])
print(f"entropy stored8={H(enc8):.2f} decrypted8={H(dec8):.2f} plaintext16={H(pl16):.2f}")
if len(sys.argv) > 3: open(sys.argv[3], 'wb').write(out)
