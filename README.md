# OP-1 #248 → M-VAVE FM1: research repository

Work in progress. This repository contains only tools and notes. No OP-1 firmware,
extracted files or memory images are committed: they stay in `private/`, which is
ignored by git.

## Tools

| Path | What it does |
| --- | --- |
| `tools/fetch_firmware.sh` | Downloads op1_245–248.op1 into `private/fw` and checks their SHA-256 |
| `tools/ldr/op1unpack.py` | Checks the CRC32 of an `.op1` file, unpacks the LZMA → TAR payload and writes an inventory |
| `tools/ldr/ldrimg.py` | Parses Blackfin LDR blocks and builds a flat guest memory image |
| `tools/ldr/xtea_probe.py` | Decrypts CALLBACK blocks with a key you supply and prints entropy statistics (it does not derive keys) |
| `tools/bfin/` | `bftool`, built on op1emu's bcore: disassembles original code and calls a single original function |
| `tools/analysis/funcmap.py` | Recovers functions from the disassembly: call graph, CFG walk, MMR/L1 use |
| `tools/analysis/xref.py` | Adds string cross-references and attributes functions to source files through assert strings |

## Verified so far

- The `.op1` format is CRC32 + LZMA + TAR. Between #246 and #248 only `OP1_vdk.ldr` changes.
- SDRAM code is plaintext. Only the L1 `CALLBACK|INDIRECT` blocks are XTEA-encrypted, and the key
  is provisioned per device into OTP. With an all-zero key the decrypted bytes stay random
  (entropy ≈ 7.99 bits/byte).
- #248 adds a new block at 0x01E00000 and redirects 12 original CALL/JUMP instructions plus
  2 table pointers into it. The added code names its hooks (`hook_H1_rec_on` … `hook_H11_name_format`).
- Function recovery finds 3,679 functions reached by direct calls (about 1.03 MB of code).
  917 of them are attributed to a source file through assert strings.

## Not yet done

- Translating and comparing a function, and compiling it for Pi32v2.
- Booting firmware in op1emu, which needs a NAND and OTP dump from a real device.
- The documents in `research/`.
- Nothing has been done on FM1 hardware.
