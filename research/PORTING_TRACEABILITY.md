# Tracciabilità del porting

Ogni componente portato dovrà avere qui una scheda che lo colleghi al codice originale. Le categorie sono quelle della specifica: ORIGINAL, TRANSLATED, LIFTED, ADAPTED, REIMPLEMENTED, UNSUPPORTED.

## Stato al 2026-10-10

**Nessun componente portato.** Non c'è codice OP-1 eseguito, tradotto o adattato su FM1 o su host, tranne il bootloader te-boot eseguito in op1emu (vedi sotto). Non esiste nessuna scheda di porting verificata.

| Sottosistema | Categoria attuale | Note |
|---|---|---|
| Sistema operativo OP-1 (`OP1_vdk.ldr`) | UNSUPPORTED | non avviato nemmeno su host (vedi `TEST_RESULTS.md` T7) |
| te-boot v2.30 | ORIGINAL (solo host, op1emu) | raggiunge il menu di servizio; non fa parte del sistema da portare |
| UI, audio, tape, sequencer | UNSUPPORTED | – |

## Componenti selezionati (in attesa di esperimento)

| Campo | Candidato 1 | Candidato 2 |
|---|---|---|
| Sottosistema | DSP / SampleBuffer (ruolo ipotizzato) | Tape (ruolo ipotizzato) |
| Firmware di origine | #246 = #248 (byte identici in questa zona) | #246 = #248 |
| Indirizzo originale | 0x01A36490 | 0x01A53F08 |
| Dimensione raggiunta | 296 B, 115 istruzioni | 116 B, 45 istruzioni |
| Dipendenze | nessuna chiamata, nessun MMR, nessuna L1 | legge il globale 0x01B4EF3C (BSS) |
| Metodo previsto | TRANSLATED (Blackfin → C) | TRANSLATED |
| Codice di destinazione | – | – |
| Test | – | – |
| Risultato | non eseguito | non eseguito |

## Report di copertura

**Denominatore:** 2 078 244 byte, i byte inizializzati in SDRAM di `OP1_vdk.ldr` #246, cioè i blocchi non-FILL e non-CALLBACK. Le altre porzioni sono riportate a parte con il proprio denominatore.

| Misura | Byte | % del denominatore |
|---|---|---|
| Codice raggiunto dal recupero funzioni (unione istruzioni) | 1 029 692 | 49,5 % |
| di cui in funzioni attribuite a un file sorgente via assert | 430 010 | 20,7 % |
| di cui in funzioni attribuite solo per vicinanza | 115 838 | 5,6 % |
| Eseguito in emulazione (OS OP-1) | 0 | 0 % |
| Tradotto | 0 | 0 % |
| Adattato | 0 | 0 % |
| Sconosciuto (non raggiunto dall'analisi) | 1 048 552 | 50,5 % |

Byte nei blocchi cifrati L1 (`CALLBACK`): 80 564, ovvero 49 076 di istruzioni e 31 488 di dati. Sono **0 % leggibili** come blocchi interi; 2/3 dei byte sono in chiaro ma frammentati.
