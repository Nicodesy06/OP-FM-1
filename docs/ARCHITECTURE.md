# Architettura di FM1 Tape OS

## 1. Diagramma

```
                 ┌────────────────────────── src/ (C condiviso, unity build: src/tapeos.c) ─────────────────────────┐
  FM1 pannello   │  core/panel.c          core/engine.c             audio/        tape/tape.c        ui/             │
  (Dinghy scan)──┼─►tos_panel() ─► comandi ─► ring SPSC ─► cmd_apply() ─► synth.c ◄── tape_tick()     screens.c        │
  browser        │  tos_midi()   (main loop)   (128 voci)   (lato audio)   drum.c        (eventi)     fb.c ─► fb[240x240]│
  (app.js)  ─────┤                                                        mixer.c ─► out (Q15)          ▲              │
                 │  tos_ui(ms) ── 25–30 fps ─► render_frame() ────────────────────────────────────────┘ (legge lo   │
                 │                                                                                      stato audio)  │
                 └───────────────────────────────────────────────────────────────────────────────────────────────────┘
        ▲ tos_audio(out, n)                ▲ tos_fb()                                   ▲ tos_leds_*()
  ┌─────┴──────────────┬─────────────────┴──────────────┬───────────────────────────────┴─────────┐
  │ firmware/src/fm1_tapeos.c        │ platform/web/web.c (wasm32)     │ platform/host/host.h          │
  │ ISR audio Dinghy (256 frame)     │ AudioWorklet (128 frame)        │ test: blocchi di 256 frame    │
  │ LCD: righe cambiate, byte swap   │ framebuffer → canvas 240x240    │ PPM, WAV, hash                │
  │ LED tasti/pulsanti               │ LED → classi CSS                │                               │
  └──────────────────────────────────┴─────────────────────────────────┴───────────────────────────────┘
```

