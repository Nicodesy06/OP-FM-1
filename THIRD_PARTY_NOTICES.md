# Third-party notices

FM1 Tape OS è distribuito sotto **GPL-3.0-only** (`LICENSE`), perché incorpora codice GPL-3.0 di Felucca e Dinghy.

| Componente | Percorso | Origine | Licenza | Note |
|---|---|---|---|---|
| ANALOG engine, DSP helpers, generatore di tabelle | `third_party/felucca/dsp.c`, `eng_analog.c`, `gen_tables.py` | [hugelton/Felucca](https://github.com/hugelton/Felucca), commit in `third_party/felucca/UPSTREAM_COMMIT`. Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments | GPL-3.0-only | copiati senza modifiche |
| Estratti di `core.h` | `third_party/felucca/felucca_types.h` | idem | GPL-3.0-only | righe copiate verbatim; aggiunto un `track_t` minimo (indicato nel file) |
| Platform FM1 (boot, HAL, USB, OTA, rescue, LCD, gfx, libc, loader, linker script, crt0) | `firmware/hal`, `firmware/loader`, `firmware/gen`, `firmware/src/{platform,usb,ota,recovery,lcd,gfx,libc}.c`, `bootguard.h`, `uac_fir.h`, `firmware/app.ld`, `firmware/crt0.S` | [zvenson/dinghy](https://github.com/zvenson/dinghy), commit in `firmware/DINGHY_UPSTREAM_COMMIT`; a sua volta da Felucca / SLOOP (isod89) / sloopDX | GPL-3.0-only (vedi `firmware/DINGHY_LICENSING.md`) | copiati senza modifiche |
| Strumenti di build/pacchetto/installazione | `firmware/tools/{build,fm1pkg_make,lz4blk,fm1_install}.py`, `get_toolchain.sh` | Dinghy | GPL-3.0-only | `build.py` adattato (intestazione del file); gli altri senza modifiche |
| Font del platform: Terminus 8x16 resa in `felucca_font.h` | `firmware/gen/` | Dinghy/Felucca (Terminus di Dimitar Zhekov) | SIL OFL 1.1 (`firmware/assets/fonts/Terminus-LICENSE.txt`) | usato solo dalle schermate di update/rescue di Dinghy |
| JieLi AC79 SDK (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`) | **non incluso** | Jieli-Tech | Apache-2.0 | servirebbe per il `.fwsc`; da procurarsi separatamente |
| JieLi toolchain (clang 4.0.1 pi32v2) | **non inclusa** | JieLi | proprietaria / termini JieLi | scaricata da `get_toolchain.sh` |

Il resto (`src/`, `platform/`, `web/`, `tests/`, `tools/`, `docs/`, `research/`) è stato scritto per questo progetto. Il font 5×7 (`tools/gen_font.py`) e le grafiche sono originali.

**Materiale escluso:** nessun firmware, binario, SVG, font, campione o preset di teenage engineering (OP-1) è incluso. FM1 Tape OS è ispirato al workflow dell'OP-1 e al firmware #248 di Maiko Pruett. Non è un prodotto teenage engineering, M-VAVE o Maiko Pruett, né è affiliato o approvato da loro.
