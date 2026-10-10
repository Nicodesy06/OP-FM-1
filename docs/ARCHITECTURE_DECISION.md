# Decisione architetturale

## Decisione

La base è **una sola**: il **platform di Dinghy**. Sopra c'è un'**applicazione C nuova e condivisa** (FM1 Tape OS), che importa selettivamente il **motore ANALOG di Felucca**.

| Strato | Origine | Come |
|---|---|---|
| Piattaforma FM1 (`firmware/`) | Dinghy 9297300 | vendorizzato **senza modifiche** (`platform.c`, `hal/`, `usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `loader/`, `app.ld`, `crt0.S`); `build.py` adattato (sorgente, nomi, SDK opzionale) |
| Adattatore FM1 | nostro | `firmware/src/fm1_tapeos.c` (≈100 righe): pannello → `tos_panel`, ISR → `tos_audio`, frame → LCD |
| Applicazione (`src/`) | nostra | synth voice manager, drum, mixer + delay, EventTape + undo/redo, renderer, schermate, mappatura dei controlli |
| Motore synth | Felucca 129a4cf | `third_party/felucca/dsp.c`, `eng_analog.c`, `gen_tables.py` **verbatim**; da `core.h` sono estratti solo i tipi necessari (`felucca_types.h`) |
| Web | nostro | `platform/web/web.c` + `web/` (worklet, pagina); architettura ispirata a Felucca `web/emu` |

## Perché Dinghy e non Felucca come base

1. **L'obiettivo è un'app nuova, non un Felucca modificato.** Felucca è un prodotto completo, con UI, sequencer a step, formati di progetto e protocollo dell'editor. Adattarlo all'esperienza OP-1 (modalità SYNTH/DRUM/TAPE/MIXER, nastro lineare, UI a 4 parametri colorati) richiederebbe di smontarne la maggior parte. Dinghy è proprio "Felucca's platform without the rest".
2. **Sicurezza.** Dinghy conserva le parti che evitano i brick: boot guard, rescue OCT−, updater sempre attivo. Ha inoltre controlli di build che bloccano l'immagine se supera RAM o slot, se chiama la ROM o se tocca registri fuori da `hal/`. La nostra build li supera tutti.
3. **Superficie minima.** Il platform è di ~2,8 k righe che non modifichiamo; tutto il resto è codice nostro, testabile su host e in browser.

## Perché importare il motore ANALOG di Felucca

- È un motore stabile e rilasciato, in fixed point, senza float, con preset.
- Riscriverlo avrebbe prodotto un motore meno provato.
- Le sue dipendenze sono piccole: `dsp.c`, le tabelle generate e i tipi di `core.h`. Importarlo verbatim mantiene la tracciabilità (`UPSTREAM_COMMIT`) e semplifica gli aggiornamenti.
- Il voice manager (ADSR, LFO, allocazione) è nostro, perché in Felucca è intrecciato con tracce, sequencer e modulation matrix.

## Perché non le altre opzioni

- **SLOOP / sloopDX:** ottimi groovebox, ma sono Felucca più step sequencer. Il loro undo copia l'intera traccia di step: lo abbiamo letto, ma per un nastro lineare a 7 livelli conviene un modello per operazioni (vedi ARCHITECTURE §5).
- **Lunar Modulator:** licenza MIT, buona simulazione, ma non ancora installato su un FM1 (dichiarazione del README). Non è una base verificata per l'hardware.
- **TOP-1:** C++17, float, JACK su Linux; non portabile su pi32v2 senza FPU con 512 KiB.
- **Emulazione Blackfin sull'FM1:** esclusa per capacità; la ricerca è in `research/PORTING_STRATEGIES.md`. Il firmware OP-1 serve solo come riferimento di comportamento.

## Uso del firmware OP-1 #248

È un **riferimento funzionale**, non una fonte di codice o di asset:
- undo/redo del nastro fino a 7 registrazioni indietro;
- combinazione TAPE tenuto + ←/→ (qui SEQ tenuto + OCT−/OCT+);
- funziona sia in play sia in stop;
- una registrazione in loop si annulla in un solo passo;
- popup UNDO/REDO.

Nessun binario, SVG, font o campione dell'OP-1 è incluso. Grafica e font sono originali (`tools/gen_font.py`).
