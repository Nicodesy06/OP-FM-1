# Estrazione del firmware OP-1 #248 (con #245, #246, #247 per confronto)

Strumenti usati: `tools/ldr/op1unpack.py` (scritto da noi) e, per la verifica incrociata della versione, `op1repacker`, `op1_analyze.analyze_unpacked_fw`. Le provenienze sono in `SOURCES.md`.

## 1. Contenitore `.op1`

Il formato descritto da op1hacks/docs risulta confermato su tutti e quattro i file:

```
| CRC32 LE (4 B) | LZMA "alone" (props 0x5d, dict 8 MiB, size sconosciuta) -> TAR |
```

| Versione | CRC32 salvato | CRC32 calcolato sul payload | Corrisponde | TAR (byte) | SHA-256 del TAR |
|---|---|---|---|---|---|
| 245 | 0xe1984cd5 | 0xe1984cd5 | sì | 26 388 480 | `b83fe9d0…b848a0` |
| 246 | 0xcc08445c | 0xcc08445c | sì | 26 368 000 | `ee96790a…8e8171` |
| 247 | 0xf25de3f9 | 0xf25de3f9 | sì | 26 378 240 | `fe4b960b…18dcb` |
| 248 | 0xd33e0fcd | 0xd33e0fcd | sì | 26 531 840 | `5b110895…c74e1` |

Il CRC è uno zlib.crc32 standard calcolato sui byte che seguono i primi 4.

## 2. Contenuto del TAR #248

Contiene 107 file per 26 433 290 byte. #245 ne conteneva 110.

| Directory | File | Byte |
|---|---|---|
| (radice) `OP1_vdk.ldr`, `te-boot.ldr` | 2 | 2 593 284 |
| `content/` (`op1.db`, `op1_factory.db`, `tape.db`, `kerntable.db`) | 4 | 153 600 |
| `content/audio/drum` | 8 | 6 230 840 |
| `content/audio/factory_drum` | 14 | 11 728 066 |
| `content/audio/factory_synth` | 9 | 3 489 938 |
| `content/audio/speech` | 1 | 57 792 |
| `content/audio/synth` | 8 | 529 207 |
| `content/display` (SVG) | 61 | 1 650 563 |

### File chiave

| File | Byte | SHA-256 (#248) | Ruolo |
|---|---|---|---|
| `OP1_vdk.ldr` | 2 331 680 | `d6c3541a9f20af7aeb2ea5b50451af1998e10d44eb0c3f710b1f93c6b658abeb` | Sistema operativo/applicazione (Blackfin LDR). Il nome rimanda a VDK, il kernel di VisualDSP++ (ipotesi). |
| `te-boot.ldr` | 261 604 | `f6233b974bdb6c92f39e73401f206721924ef3ce52abd66b642a5350cebcad93` | Bootloader TE-BOOT v2.30, identico in 245/246/247/248 |
| `content/op1.db` | 37 888 | `cb0d459d…847e17` | Database (SQLite, ipotesi basata sulla presenza del codice SQLite nel firmware) |
| `content/op1_factory.db` | 107 520 | `be457229…8c9cf0` | Preset di fabbrica |
| `content/tape.db` | 4 096 | `38e2c4af…e6254b4` | Database del nastro vuoto. La tabella `clips` è usata in `db2/Clips.cpp` (vedi FUNCTION_MAP). |
| `content/kerntable.db` | 4 096 | `27f625e4…bb6f4` | Kerning del font |

L'inventario completo, con hash per file, si rigenera con `op1unpack.py` e finisce in `private/ex/op1_<v>.inventory.json`.

## 3. Versione effettiva

| | 245 | 246 | 247 | 248 |
|---|---|---|---|---|
| `firmware_version` (op1repacker) | R. 00245 | R. 00246 | R. 00247 | R. 00248 |
| `build_date` / `build_time` | 2022/05/30 15:13 | 2022/11/09 16:17 | 2022/11/09 16:17 | 2022/11/09 16:17 |
| Versione bootloader | 2.30 | 2.30 | 2.30 | 2.30 |
| mtime TAR di `OP1_vdk.ldr` | 2022-05-30 14:26 | 2022-11-09 15:23 | 2022-11-09 15:23 | 2022-11-09 15:23 |

#247 e #248 conservano la data di build e gli mtime del TAR di #246. Cambiano solo il numero di revisione nelle stringhe e il contenuto di `OP1_vdk.ldr` (vedi `OP1_246_VS_248.md`). Questo è coerente con una patch binaria applicata a #246 e non con una ricompilazione dai sorgenti. La documentazione di op1hacks si riferisce a firmware più vecchi (#218–#235), ma il formato del contenitore e la struttura delle directory risultano invariati.

## 4. Differenze di contenuto tra versioni

- **246 → 247 → 248:** cambia soltanto `OP1_vdk.ldr`. `te-boot.ldr`, i database, i font, gli SVG e i campioni sono byte per byte identici. Nessuna risorsa grafica nuova: il popup UNDO/REDO e la schermata di naming di #248 vengono disegnati con bitmap incluse nel nuovo blocco di codice (vedi `OP1_246_VS_248.md` §4).
- **245 → 246:** 3 file in meno (110 → 107) e un `OP1_vdk.ldr` diverso. L'analisi di questa differenza non è oggetto dello studio.
