# Test eseguiti

Ambiente:
- Linux x86-64, 4 core, Python 3.13, LLVM 15.0.7, GCC di sistema, JieLi toolchain 20260730.1.
- `private/` = firmware ed estratti, non versionati. `build/` = binari compilati, non versionati.
- Nessun test è stato eseguito su hardware FM1 o OP-1.

| ID | Test | Esito |
|---|---|---|
| T1 | Download e hash dei firmware 245–248 | superato |
| T2 | CRC32, LZMA, TAR e inventario | superato |
| T3 | Parsing LDR (header, firma 0xAD, checksum) | superato |
| T4 | Entropia per offset: localizzazione delle parti cifrate | superato |
| T5 | XTEA con chiave nulla sui blocchi CALLBACK | **negativo** (atteso) |
| T6 | Build di op1emu + bcore e di bftool | superato (dopo 2 correzioni d'ambiente) |
| T7 | op1emu: avvio di te-boot su NAND vuota | parziale: menu raggiunto, OS non avviato |
| T8 | Diff 246/248 a livello di istruzioni + mappa degli hook | superato |
| T9 | Compatibilità binaria pi32v2/Blackfin | **negativo**: incompatibili |
| T10 | Recupero delle funzioni e attribuzione | eseguito (metriche sotto) |
| – | Esecuzione isolata di una funzione OP-1 (bftool `run`) | **non eseguito** |
| – | Traduzione Blackfin → C, confronto, build pi32v2 | **non eseguito** |
| – | Microbenchmark su FM1 | **non eseguibile** (nessun dispositivo, flash non autorizzata) |

## T1. Download

```
tools/fetch_firmware.sh        # scarica in private/fw e verifica con sha256sum -c
```

Tutti e 4 gli hash sono riportati in `SOURCES.md`.

## T2. Contenitore

```
python3 tools/ldr/op1unpack.py private/fw/op1_248.op1 private/ex/op1_248
```

Output (#248):

```
file=private/fw/op1_248.op1 size=13075330 crc_stored=0xd33e0fcd crc_calc(payload)=0xd33e0fcd match=True
lzma props=0x5d dict=8388608 uncompressed_size=unknown
tar size=26531840 sha256=5b110895017ca1796d32ecd9059f18187dc65a72b7b989bb31282f9ab01c74e1
files 107 total 26433290
```

Esiti uguali per 245, 246 e 247 (`FIRMWARE_EXTRACTION.md` §1). Verifica incrociata con `op1repacker` (`analyze_unpacked_fw`): versioni R. 00245…R. 00248, bootloader 2.30.

## T3. LDR

`python3 tools/ldr/ldrimg.py list …/OP1_vdk.ldr` legge 801 blocchi (#246) e 803 (#248). Il parsing termina esattamente alla fine del file. Tutti gli header hanno firma 0xAD e XOR uguale a 0.

## T4. Entropia per offset modulo 24

Per ogni blocco si misura l'entropia separata dei byte 0–7 e 8–23 di ogni gruppo di 24 byte.

| Blocco | H(0–7) | H(8–23) | Interpretazione |
|---|---|---|---|
| SDRAM 0x01980C00 (0x4931C B) | 6,86 | 6,86 | in chiaro |
| SDRAM 0x019E9CF0 (0x32404 B) | 6,86 | 6,87 | in chiaro |
| L1 0xFFA00000 (0xBFB4 B, CALLBACK) | 7,99 | 6,78 | primi 8 byte cifrati |
| L1 0xFF901284 (0x1094 B, CALLBACK) | 7,86 | 7,30 | primi 8 byte cifrati |

## T5. XTEA con chiave nulla

```
python3 tools/ldr/xtea_probe.py private/img/vdk248.blk798_ffa00000.raw 0,0,0,0
entropy stored8=7.99 decrypted8=7.99 plaintext16=6.78
```

La decifratura con chiave nulla non produce codice. È la situazione di un `otp.bin` vuoto creato da op1emu. Non sono state provate altre chiavi.

## T6. Build degli strumenti

- **op1emu** a 5ad01e5, con i submodule bcore b7ff3c8 e bfin_sim 08e9153.
  - Il primo tentativo fallisce: `Xinerama headers not found`.
  - Dopo l'installazione di `libxinerama-dev libxcursor-dev libxi-dev libxrandr-dev libgl-dev` il build riesce (197/197 target).
- **bftool** (`tools/bfin`). Il primo link fallisce per un riferimento mancante a `bcore_profile::detach_perf_listener` con `BCORE_ENABLE_PROFILE=ON`. Con `-DBCORE_ENABLE_PROFILE=OFF` riesce.
- **Comando completo:**
  ```
  cmake -S tools/bfin -B build/bfin -GNinja -DCMAKE_BUILD_TYPE=Release \
    -DBCORE_DIR=<op1emu>/ext/bcore -DLLVM_DIR=/usr/lib/llvm-15/lib/cmake/llvm -DBCORE_ENABLE_PROFILE=OFF
  ```

## T7. op1emu, baseline

```
cd private/emu && ln -s <op1emu>/gui gui
printf 'accel 64 -64 512\nwait-frames 1 90000\nscreenshot boot1.ppm\nwait 20000\nscreenshot boot2.ppm\nquit\n' > s.txt
<op1emu>/build/op1emu nand.img ../ex/op1_246/te-boot.ldr --headless --input-script s.txt
```

- Va eseguito con `gui/ui.json` raggiungibile dalla directory corrente, altrimenti esce con `Failed to open key config: gui/ui.json`.
- **Esito:**
  - te-boot esegue due DXE e 17 chiamate a `delay()`;
  - disegna frame completi 400×200 e mostra "TE-BOOT v2.30 / 1. Upload firmware / shift + 2. Motherboard test / shift + 3. Function test / 7. Factory reset / 8. Format drive" con l'id del chip generato dall'emulatore;
  - SHA-256 del frame: `boot2.ppm` = `81d494bb…02c5`. Le immagini non sono nel repository.
- **Non fatto:** l'installazione di `op1_246.op1`/`op1_248.op1` nella NAND emulata. Richiede l'upload via USB/IP (opzione 1 del menu) e un client usbip, non presente in questo container.
- **Previsione da verificare:** anche dopo l'installazione, con l'`otp.bin` vuoto i blocchi L1 verrebbero decifrati con chiave nulla e il sistema non partirebbe (T5). Per avviare #246/#248 in op1emu servono NAND e OTP dumpati da un OP-1 reale, come indica il README di op1emu ("Run … with the matching `otp.bin`").
- **Compatibilità di #248 con op1emu: non determinata.**

## T8. Diff #246/#248

Diff blocco per blocco degli LDR e disassemblato di entrambe le versioni a ±8 byte da ogni differenza (`bftool dis`). Le 17 zone sono tutte interpretabili (`OP1_246_VS_248.md`, `data/op1_248_patches.tsv`). Gli 11 contatori degli hook sono ciascuno incrementato dentro esattamente un target dei trampolini.

## T9. pi32v2 contro Blackfin

```
cat > t.c: int add(int a,int b){return a+b;}  int mac(const short*x,const short*y,int n){...}
~/.jieli/toolchain/pi32v2/bin/clang -target pi32v2 -O2 -c t.c -o t.o
objdump (JieLi):  0: 10 18  r0 += r1    2: 80 00  rts    4: 75 04  [--sp] = {rets, r5, r4}
bftool dis (Blackfin) sugli stessi byte:  1810 if cc jump …   0080 jump (pc + P0)   0475 illegal
```

**Esito:** codifiche incompatibili. Lo stesso test mostra che la toolchain JieLi compila C a `-O2` e genera da sé hardware loop e MAC 16×16.

## T10. Recupero delle funzioni

```
build/bfin/bftool dis private/img/vdk246.sdram 0x018ab7cc 0x01aa7078 > private/dis/vdk246.dis   # 773 502 righe
python3 tools/analysis/funcmap.py private/dis/vdk246.dis private/dis/funcs246.json
  candidate entries=3683 functions=3679 union_code_bytes=1029692
python3 tools/analysis/xref.py … → functions=3679 with_module=917 with_strings=1337
```

Per #248, con il blocco di Maiko incluso: 3 700 funzioni, 1 037 970 byte.

**Limiti noti:**
- Le funzioni raggiunte solo indirettamente sono escluse.
- Il disassemblato lineare contiene 69 070 parole `illegal`: sono dati nella zona di codice, non errori del decoder (non verificato contro objdump).
