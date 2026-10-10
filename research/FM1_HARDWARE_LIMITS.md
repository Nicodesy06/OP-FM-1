# M-VAVE FM1: risorse reali e vincoli

Non abbiamo un FM1 a disposizione e nessuna misura è stata fatta sul dispositivo. Ogni riga riporta la propria fonte. "Verificato qui" significa riprodotto in questa sessione con i file indicati.

## 1. Specifiche

| Risorsa | Valore | Fonte |
|---|---|---|
| SoC | JieLi AC791N / WL82 | FM-1-RE `README.md`, `docs/architecture.md`: identificato da pacchetto, SPL e SDK |
| CPU / ISA | pi32v2, 32 bit, istruzioni a 16/32 bit LE, sintassi algebrica | FM-1-RE `architecture.md`; **verificato qui** con la toolchain JieLi (§3) |
| Compatibilità binaria con Blackfin | **assente** | **verificato qui** (§3) |
| Clock | 240 MHz (PLL da quarzo 24 MHz) | FM-1-RE `architecture.md`: `pll_clock_init` |
| FPU | assente | Dinghy `docs/RULES.md`: "No float, no double… the link fails" |
| Divisione 64 bit | assente nel runtime (`__udivdi3` non disponibile) | Dinghy `docs/RULES.md` |
| SRAM interna | 0x01C00000–0x01C80000 = **512 KiB** in tutto, con la parte alta riservata da 0x01C7FD50 (boot info, mailbox, vettori) | Dinghy/Felucca `firmware/app.ld`; FM-1-RE (hwinfo a 0x01C7FD50) |
| Flash SPI NOR | **1 MiB**, mappata XIP a 0x02000000 | FM-1-RE `architecture.md` §2 |
| Slot applicazione (XIP) | 0x8DFBC = **581 564 B** | Dinghy `tools/fm1pkg_make.py` `APP_SLOT` |
| Storage utente (stock) | regione USR 0x12000 = 72 KiB; VM (config KV) 0x55000 = 340 KiB | FM-1-RE `architecture.md` §2 (layout del firmware stock) |
| SDRAM/PSRAM esterna | nessuna risulta mappata | FM-1-RE: nessuna regione SDRAM nella mappa |
| Display | 240×240 LCD SPI; un frame intero è 115 KB su SPI | Dinghy `README.md`, `docs/RULES.md` |
| Audio | DAC interno, 44 118 Hz nel firmware stock; budget ISR in Dinghy di 5,8 ms per 256 frame | FM-1-RE `architecture.md` §4; Dinghy `docs/RULES.md` |
| Input | 27 tasti, 14 pulsanti, 7 encoder, manopola MASTER (in Dinghy) | Dinghy `README.md` |
| USB | dispositivo USB-MIDI + UAC (in Dinghy/Felucca) | Dinghy/Felucca `README` |
| Bootloader | Mask ROM → SPL `uboot.boot` → app; update via USB-MIDI (UFW `.fwsc`) | FM-1-RE `architecture.md` §3, `docs/io/11-ota-protocol.md` |

## 2. Budget di memoria per un firmware basato su Dinghy/Felucca

| Regione (`app.ld` di Dinghy) | Indirizzo | Dimensione | Uso |
|---|---|---|---|
| XIP (codice + costanti) | 0x02000120 | 0x8DFBC (568 KiB) | `.text`, `.rodata` |
| RAMTEXT | 0x01C00000 | 24 KiB | codice che gira con la flash occupata |
| (debug) | 0x01C06000 | 8 KiB | – |
| RAM `.data`/`.bss` | 0x01C08000 | 96 KiB (limite controllato dal build) | stato |
| POOL | 0x01C20000 | 0x54000 = 336 KiB | buffer grandi (linee FX, canvas) |
| Stack utente / sistema | 0x01C74100 / 0x01C7A100 | ~24 KiB / ~8 KiB | – |
| NOINIT | 0x01C7C000 | 0x3D50 | sopravvive al reset |

**RAM utile per dati: circa 432 KiB** (96 + 336). Questa cifra comprende il framebuffer: 240×240×2 = 112 500 B se si tiene un framebuffer RGB565 intero in RAM. È una scelta di progetto, non un obbligo: Dinghy disegna per primitive.

## 3. Esperimento: pi32v2 non esegue codice Blackfin

Si compila una funzione C con la toolchain JieLi (`clang 4.0.1`, `-target pi32v2 -O2`). Il codice ottenuto si decodifica poi come Blackfin con bftool. Script e output sono in `TEST_RESULTS.md` T9.

| Parola | pi32v2 (objdump JieLi) | Blackfin (decoder bcore) |
|---|---|---|
| `0x1810` | `r0 += r1` | `if cc jump 0x…` |
| `0x0080` | `rts` | `jump (pc + P0)` |
| `0x0475` | `[--sp] = {rets, r5, r4}` | illegale |

