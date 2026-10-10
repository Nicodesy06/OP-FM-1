# Mappa delle funzioni di `OP1_vdk.ldr` (#246, valida anche per #248 salvo il blocco di Maiko)

Dati completi, una riga per funzione: `data/function_map_246.tsv`. Per rigenerarli:

```
python3 tools/ldr/ldrimg.py image private/ex/op1_246/OP1_vdk.ldr private/img/vdk246
build/bfin/bftool dis private/img/vdk246.sdram 0x018ab7cc 0x01aa7078 > private/dis/vdk246.dis
python3 tools/analysis/funcmap.py private/dis/vdk246.dis private/dis/funcs246.json
python3 tools/analysis/xref.py private/img/vdk246.sdram private/dis/vdk246.dis private/dis/funcs246.json private/dis/funcs246x.json
python3 tools/analysis/export_tsv.py private/dis/funcs246x.json research/data/function_map_246.tsv
```

## 1. Metodo e limiti

**Disassemblatore.** È il decoder di bcore (op1emu/bcore), che il suo README dichiara allineato all'output di `bfin-elf-objdump`. Non c'è un `bfin-elf-objdump` installato per un confronto diretto: il pacchetto `binutils-multiarch` 2.42 di Ubuntu non include il target bfin. Ghidra non è stato usato: il supporto Blackfin in Ghidra non è stato verificato in questa sessione.

**Confini delle funzioni.**
- Gli entry sono i target delle `call` dirette che cadono su un'istruzione decodificata.
- Da ciascun entry si fa una visita del CFG fino a `rts` o all'epilogo VisualDSP `jump (Px)`.
- Un `jump` verso un altro entry è trattato come tail call.
- Le funzioni raggiunte **solo** tramite puntatori (vtable C++, callback, tabelle come quella patchata a 0x018FC0B0) **non sono incluse**. È il limite principale.

**Attribuzione al sorgente**, in tre classi:
- `assert` (confidenza alta): la funzione stessa costruisce l'indirizzo (`Rx.l`/`Rx.h`) di una stringa di assert che nomina il file sorgente. Un falso positivo possibile è una funzione inline di un header che asserisce per conto di un altro file.
- `near` (confidenza bassa): le funzioni attribuite immediatamente prima e dopo appartengono allo stesso file.
- vuota: nessuna attribuzione.

**Nomi.** I nomi qui sotto sono provvisori e dicono su quali prove si basano. Non attribuiamo un sottosistema sulla sola vicinanza di stringhe.

## 2. Numeri (#246)

- Funzioni recuperate: **3 679**, su 3 683 candidati; 4 scartati perché la visita incontrava parole non decodificabili.
- Byte di codice raggiunti (unione delle istruzioni visitate): **1 029 692**. È il 49,5 % dei 2 078 244 byte inizializzati in SDRAM. Il resto è dati, codice raggiunto solo indirettamente o padding; la ripartizione esatta non è misurata.

| Attribuzione | Funzioni | Byte raggiunti |
|---|---|---|
| `assert` | 917 | 430 010 |
| `near` | 528 | 115 838 |
| nessuna | 2 234 | 488 546 |

| Sottosistema (directory sorgente) | Funzioni `assert` | Funzioni `near` | Byte (assert+near) |
|---|---|---|---|
| file `.c` con nomi SQLite (btree.c, vdbe.c, pager.c…) | 488 | 259 | 323 804 |
| gui/ (Screen, Raster, AGG, svg_loader, schermate) | 67 | 48 | 55 910 |
| db2/ (db2, Clips, BgFile, FileWorker, patch) | 78 | 40 | 44 880 |
| dsp/ (motori, LFO, effetti, tape_c, SampleBuffer…) | 121 | 69 | 43 694 |
| disk/ (fat_manager, filesync_yaffs, Defragmenter…) | 28 | 30 | 23 994 |
| bf/ (driver: audio, codec, nand, usb, gpio, twi, SPI…) | 41 | 24 | 17 014 |
| main/ctls/ (controller, incluso TapeController) | 26 | 12 | 14 342 |
| lib_json/ | 21 | 30 | 9 960 |
| seq/ | 27 | 7 | 8 816 |
| midi/ | 6 | 7 | 1 720 |
| shared/ | 14 | 2 | 1 714 |

Dipendenze hardware dirette: 93 funzioni caricano indirizzi MMR e 673 indirizzi L1. Le foglie senza dipendenze dirette sono 709 (vedi `OP1_BINARY_ANALYSIS.md` §6).

## 3. Funzioni chiave identificate

Colonna "Porting": E = eseguibile isolatamente in emulazione host senza periferiche (stima statica, non ancora provata); D = dipende da L1, MMR o stato globale; ? = da analizzare.

### Boot e sistema

| Indirizzo | Byte | Nome provvisorio | Prove | Confidenza | Porting |
|---|---|---|---|---|---|
| 0x019EFEEC | 4 072 | `op_main_init_and_loop` | stringhe `.\OP_Main.cpp:138 !((int)start_of_stack & 3)`, `/yaffs2/te-boot.op1`; 112 callee diretti; contiene i due punti patchati da Maiko (H3, H10) | media | D (MMR 0xFFC0, L1) |
| 0x01997B60 | 242 | `fat_open_or_create` (ipotesi) | modulo `disk/fat_utils.cpp`; Maiko lo chiama con `/log/mods.txt`, 1 | media | D |
| 0x01A9CE04 | 100 | `format_string` (ipotesi: snprintf-like) | 234 chiamanti; chiamato con R1 = 0x10 o 0x100 (dimensione del buffer, ipotesi); Maiko lo avvolge in H11_name_format | bassa | ? |

