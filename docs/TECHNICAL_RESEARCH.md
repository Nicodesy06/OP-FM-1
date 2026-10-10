# Ricerca tecnica: repository, componenti, scelta della base

Data: 2026-10-10. I repository sono clonati in `references/`, che non è versionato e punta alla directory di ricerca. Per l'analisi del firmware OP-1 vedi `research/`.

## 1. Repository esaminati

**Legenda della colonna "Stato verificato da noi":**
- **compila** = compilato in questa sessione;
- **gira** = eseguito in questa sessione;
- **dichiarato** = affermazione del README, non verificata da noi.

| Repository | Commit | Licenza | Cosa è | Stato verificato da noi |
|---|---|---|---|---|
| hugelton/Felucca | 129a4cf (2026-10-09) | GPL-3.0 | firmware FM1 completo (13+ motori, sequencer 4 tracce, UI, flash, USB), test host, emulatore web (Emscripten) | i file vendorizzati (`dsp.c`, `eng_analog.c`, `gen_tables.py`) **compilano** per host, wasm32 e pi32v2, e **suonano** nei nostri test. Il build completo di Felucca non è stato fatto: richiede l'SDK AC79. Pacchetti rilasciati e web installer: **dichiarato** |
| zvenson/dinghy | 9297300 (2026-10-09) | GPL-3.0 | piattaforma FM1 minima: boot guard, rescue USB, updater, USB-MIDI/UAC, pannello, LCD, settings; un'app sinusoidale di 200 righe | il platform vendorizzato **compila e linka** con la nostra app (pi32v2), con i controlli di build superati. "Runs on the FM-1, tested on the device": **dichiarato** |
| isod89/sloop-fm1 | fa9ce57 (2026-10-08) | GPL-3.0 | groovebox su Felucca: 4 tracce (3 synth + drum), 12 motori, undo di sequenze (`seq.c`: copia degli step della traccia) | letto il codice; "2.5, running on the FM-1, beta": **dichiarato** |
| zvenson/dxsloop | 17e395b (2026-10-09) | GPL-3.0 | variante DX7/FM di SLOOP | letto il README; "installed and played on a real FM-1": **dichiarato** |
| ip2k/lunar-modulator | 40c9430 (2026-10-10) | MIT | firmware modulare in sviluppo, simulatore browser | README: "not installed on an FM-1 yet". **Solo host/browser** |
| AL-255/FM-1-RE | ec832f2 (2026-09-08) | WTFPL | reverse engineering del firmware stock: SoC, memoria, boot, protocollo OTA, sicurezza | usato per i dati hardware (vedi `research/FM1_HARDWARE_LIMITS.md`) |
| kurogedelic/FM-1-transporter | a632d92 (2026-10-01) | MIT | lettura e scrittura della flash via RP2040 sulle linee USB (UBOOT della mask-ROM) | letto il README; è la via di recupero più bassa documentata. **Nessuna operazione eseguita** |
| op1hacks/docs, op1repacker | 05e13a2 / 390b18e | – / MIT | formato del firmware OP-1, SVG, font | usati nella ricerca `research/` |
| op1emu/op1emu | 5ad01e5 (2026-10-10) | MIT | emulatore OP-1 (Blackfin JIT, bcore) | **compila e gira**: te-boot raggiunge il menu; l'OS OP-1 richiede NAND/OTP di un dispositivo (`research/TEST_RESULTS.md` T7) |
| AlbertSmit/TOP-1 | e641d27 (2017-12-25) | MPL-2.0 | workstation ispirata all'OP-1: C++17, Raspberry Pi, JACK, float | letto; **non portabile su FM1** (float, C++/STL, Linux). Riferimento concettuale per tape a 4 tracce e mixer |
| Maiko Pruett OS #248 | – | proprietario (nessun sorgente pubblico trovato) | patch binaria di OS #246 (undo/redo del nastro, naming) | analizzato in `research/OP1_246_VS_248.md` |

Baud Girl FM-1+VA e Groove OS sono stati usati solo come riferimento funzionale: i loro sorgenti non risultano pubblici. Le discussioni Reddit (r/MVaveFM1, r/OP1users) non sono state consultate in questa sessione.

## 2. Matrice per componente

