# Test report: FM1 Tape OS, prima iterazione

Ambiente: Linux x86-64 (Intel Xeon 2,1 GHz, 4 core), GCC di sistema, clang 18 (wasm32), Node 22, Chromium (Playwright 1.56), JieLi clang 4.0.1 pi32v2.

Per rilanciare tutto: `WEB=1 FM1=1 JIELI_TOOLCHAIN=~/.jieli/toolchain tests/run_tests.sh`. Esito: **ALL TESTS PASSED**.

## Riepilogo

| Suite | Cosa verifica | Esito |
|---|---|---|
| `tests/test_tape.c` | registrazione, riproduzione, overdub, undo/redo, limite di 7 livelli, invalidazione del redo, registrazione in loop, erase, batteria sul nastro, tracce, clock | **45/45** |
| `tests/test_audio.c` | nota on/off, efficacia di CUTOFF, 7 forme d'onda, 8 suoni di batteria, mixer/mute/meter, golden hash dei WAV, costo CPU host | **31/31** |
| `tests/test_ui.c` | testi che entrano negli spazi, grafiche che seguono WAVE, CUTOFF, note, pad, undo; 9 schermate 240×240 salvate | **103/103** |
| smoke wasm (Node) | `tapeos.wasm`: init, nota, audio non nullo, clock 580 ms, frame | **ok** |
| `tests/web_test.cjs` (Chromium) | accensione, wasm nel worklet, nota dalla tastiera del computer → audio reale, KNOB1, 4 modalità, REC/Ctrl+Z/Ctrl+Y, pulsanti a schermo SEQ+OCT− | **17/17** |
| build FM1 (`firmware/tools/build.py`) | compilazione e link pi32v2, controlli di sicurezza Dinghy, RAM/flash | **ok** (pacchetto non prodotto: manca l'SDK) |

## Dettagli rilevanti

### Tape e undo/redo (test_tape)

- **Timing.** Una nota tenuta 200 ms dura 270–285 tick, attesi 275,6. L'inizio cade entro 8 tick (≤ 1 blocco audio) dalla pressione del tasto.
- **Clock.** Il nastro avanza esattamente di 1 tick ogni 32 frame audio (`tape clock = audio clock`), indipendentemente dal frame rate grafico.
- **Overdub.** Si annulla esattamente: restano le note della take precedente.
- **Redo.** Dopo un undo, una nuova take elimina il redo e la take annullata non torna più.
- **7 livelli.** Dopo 9 take, 7 undo lasciano le 2 più vecchie e l'8° mostra NO UNDO. 7 redo ripristinano tutto, senza copie: 9 eventi in memoria.
- **Loop.** Una nota tenuta oltre la fine del loop (400 ms) ha durata 540–560 tick (attesi 551). Più passaggi di loop formano una sola take e si annullano in un passo.
- **Undo da fermo** funziona; il popup UNDO/REDO/NO UNDO/NO REDO viene mostrato.

### Audio (test_audio)

- **Livelli.** Una voce SAW LEAD ha picco 9 840 (−10,4 dBFS), senza clipping; RMS dopo il rilascio < 20.
- **CUTOFF.** Abbassarlo di 70 passi riduce l'energia alle alte frequenze di oltre 2 volte.
- **Golden hash** (`tests/golden_audio.txt`): 9 render (nota, 7 forme d'onda, batteria). Si rigenerano con `GOLDEN_UPDATE=1` solo per un cambiamento voluto del suono, da motivare nel commit.
- **CPU, solo host.** 10 s con 8 voci, 8 suoni di batteria e delay calcolati in 0,049 s, cioè lo 0,5 % del tempo reale su questo Xeon. **Non è una misura dell'FM1.** Sull'FM1 la misura è predisposta (meter CPU nella schermata MIXER, calcolato con `fm1_ticks` attorno all'ISR) ma **non è stata osservata**, perché nessun dispositivo è stato usato.

### UI (test_ui)

- **Dimensioni.** Tutte le etichette e i valori enumerati stanno in 58 px, i nomi di batteria nei pad, i titoli prima di "BAR". Il framebuffer è 240×240 RGB565.
- **Le grafiche cambiano con lo stato reale:** WAVE (forma d'onda), CUTOFF (filtro), note suonate (scope e voci), hit di batteria (pad), undo (tracce del nastro).
- **Screenshot:** `build/test/ui_*.png`, generati dal renderer reale (vedi README).

### Browser (web_test)

- **Audio reale.** Il picco del master misurato **dentro il wasm** supera 500 con un tasto premuto e scende sotto 200 dopo il rilascio. Il suono proviene dal motore C compilato, non da JavaScript.
- **Input.** Gli stessi comandi FM1 funzionano dalla tastiera del computer (Ctrl+Z ⇒ SEQ+OCT−) e dai pulsanti del pannello simulato.

## Errori trovati e corretti durante lo sviluppo

1. **Pulsanti a schermo.** `setPointerCapture` lancia un'eccezione con un puntatore sintetico e bloccava l'evento. Ora l'errore è gestito: può accadere anche su alcuni dispositivi touch.
2. **Meter CPU dell'FM1.** Il calcolo del periodo andava in overflow a 32 bit (256 × 24 × 10⁶ > 2³²). Corretto prima di qualsiasi esecuzione.
3. **Collisioni di nomi con Dinghy** (`text_w`, `RGB`, `C_WHITE`, `FS`): risolte con rinomina e `#undef`; il build pi32v2 è pulito.
4. **Test di rewind.** Tolleranza aumentata: la combinazione SEQ+PLAY dura ~10 ms di tempo simulato.

## Limiti e test non eseguiti

- **Nessun test su hardware:** né installazione, né misura CPU reale, né verifica del pannello o della latenza dell'LCD.
- **Pacchetto `.fwsc`** non generato (SDK AC79 irraggiungibile da gitee in questo ambiente).
- **Persistenza** (salvataggio e caricamento): non implementata, quindi non testata.
- **Test visivi automatici con immagini di riferimento** (pixel golden): non presenti. Le schermate sono verificate con hash di regione e controlli di dimensione.
- **Browser:** testato solo Chromium headless su Linux. Chrome/Edge su Windows: non verificato in questa sessione.
- **Web MIDI:** codice presente, non testato con un dispositivo.

## Funzionalità non ancora implementate

Vedi `docs/ROADMAP.md`. In sintesi:
- persistenza di nastro, suoni e progetti;
- altri motori sonori (FM6, PHYS, sampler);
- arpeggiatore;
- sequencer a step;
- punch-in temporizzato;
- editing dei clip (taglio, spostamento);
- automazioni;
- AudioTape (vedi ARCHITECTURE §5);
- metronomo;
- MONO/legato dei preset (oggi le voci sono sempre polifoniche);
- LFO per traccia del nastro (oggi la fase dell'LFO è unica, condivisa).
