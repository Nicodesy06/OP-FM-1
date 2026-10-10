# OP-1 #248 → M-VAVE FM1: repository di ricerca

Ricerca sperimentale: quanta parte del firmware originale OP-1 (custom OS #248 di Maiko Pruett, derivato da #246 di teenage engineering) può essere eseguita, tradotta o recuperata sull'hardware M-VAVE FM1.

Nel repository ci sono **solo strumenti, documenti e dati derivati** (hash, indirizzi, metriche). Firmware, file estratti e immagini di memoria restano in `private/`, che non è versionato. Provenienza e licenze sono in [`research/SOURCES.md`](research/SOURCES.md).

## Documenti

| Documento | Contenuto |
|---|---|
| [FIRMWARE_EXTRACTION](research/FIRMWARE_EXTRACTION.md) | struttura reale dei pacchetti 245–248 |
| [OP1_BINARY_ANALYSIS](research/OP1_BINARY_ANALYSIS.md) | LDR, memoria, cifratura L1, simboli recuperati |
| [OP1_246_VS_248](research/OP1_246_VS_248.md) | come Maiko ha patchato #246: 17 zone, 11 hook |
| [OP1_FUNCTION_MAP](research/OP1_FUNCTION_MAP.md) | 3 679 funzioni, attribuzione ai sorgenti, funzioni chiave del Tape |
| [FM1_HARDWARE_LIMITS](research/FM1_HARDWARE_LIMITS.md) | risorse FM1, confronto quantitativo, incompatibilità ISA |
| [PORTING_STRATEGIES](research/PORTING_STRATEGIES.md) | stato delle tre strategie |
| [PORTING_TRACEABILITY](research/PORTING_TRACEABILITY.md) | schede di porting e report di copertura |
| [TEST_RESULTS](research/TEST_RESULTS.md) | test T1–T10 con comandi ed esiti |

## Risultati principali (verificati)

1. Il codice SDRAM di `OP1_vdk.ldr` è **in chiaro**: ~1 MB di codice disassemblabile. Sono cifrati con XTEA solo i blocchi L1 (80 564 B), con una chiave per dispositivo programmata in OTP.
2. #248 è una **patch binaria a trampolini** su #246: un nuovo blocco a 0x01E00000 e 17 zone modificate. Tutti gli hook sono identificati tramite i contatori che incrementano.
3. **pi32v2 non è binario-compatibile con Blackfin.** L'immagine OP-1 riserva ~23 MB di SDRAM contro ~432 KiB di RAM dati dell'FM1, e il codice inizializzato è 3,6 volte lo slot applicazione. Il firmware completo non può girare sull'FM1 non modificato.

## Non ancora fatto

- Esecuzione isolata, traduzione e compilazione pi32v2 di una funzione OP-1. Candidati: 0x01A36490, 0x01A53F08.
- Avvio dell'OS OP-1 in op1emu. te-boot raggiunge il menu; l'OS richiede NAND e OTP di un dispositivo reale.
- Qualsiasi attività sull'hardware FM1. Nessuna flash è stata eseguita e nessuna sarà eseguita senza autorizzazione esplicita.

## Strumenti

| Percorso | Funzione |
|---|---|
| `tools/fetch_firmware.sh` | scarica op1_245–248.op1 in `private/fw` e verifica gli SHA-256 |
| `tools/ldr/op1unpack.py` | CRC32, LZMA → TAR, inventario |
| `tools/ldr/ldrimg.py` | parser LDR Blackfin, immagine di memoria flat |
| `tools/ldr/xtea_probe.py` | decifra i blocchi CALLBACK con una chiave *fornita* e ne misura l'entropia (non deriva chiavi) |
| `tools/bfin/` | `bftool` (basato su bcore): disassembla codice originale, chiama una funzione originale |
| `tools/analysis/funcmap.py` | recupero funzioni: call graph, CFG, dipendenze MMR/L1 |
| `tools/analysis/xref.py` | riferimenti alle stringhe, attribuzione ai file sorgente tramite assert |
| `tools/analysis/export_tsv.py` | esporta la mappa in `research/data/` |
