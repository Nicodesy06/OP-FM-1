#!/bin/sh
# Download the OP-1 firmware files used by this research into private/fw
# (never committed; see .gitignore) and verify them against the hashes
# recorded in research/FIRMWARE_EXTRACTION.md.
set -e
D="${1:-private/fw}"; mkdir -p "$D"; cd "$D"
curl -fsSLO https://maikopruett.com/firmware/op1_248.op1
curl -fsSLO https://maikopruett.com/firmware/op1_247.op1
curl -fsSLO https://teenage.engineering/_software/op-1/op1_246.op1
curl -fsSLO https://teenage.engineering/_software/op-1/op1_245.op1
sha256sum -c <<'SUMS'
836208af25f2658598e2fe8bf70ca450fc34dc8abeb9f69873a4cc2196018dd5  op1_248.op1
226e4e287ca5f1834fc6e505fbc1f6ae45026430ee403da39044bc55cea034c0  op1_247.op1
c5315218f825f143b415ca554516541898abee843d3a236df0b54c04e1fb13a9  op1_246.op1
d499f6a72dee13c9e7968c83ce3daefd780572020d5aacbda933bb79c7ba36d6  op1_245.op1
SUMS