In Blackfin `rts` è 0x0010. Le due ISA condividono lo stile della sintassi (algebrica, hardware loop `if (--r2 != 0) goto`, MAC 16×16) ma **non la codifica**. Il codice macchina OP-1 non può essere eseguito direttamente: serve emulazione o traduzione (vedi `PORTING_STRATEGIES.md`).

## 4. Confronto quantitativo OP-1 vs FM1

| Risorsa | OP-1 | FM1 | Conseguenza |
|---|---|---|---|
| CPU | ADSP-BF524 (Blackfin). Clock non verificato; op1emu modella 400 MHz | pi32v2 a 240 MHz | ISA incompatibile (§3) |
| FPU | assente (Blackfin) | assente | Il firmware OP-1 usa float (tabelle float32 a 0x018AB7CC…, assert `start_float >= 0.f` in `SampleBuffer.cpp`), quindi su BF524 sono emulati in software; su FM1 nessun runtime float (§1): il codice tradotto che usa float richiede una libreria soft-float da fornire |
| RAM | SDRAM ≥ 48,3 MB (indirizzo massimo toccato 0x0303FFFF) | 512 KiB SRAM, ~432 KiB per dati | **Il loader OP-1 riserva da solo ~23,2 MB (#246) / ~25,2 MB (#248) di SDRAM: ~52× / ~57× la RAM dati dell'FM1 (432 KiB)** |
| Codice inizializzato | 2 078 244 B (#246) / 2 238 268 B (#248) in SDRAM + 80 564 B in L1 | slot XIP di 581 564 B | **L'immagine OP-1 non sta nello slot applicazione, nemmeno senza risorse** (3,6× per #246) |
| Risorse (campioni, SVG, DB) | 26,4 MB nel TAR, su NAND da 512 MiB (MT29F4G08 in op1emu) | flash totale 1 MiB | I campioni di fabbrica da soli (~22 MB) non stanno |
| L1 veloce | 48 KB istruzioni + 64 KB dati (BF524) | n/d (nessuna L1 equivalente documentata) | il codice L1 è comunque cifrato (vedi BINARY_ANALYSIS §4) |
| Throughput DSP | **da misurare** | **da misurare** | nessun microbenchmark eseguito su FM1 (niente dispositivo); su host vedi `TEST_RESULTS.md` |
| Display | 320×160 visibili secondo op1hacks (pannello 320×240); op1emu salva frame 400×200 | 240×240 | risoluzione e proporzioni diverse: il rendering originale va riscalato o ritagliato |
| Input | tastiera 24 tasti + ~20 pulsanti + 4 encoder (op1hacks `README`, op1emu `ui.json`) | 27 tasti, 14 pulsanti, 7 encoder | mappatura possibile, ma con differenze (da documentare in un adapter) |
| Storage persistente | NAND con YAFFS2 (`/yaffs2/...`) | NOR 1 MiB; USR 72 KiB nello stock | il nastro OP-1 (PCM su yaffs2 + `tape.db`) non ha spazio equivalente |

## 5. Implicazioni per il Tape Engine (solo calcolo, nessuna prova sull'hardware)

PCM 16 bit mono a 44,1 kHz richiede 88 200 B/s.

| Risorsa | Spazio | Durata massima di 1 traccia mono 16 bit / 44,1 kHz |
|---|---|---|
| POOL in RAM (336 KiB) | 344 064 B | ≈ 3,9 s |
| Regione USR stock (72 KiB) | 73 728 B | ≈ 0,8 s |
| Flash teoricamente libera (1 MiB − slot app 581 564 B − 16 672 B fino all'inizio dell'app) | ≈ 450 340 B | ≈ 5,1 s, con usura della NOR e stallo di decine di ms per ogni erase (Dinghy `RULES.md`) |

Il nastro OP-1 ha 4 tracce di registrazione continua e su NAND dura minuti. **Su FM1 non modificato non è riproducibile come registrazione PCM continua a 4 tracce.** Questa conclusione è un calcolo di capacità; nessuna delle alternative (sample rate ridotto, compressione, streaming verso host via USB audio, memoria esterna) è stata verificata sull'hardware.

## 6. Toolchain JieLi: cosa è stato verificato

- `clang 4.0.1`, target `pi32v2`: compila C a `-O2` e produce ELF32-pi32v2 (verificato qui).
- Il compilatore emette di suo l'hardware loop (`if (--r2 != 0) goto`) e la moltiplicazione 16×16 (verificato qui).
- Non verificati qui: il comportamento di `int64_t`, la mancanza di `float` in link e i limiti di dimensione. Sono riportati da Dinghy `RULES.md`.
- Il build completo di Dinghy richiede anche tre file del JieLi AC79 SDK (gitee). Non è stato ancora scaricato né provato.