**Un solo codice applicativo.** Le tre piattaforme compilano lo stesso `src/tapeos.c`. Differiscono solo:
- l'adattatore (I/O fisico);
- `TOS_POOL` (sezione `.pool` sull'FM1);
- `TOS_FRAME_MS` (40 ms sull'FM1, 33 ms altrove).

## 2. Flusso degli input

1. Il controllo fisico genera un evento `tos_panel(kind, id, value)`. Gli id sono quelli di Dinghy (`BTN_*`, `ENC_*`, tasti 0–26).
   - **FM1:** `fm1_input_edges`, `fm1_in.notes`, `fm1_input_note_edges`, `fm1_enc_take`.
   - **Browser:** `app.js` → `postMessage` → `worklet.js`. Ogni pressione dura almeno 20 ms di tempo dispositivo, così anche un tap brevissimo viene registrato.
2. `core/panel.c` (main loop) traduce l'evento fisico in azione: cambio schermata, combinazioni, ottava, nota o hit di batteria, parametro. Il risultato è un comando `cmd_push()`.
3. Il lato audio svuota la coda all'inizio di ogni blocco di controllo (32 frame) ed esegue `cmd_apply()`. **Synth, tape e mixer sono modificati solo qui** (un solo scrittore per lato). Il main loop legge lo stato per disegnare.

### Mappatura dei controlli FM1 → funzioni (ispirate all'OP-1)

| Controllo FM1 | Funzione | Equivalente OP-1 |
|---|---|---|
| HOME | schermata SYNTH | tasto synth |
| SEL | schermata DRUM (la tastiera suona la batteria) | tasto drum |
| SEQ | schermata TAPE | tasto tape |
| GLO | schermata MIXER | tasto mixer |
| ENV / LFO / FX | pagine del synth: inviluppo, LFO, effetto | pagine envelope/LFO/FX |
| KNOB1–4 | i quattro parametri colorati (blu, verde, bianco, arancio) | i 4 encoder colorati |
| EDIT (tenuto) | seconda riga di parametri (synth: MIX/NOISE/KEYTRK/FILTER; mixer: PAN) | shift |
| PLAY | play / stop del nastro | play / stop |
| REC | registra sulla traccia selezionata (da stop: arma e parte) | rec |
| **SEQ + OCT− / SEQ + OCT+** | **UNDO / REDO** (popup) | #248: TAPE + ← / → |
| SEQ + PLAY | torna all'inizio | – |
| SAVE + REC | cancella la traccia (annullabile) | – |
| GLO + F3/G3/A3/B3 | mute dei canali 1–4 | – |
| OCT− / OCT+ | ottava (−2..+2) | shift ottava |
| SELECT | pagina successiva del synth / traccia (TAPE) / master (MIXER) | – |
| ALGORITHM | WAVE (synth) / suono da editare (DRUM) | – |
| PRESETS | preset ANALOG / posizione a battute (TAPE) / master (MIXER) | – |
| MASTER | volume (platform Dinghy; nel browser: guadagno) | volume |
| tasti F3–G5 | note (27 tasti) / batteria: i tasti bianchi suonano gli 8 suoni a ciclo, i neri lo stesso suono accentato | tastiera |

**Conflitti verificati:** SEQ, GLO e SAVE sono usati come modificatori solo con OCT, PLAY, REC e i tasti F3–B3. Nessuna di queste combinazioni ha un'altra funzione nella nostra app. ARP non è usato.

## 3. Flusso audio

Il lato audio lavora a blocchi di controllo **CTL = 32 frame** (0,726 ms):

```
cmd_apply()* → tape_tick() → synth_block() → drum_block() → mix_block()
              (eventi del     (ADSR/LFO nostri +        (8 suoni      (4 canali vol/pan/mute,
               nastro a        ENG_ANALOG.render di       nostri)        delay ping-pong,
               questo tick)    Felucca per voce)                         master + soft knee,
                                     │                                   meter, scope)
                              bus[traccia][32] (mono, int32)
```

- `tos_audio(out, n)` accetta qualunque `n`: FM1 256, browser 128, test 256. Conserva il blocco parziale in `part[]`.
- **8 voci** synth condivise fra tastiera e nastro. Ogni voce ha la propria `sound_t`: il nastro suona ogni traccia con il suono con cui è stata registrata (`trk_snd`).
- **Allocazione:** voce libera, altrimenti la più vecchia in release, altrimenti la più vecchia.
- **Integer only:** l'unico `float` è nella conversione finale verso Web Audio (`platform/web/web.c`).
- Le moltiplicazioni a 64 bit sono usate in ADSR e UI. La divisione 64/32 compilata da clang pi32v2 è un'istruzione nativa (verificato nel disassemblato); non usiamo divisioni 64/64.

## 4. Renderer

- **Framebuffer:** 240×240 RGB565 (`fb[]`, 115 200 B, nel pool sull'FM1). Ridisegnato interamente a 25 fps (FM1) o 30 fps (browser), solo dal main loop.
- **Primitive** (`ui/fb.c`): rect, line, circle, testo 5×7 scalabile, con clipping. Il font è originale (`tools/gen_font.py`); i colori dei parametri sono blu/verde/bianco/arancio.
- **Le grafiche dipendono dallo stato reale:**
  - SYNTH: forma d'onda calcolata con gli oscillatori di Felucca (`osc_saw/pulse/tri/sine`) secondo WAVE, MIX e DETUNE; risposta del filtro con il cutoff **modulato in quel momento** (inviluppo della voce più recente + LFO); scope dell'audio reale del canale selezionato; pallini delle voci con raggio pari all'inviluppo.
  - ENV: punto che segue l'inviluppo della voce più recente.
  - LFO: punto alla fase reale.
  - FX: echi con spaziatura TIME e decadimento FEEDBACK; meter master.
  - DRUM: pad illuminati dal livello della voce.
  - TAPE: bobine che ruotano con la posizione, eventi delle 4 tracce, playhead, livelli undo/redo, popup.
  - MIXER: fader, pan, meter di picco reali, mute; carico CPU sull'FM1.
- **FM1:** `push_screen()` calcola un hash FNV per riga e invia solo le bande di 8 righe cambiate, con byte swap per l'ST7789 e due buffer alternati (`lcd_sync` prima di riusarli).
- **Browser:** il worklet invia il framebuffer (transfer) quando cambia, al massimo ogni 30 ms di tempo dispositivo; `app.js` lo converte in RGBA sul canvas. **Stessi pixel** dell'FM1, senza una seconda UI.

## 5. Tape Engine

### Modello

- **4 tracce**, fino a 1 024 eventi ciascuna.
- Evento `tev_t` = {tick, durata, nota, velocity (bit 7: hit di batteria), add, del}: **12 byte**. Un hit di batteria ha durata 1 tick.
- **Clock:** 1 tick = 32 campioni = 0,726 ms, avanzato da `tape_tick()` nel lato audio. **Non** usa il frame rate grafico (test `tape clock = audio clock`).
- **Loop:** BARS × 4 × 60/BPM secondi (1 battuta a 120 = 2 756 tick). Senza loop il nastro dura fino a 6 minuti, poi si ferma.
- **Riproduzione:** un cursore per traccia sul primo evento con tick ≥ posizione (gli eventi sono tenuti ordinati). Le note-off sono pianificate per tick, con wrap del loop.

### Stati del transport

| Stato | PLAY | REC | UNDO / REDO |
|---|---|---|---|
| STOP, disarmato | → PLAY | arma e parte (registra) | annulla/ripristina; nessun suono in corso |
| PLAY | → STOP (chiude la take) | arma: registra da qui | annulla/ripristina subito; le note del nastro in corso vengono spente |
| PLAY + REC (registrazione) | → STOP, chiude le note tenute, fine take | disarma (fine take), continua a suonare | chiude la take in corso, poi annulla |
| loop attivo | il wrap ricomincia da 0 | la registrazione continua attraverso il wrap: **una sola take**; le note tenute sul confine hanno la durata corretta | – |

**Overdub:** registrare su una traccia che ha già eventi li unisce. La take precedente continua a suonare durante la registrazione.

### Undo / redo (7 livelli)

- Ogni operazione (take o erase) ha un numero di serie. Gli eventi portano `add` (chi li ha creati) e `del` (chi li ha cancellati).
- La visibilità è `add ≤ cur && !(del && del ≤ cur)`:
  - **undo** = `cur−1`, **redo** = `cur+1`;
  - **una nuova operazione dopo un undo** rimuove fisicamente ciò che è stato annullato (non c'è più redo);
  - **oltre 7 livelli** le operazioni diventano permanenti e gli eventi cancellati vengono eliminati.
- **Costo:** 4 byte per evento, qualunque sia il numero di livelli. Al massimo 4 096 eventi, cioè 16 KiB dei 48 KiB di memoria del nastro. Nessuna copia del progetto (test `storage: 9 events, no copies`).
- I serial sono rinumerati prima di 60 000 operazioni.

### EventTape vs AudioTape

`tape.c` registra **eventi**, non audio. Non è il nastro PCM dell'OP-1 e non viene presentato come tale. Transport e cronologia sono separati dalla memoria degli eventi, così un backend AudioTape potrebbe affiancarlo. Fattibilità:

| Formato (1 traccia) | B/s | Durata nei ~60 KB di pool liberi oggi | in ~250 KB (senza delay, framebuffer a bande) |
|---|---|---|---|
| PCM 16 bit stereo 44,1 kHz | 176 400 | 0,3 s | 1,4 s |
| PCM 16 bit mono 44,1 kHz | 88 200 | 0,7 s | 2,8 s |
| PCM 16 bit mono 22,05 kHz | 44 100 | 1,4 s | 5,7 s |
| IMA ADPCM 4 bit mono 22,05 kHz | 11 025 | 5,5 s | 22,7 s |
| OP-1: 4 tracce × 6 min, 16 bit mono 44,1 kHz | 4 × 88 200 | servono 127 MB | – |

- **Flash:** 1 MiB in tutto. Una cancellazione blocca audio e interrupt per decine di ms (Dinghy `RULES.md`), quindi lo streaming su flash in tempo reale **non è praticabile**.
- **USB:** l'FM1 è un dispositivo UAC solo in uscita verso il computer; registrare è possibile nella DAW, non sul dispositivo.
- **Memoria esterna:** nessuna documentata.

**Conclusione:** un AudioTape a 4 tracce come quello dell'OP-1 **non è realizzabile sull'FM1 non modificato**. Un'ipotesi realistica è un "loop audio" ADPCM corto (≤ 20 s mono a 22 kHz), da valutare solo dopo aver misurato il costo CPU dell'encoder nell'ISR. Vedi ROADMAP.

## 6. Storage

Nella prima iterazione **non c'è persistenza**: nastro, suoni e mixer si perdono allo spegnimento.
- Dinghy offre `settings_load/save` per 256 byte in un settore.
- Salvare il nastro (fino a 48 KiB) richiede di allargare `FL_RANGE_OK` a una regione dedicata e di salvare a strumento fermo, perché la cancellazione ferma l'audio. In roadmap, con una procedura di recupero.

## 7. Differenze fra web e hardware

| Aspetto | Browser | FM1 |
|---|---|---|
| Codice applicativo | identico (`src/tapeos.c`) | identico |
| Blocco audio | 128 frame (render quantum) | 256 frame (ISR, 5,8 ms) |
| Clock del main loop | ogni ms di tempo dispositivo (guidato dall'audio) | `fm1_ms` (TIMER5) |
| Frame rate | 30 fps | 25 fps, solo le righe cambiate |
| Uscita | float, GainNode per MASTER | DAC via platform Dinghy (MASTER = ADC) |
| MIDI | Web MIDI (facoltativo) → `tos_midi` | USB-MIDI (`midi_get`) → `tos_midi` |
| Misura CPU | solo host (test) | `fm1_ticks` nell'ISR → MIXER "CPU xx%" (non ancora osservata) |
| USB audio, rescue, update | assenti | dal platform Dinghy |
| Timing reale | non dimostra quello dell'FM1 | da misurare sul dispositivo |