### UI

| Indirizzo | Byte | Nome provvisorio | Prove | Confidenza | Porting |
|---|---|---|---|---|---|
| 0x01A3BB20 | 288 | `screen_render_frame` (ipotesi) | modulo `gui/Screen.cpp` (assert `s_raster`); dopo di esso Maiko aggancia il dispatcher per-frame | media | D (L1, 3 chiamate indirette) |
| 0x019B6C74 / 0x019B6C0C | 234 / 100 | `key_event_dispatch` / `encoder_event_dispatch` (ipotesi) | Maiko vi aggancia H8_keys / H9_encoders; vicino a `shared/Keys.cpp` | bassa | D |
| 0x01A5A0E8…0x01A5CA9C | – | funzioni di `gui/screens/TapeScreen.cpp` | assert; riferimenti agli id SVG `track_inactive`, `reelleftmid` | alta (modulo) | D |

### Tape

| Indirizzo | Byte | Nome provvisorio | Prove | Confidenza | Porting |
|---|---|---|---|---|---|
| 0x0195CCEC | 400 | `clips_insert` | `INSERT INTO clips VALUES (%i,%i,%i,%i)`, `Can't add, out of clips!`; 16 chiamanti | alta | D (SQLite) |
| 0x0195CF9C | 1 578 | `clips_split` | `UPDATE clips SET ch=%i,start=%i,stop=%i WHERE id=%i`, `Can't split, out of clips!` | alta | D |
| 0x0195D688 | 342 | `clips_delete` | `DELETE FROM clips WHERE id = %i` | alta | D |
| 0x0195E4A4 | 3 180 | `clips_edit_range` (ipotesi) | UPDATE + DELETE + `out of clips!` | media | D |
| 0x01A581F4 | 942 | funzione di `TapeController` | assert `main/ctls/TapeController.cpp`; 21 chiamanti | alta (modulo) | D (L1) |
| 0x01A58EB8 / 0x01A58ED6 | – | `tape_ctl_arrow_handler` (←/→) | puntatori in tabella a 0x018FC0B0; Maiko li sostituisce con H4_arrows ("hold TAPE and press ← or →") | media | D |
| 0x01A53EC8 | 22 | `tape_set_rec_flag` | scrive il byte R0 in L1 0xFF8029EC; 7 chiamanti; Maiko la sostituisce e conta `rec_on`/`rec_off` a seconda di R0 | media | D (L1) |
| 0x01A54710 | 324 | `tape_write_block` (ipotesi) | contiene il punto H2b_mark e la chiamata H2_writeback (originale 0x01A99F84); `near` di `dsp/tape_c.cpp` | bassa-media | ? (1 hwloop, nessun L1 diretto) |
| 0x01A53F08 | 116 | `tape_find_free_slot` (ipotesi) | foglia, 3 hwloop; scorre una lista (passo 0x14) a partire dal globale 0x01B4EF3C; ritorna 0..3; chiamata da `op_main` | bassa | E (con stato sintetico) |

### Audio / DSP

| Indirizzo | Byte | Nome provvisorio | Prove | Confidenza | Porting |
|---|---|---|---|---|---|
| 0x01A36490 | 296 | `samplebuf_triplet_pass` (ipotesi: post-elaborazione di triple min/max per la forma d'onda) | foglia; MAC frazionari 32×32 su A1 (FU/IS/M), PACK, MAX, 2 hwloop; chiamata da 0x01979634 e 0x01A356B2; `near` di `dsp/SampleBuffer.cpp` (che contiene `size <= 290` e `start_float/end_float`) | bassa per il ruolo, alta per "codice TE puro" | E |
| 0x01A22F3C | 172 | foglia DSP non identificata | 2 hwloop, nessun globale; vicino a `dsp/Ranges.cpp` | bassa | E |
| motori `dsp/*` | – | ~190 funzioni attribuite | assert per file: `Fm.cpp`, `Dssd.cpp`, `Pd.cpp`, `Pls.cpp`, `Iter.cpp`, `dsynth.cpp`, `DrumSampler.cpp`, `SynthSampler.cpp`… | alta (modulo) | per lo più D |

**Nota.** Le routine DSP per campione dei sintetizzatori stanno probabilmente in L1 cifrata: sono i 49 076 byte a 0xFFA00000. È un'ipotesi, coerente con le 673 funzioni che toccano L1. Se è vera, il nucleo DSP dei motori non è recuperabile senza l'OTP del dispositivo.

## 4. Aree non ancora mappate

- **Interrupt, scheduler VDK, timer, allocatore.** Nessuna funzione identificata con confidenza. Gli handler sono probabilmente in L1 cifrata.
- **Parsing SVG, rasterizzazione AGG, font.** Sono noti i moduli (`gui/svg_loader.cpp`, `gui/lib/AGG/agg.cpp`, `gui/lib/Raster.cpp`, `gui/screens/Font.cpp`), non le funzioni singole oltre agli assert.
- **Sequencer** (`seq/*`) e **polifonia** (`dsp/VoiceCtl.cpp`). Sono noti solo i moduli.
- **Funzioni raggiunte solo indirettamente.** Il loro numero non è misurato: serve la ricostruzione delle vtable.
