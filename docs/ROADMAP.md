# Roadmap

Ordinata per priorità e dipendenze. ✔ = fatto e testato (host/browser); ☐ = da fare.

## M0–M3 (questa iterazione)

- ✔ Ricerca, decisione (Dinghy + ANALOG di Felucca + app nostra), build host/web/FM1.
- ✔ SYNTH (pagine principale, ENV, LFO, FX), DRUM, TAPE, MIXER; 4 parametri colorati; grafiche legate al DSP.
- ✔ EventTape a 4 tracce: rec, overdub, loop, erase, undo/redo a 7 livelli con popup.
- ✔ Simulatore web: wasm in AudioWorklet, pannello FM1, tastiera, Web MIDI (non testato con hardware).
- ✔ Build pi32v2 linkata con i controlli di Dinghy (senza `.fwsc`).

## M4: integrazione (priorità alta)

1. ☐ **Validazione su FM1 (bloccante, richiede la tua autorizzazione)**
   - Prerequisiti: backup (FM-1-transporter) e pacchetto `.fwsc` con l'SDK AC79.
   - Verifiche: boot, LCD (bande, byte swap), tasti/encoder, audio, misura CPU (MIXER).
   - Se la CPU con 8 voci supera ~60 %, ridurre a 6 voci o a un blocco di controllo più lungo.
2. ☐ **Persistenza**
   - Settings Dinghy (256 B) per l'ultimo suono e il mixer.
   - Una regione dedicata per il nastro (≤ 48 KiB), salvata solo a strumento fermo, con CRC, doppio slot e recupero. Il nastro di 1 024 eventi per traccia sta in 12 settori.
3. ☐ **Preset utente e progetti**
   - Snapshot di suono, mixer e nastro.
   - Naming in stile #248 (12 caratteri, schermata "reel"), senza asset OP-1.
4. ☐ **Metronomo e count-in**, sincronizzati al clock del nastro.
5. ☐ **Punch-in/out** temporizzato (in/out a battute), annullabile come una take.

## M5: suono

6. ☐ **Altri motori Felucca**, importati con il loro voice code: FM6 (msfa, Apache-2.0 → GPL-3.0), PHYS, NOISE, DRUM di Felucca al posto della nostra drum synth (o come kit aggiuntivo). Prima va misurato il costo per voce sull'FM1.
7. ☐ **MONO/legato/glide** dei preset ANALOG.
8. ☐ **LFO per voce o traccia** e una modulation matrix minima.
9. ☐ **Effetti per traccia** (insert) e un secondo effetto master: reverb da Felucca `fx.c`.

## M6: workflow OP-1

10. ☐ **Editing dei clip sul nastro:** selezione, taglio, copia, spostamento, quantize. Ogni modifica diventa un'operazione annullabile.
11. ☐ **Sequencer** (pattern/endless) che scrive sul nastro o suona dal vivo.
12. ☐ **Arpeggiatore** (tasto ARP).
13. ☐ **Bounce di tracce** (eventi → traccia singola), per liberare voci.

## M7: ricerca AudioTape (dopo le misure su hardware)

14. ☐ Misurare il costo di un encoder IMA ADPCM nell'ISR.
15. ☐ Liberare il pool: framebuffer intero → rendering a bande (−115 KB), delay più corto.
16. ☐ Prototipo "loop audio" mono a 22 kHz di ≤ 20 s, **dichiarato come AudioTape limitato** e distinto dall'EventTape.
    - Non è un nastro OP-1: 4 tracce × 6 minuti richiederebbero 127 MB.

## Qualità

- ☐ Golden visivi (immagini di riferimento) con tolleranza documentata.
- ☐ Test su Chrome/Edge per Windows (CI con Playwright).
- ☐ Fuzz del parser dei comandi e della cronologia (sequenze casuali di rec/undo/redo/erase con invarianti).
- ☐ Analisi statica dei budget di stack (pi32v2).