| Componente | Implementazione trovata | Percorso | Stato verificato | Compatibilità FM1 | Licenza | Memoria | Pro | Contro | Decisione |
|---|---|---|---|---|---|---|---|---|---|
| Piattaforma (boot, USB, update, rescue, pannello, LCD) | Dinghy `platform.c` + `hal/` + `usb.c`, `ota.c`, `recovery.c` | `references/zvenson_dinghy/firmware` | linka con la nostra app; controlli superati | sì (pi32v2) | GPL-3.0 | RAM 96 KiB, pool 336 KiB, slot 568 KiB | minimale, update e rescue sempre raggiungibili, controlli di build anti-brick | nessuna UI né audio engine | **base piattaforma** (vendorizzata in `firmware/`) |
| Motore synth | Felucca `eng_analog.c` (2 osc band-limited, noise, drive, SVF trapezoidale) + `dsp.c` | `references/hugelton_Felucca/firmware/src` | suona, golden hash, costo host misurato | sì, fixed point, nessun float | GPL-3.0 | voce ~100 B + tabelle in flash | stabile, rilasciato, preset inclusi | accoppiato al `track_t` di Felucca | **riusato verbatim**; voice manager nostro |
| Altri motori (FM6/msfa, PHYS, DRUM…) | Felucca `eng_*.c` | idem | non integrati | sì | GPL-3.0 (FM6 deriva da msfa, Apache-2.0) | da misurare | molti motori pronti | dipendenze da `voice.c`/`mod.c` di Felucca | roadmap |
| Batteria | Felucca `drum_voice.c` (953 righe) | idem | non integrata | sì | GPL-3.0 | – | completa | dipendenze | per ora una drum synth **nostra** (8 suoni); Felucca DRUM in roadmap |
| Rendering UI | Dinghy `gfx.c` (canvas a bande + font), Felucca `ui_*.c` | – | Dinghy linkato (usato solo da OTA/rescue) | sì | GPL-3.0 | canvas 30 KB | provato sul pannello | stile Felucca, non OP-1 | renderer **nostro** su framebuffer 240×240 (pool) + invio delle righe cambiate |
| Sequencer / tape | Felucca/SLOOP `seq.c` (step a 64 passi), SLOOP undo (copia degli step) | – | letto | sì | GPL-3.0 | – | step sequencer maturo | modello a step, non nastro lineare | **EventTape nostro** (timeline lineare, overdub, loop) con undo per operazioni |
| Simulatore web | Felucca `web/emu` (Emscripten, worklet), Lunar (Emscripten) | – | letto | – | GPL-3.0 / MIT | – | architettura provata | richiede emsdk | **stessa architettura** (wasm in AudioWorklet, clock guidato dall'audio) ma con **clang wasm32** senza Emscripten |
| Recovery | Dinghy rescue (OCT- all'accensione, boot guard), FM-1-transporter (UBOOT ROM via RP2040) | – | letto | – | – | – | più livelli di recupero documentati | **nessuno verificato da noi** | conservati intatti; vedi `docs/BUILD_FM1.md` |

## 3. Cosa compila per l'FM1 fisico e cosa no

- **Compilato da noi per pi32v2:** FM1 Tape OS, cioè il platform Dinghy più l'app condivisa. Il link passa con tutti i controlli di `build.py`. Il pacchetto `.fwsc` **non** è stato prodotto (manca l'SDK AC79, vedi `docs/TEST_REPORT.md`).
- **Su FM1 secondo i loro autori** (non verificato da noi): Felucca, Dinghy, SLOOP, sloopDX.
- **Solo host/browser:** Lunar Modulator (per sua dichiarazione) e TOP-1 (Linux/RPi).

## 4. Limiti reali dell'FM1 (sintesi; dettagli in `research/FM1_HARDWARE_LIMITS.md`)

- pi32v2 a 240 MHz, senza FPU.
- La divisione 64/32 è un'istruzione (verificato nel disassemblato); quella 64/64 non ha runtime.
- 512 KiB di SRAM: 96 KiB `.data/.bss`, 336 KiB pool, ~32 KiB di stack.
- Flash 1 MiB, di cui 568 KiB di slot app.
- LCD 240×240 su SPI (115 KB per frame intero).
- Audio a 44,1 kHz, ISR ogni 5,8 ms (256 frame).
- Una cancellazione della flash blocca tutto per decine di ms.

## 5. Rischi

- **Prima installazione mai fatta:** il firmware non è mai stato eseguito su un FM1. Timing dell'LCD, carico CPU reale e comportamento del pannello vanno verificati sul dispositivo, solo dopo autorizzazione.
- **Licenza:** il firmware incorpora codice GPL-3.0 (Felucca, Dinghy), quindi l'intero progetto è GPL-3.0.
- **Packaging:** l'SDK AC79 si scarica solo da gitee, irraggiungibile da questo ambiente.
- **Recupero:** il percorso di recupero non è testato da noi. I meccanismi esistono (rescue di Dinghy, UBOOT della ROM), ma FM-1-RE avverte che il recupero da un'interruzione non è dimostrato per tutti i casi.
