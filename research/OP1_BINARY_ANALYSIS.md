# Analisi dell'eseguibile Blackfin `OP1_vdk.ldr`

Strumenti usati:
- `tools/ldr/ldrimg.py`: parser LDR e costruzione dell'immagine di memoria.
- `tools/bfin/bftool`: disassemblatore basato sul decoder di bcore, che nel README dichiara compatibilità con `bfin-elf-objdump`.
- `tools/analysis/*`: recupero delle funzioni.

Tutti i numeri si riferiscono a #246 o a #248 come indicato. Gli SHA-256 sono in `FIRMWARE_EXTRACTION.md`.

## 1. CPU di origine

- **SoC:** ADSP-BF524C2. Fonte: op1hacks/docs `hardware.md`, da foto della scheda. op1emu emula un BF524 e scrive `ADSP-BF524` nell'OTP.
- **ISA:** Blackfin, istruzioni a 16/32/64 bit (pacchetti paralleli), little-endian, 8 registri dati R0–R7 e 6 registri puntatore P0–P5 più SP/FP, due accumulatori a 40 bit A0/A1, loop hardware LC0/LC1, MAC 16×16.
- **Clock:** non verificato. La modalità `--deterministic` di op1emu assume 400 MHz e 1 ciclo per pacchetto, ma è una scelta del modello e non una misura.

## 2. Formato LDR

Il formato è quello BF52x: blocchi con header di 16 byte `{block_code, target_address, byte_count, argument}`. In tutti i blocchi di entrambi i file valgono:
- firma `0xAD` nel byte alto di `block_code`;
- checksum XOR dell'header pari a 0;
- `DMACODE` uguale a 6.

| | #246 | #248 |
|---|---|---|
| Blocchi | 801 | 803 |
| Primo blocco | `IGNORE|FIRST` @0xFFA00000, arg 0x2122D8 | `IGNORE|FIRST` @0xFFA00000, arg 0x239410 |
| `arg` del primo blocco | = dimensione file − 16, cioè l'offset dell'ultimo blocco | = dimensione file − 16 |
| Ultimo blocco | `CALLBACK|IGNORE|INDIRECT` @0xFF907E00, 0xF0 byte | identico |
| Blocco `FINAL` | penultimo, @0xFFA00000 | penultimo |

Il ruolo dell'ultimo blocco di 240 byte non è determinato. Ipotesi: una firma, perché te-boot contiene le stringhe "Signature Key programming succeeded!" e "Found KEY file…". Questa ipotesi non è verificata.

### Occupazione di memoria dichiarata dal loader

| Regione | Tipo | #246 (byte) | #248 (byte) |
|---|---|---|---|
| SDRAM | dati/codice in chiaro (340/341 blocchi) | 2 078 244 | 2 238 268 |
| SDRAM | riempimenti a zero (`FILL`, 370/371 blocchi) | 21 142 547 | 22 938 891 |
| SDRAM | indirizzo massimo toccato | 0x0303FFFF | 0x0303FFFF |
| L1 istruzioni 0xFFA0xxxx | `CALLBACK` (cifrato) | 49 076 | 49 076 |
| L1 dati A 0xFF80xxxx | `CALLBACK` (cifrato) / fill | 856 / 11 356 | idem |
| L1 dati B 0xFF90xxxx | `CALLBACK` (cifrato) / fill | 30 632 / 1 864 | idem |

Il loader riserva quindi circa 23,2 MB di SDRAM per #246 e 25,2 MB per #248. Le zone a zero sono riservate e non necessariamente usate: il working set reale non è misurato. Dall'indirizzo 0x0303FFFF segue che la SDRAM fisica è di almeno ~48,3 MB. La dimensione esatta non è verificata, ma 64 MB è plausibile.

## 3. Mappa delle zone in SDRAM (#246)

| Intervallo | Contenuto osservato |
|---|---|
| 0x01000000–0x01001EA8 | piccoli blocchi iniziali (dati e codice) |
| 0x01001EA8–0x018AB7CC | FILL a zero di 8,7 MB (heap o buffer, ipotesi) |
| 0x018AB7CC–0x018B47D8 | tabelle float32 (pattern di 4 byte, valori vicini a 1.0 e 2.0) |
| 0x018B47D8–0x01AA7078 | codice e dati di sola lettura: qui sono state recuperate le 3 679 funzioni (vedi FUNCTION_MAP) |
| 0x01AA7078–0x01DFFF34 | FILL a zero di 3,5 MB |
| 0x02000000–0x0303FFFF | blocchi piccoli e grandi FILL a zero |
| #248: 0x01E00000–0x01E27118 | **nuovo** blocco di Maiko (codice, tabelle, bitmap) |
| #248: 0x01E27118–0x01FDDA10 | **nuovo** FILL a zero di 1,8 MB (stato dell'undo, ipotesi) |

## 4. Cifratura: cosa è cifrato e cosa no

Secondo op1hacks/docs, i blocchi di codice sono cifrati con XTEA sui primi 8 byte di ogni gruppo di 24, con la chiave nella pagina OTP 0xD0. L'analisi entropica per offset (vedi `TEST_RESULTS.md` T4) precisa questa descrizione:

- **Blocchi SDRAM: in chiaro.** L'entropia è uniforme sugli offset modulo 24 (per esempio 6,86 bit/byte sia sui primi 8 sia sui restanti 16 byte). Il disassemblatore produce codice coerente: prologhi `link`, epiloghi VisualDSP `P0=[FP+4]; unlink; jump (P0)`, ~37 700 `call` dirette.
- **Blocchi `CALLBACK|INDIRECT` (L1): cifrati.** Sui primi 8 byte di ogni gruppo di 24 l'entropia è 7,99, sui restanti 16 è 6,78. Corrisponde allo schema descritto. Questi blocchi sono 80 564 byte, cioè il 3,7 % dei byte inizializzati del loader.
- **Prova con chiave nulla.** È il caso di un `otp.bin` creato vuoto da op1emu. Con `tools/ldr/xtea_probe.py`, XTEA standard a 32 cicli e chiave {0,0,0,0}, l'entropia dei byte decifrati resta 7,99: non risulta codice.
- **Origine della chiave.** Le stringhe in chiaro di te-boot ("Found KEY file...", "KEY is encrypted...", "key file decryption failed!", "Signature Key programming succeeded!", "Check OTP-power connections.") indicano che la chiave viene programmata in fabbrica da un file di chiave a sua volta cifrato.
- **Prerequisito non disponibile.** Per leggere il codice L1 serve l'OTP del proprio OP-1, dumpato legittimamente dal dispositivo. Non abbiamo un dispositivo. Non è stata cercata, derivata né ipotizzata alcuna chiave.

**Conseguenza:** ~1 MB di codice applicativo è analizzabile staticamente. I ~48 KB di codice in L1, che con buona probabilità contengono gli handler di interrupt e le routine DSP più critiche per le prestazioni, non lo sono. Questa probabilità è un'ipotesi basata sulla prassi Blackfin di mettere in L1 il codice più caldo. Si noti che 16 byte su 24 sono in chiaro, quindi anche nei blocchi cifrati due terzi dei byte sono leggibili, ma frammentati.

## 5. Simboli e indizi recuperati

- Il binario non ha tabella dei simboli.
- **Stringhe di assert con percorso sorgente.** Ce ne sono 198 file distinti nel formato `..\src\<dir>\<file>:<linea> <espressione>`. Le directory di primo livello sono `bf/` (driver), `db2/` (database, Clips), `disk/` (yaffs, FAT, sync), `dsp/` (motori di sintesi, effetti, LFO, `tape_c.cpp`, `audio_clipboard.cpp`, `SampleBuffer.cpp`…), `gui/` (Screen, Raster, AGG, `svg_loader.cpp`, schermate, `TapeScreen.cpp`), `main/ctls/` (controller: `TapeController.cpp`, `global_controller.cpp`, `SeqManager.cpp`…), `seq/`, `midi/`, `shared/`, `lib_json/`. Ci sono anche file `.c` con i nomi tipici di SQLite (`btree.c`, `vdbe.c`, `pager.c`…).
- Altre stringhe utili:
  - `"Rev. 00246; 2022/11/09 16:17:00; 8.1.11.1"`;
  - query SQL della tabella `clips` (`INSERT INTO clips VALUES (%i,%i,%i,%i)`, `UPDATE clips SET ch=%i,start=%i,stop=%i WHERE id=%i`, `DELETE FROM clips WHERE id = %i`);
  - percorsi `/yaffs2/...`, `/album/side_a.aif`;
  - `.\OP_Main.cpp:138 !((int)start_of_stack & 3)`.
- Gli entry point da cui parte il recupero delle funzioni sono i target delle `call` dirette (vedi FUNCTION_MAP). L'entry point del boot dell'applicazione non è stato isolato: op1emu salta al primo blocco `FIRST` dopo che te-boot ha eseguito il callback.

## 6. Dipendenze hardware visibili staticamente

Su 3 679 funzioni recuperate in #246:

| Criterio (immediato alto caricato in un registro) | Funzioni |
|---|---|
| Accesso a MMR (≥ 0xFFC0xxxx) | 93 |
| Riferimento a L1 SRAM (0xFF80–0xFFBF) | 673 |
| Foglie senza chiamate, chiamate indirette, MMR o L1 | 709 |

Il criterio è conservativo. Un indirizzo passato come argomento o letto da una struttura non viene rilevato, quindi "foglia senza dipendenze" non significa automaticamente "portabile".

## 7. te-boot.ldr

- Contiene 2 DXE: un DXE di init in L1 e il DXE principale caricato da 0x00000004 (0x29290 byte) con area dati in 0xFF80xxxx. Nessun blocco è cifrato.
- Le stringhe mostrano un menu di servizio (upload firmware, test, factory reset, format), un filesystem YAFFS2 su NAND (`/yaffs2/OP1_vdk.ldr`, `/yaffs2/te-boot.ldr`) e la programmazione della chiave OTP.
- È identico in tutte le versioni esaminate. Eseguito in op1emu raggiunge il proprio menu: vedi `TEST_RESULTS.md` T7.
