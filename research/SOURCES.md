# Fonti, provenienza e licenze

Data di acquisizione: 2026-10-10.

## Firmware OP-1

Questi file **non sono nel repository** (`.gitignore`). Per riscaricarli e verificarne gli hash usare `tools/fetch_firmware.sh`.

| File | URL | Byte | SHA-256 | Last-Modified (server) | Autore |
|---|---|---|---|---|---|
| op1_248.op1 | https://maikopruett.com/firmware/op1_248.op1 | 13 075 330 | `836208af25f2658598e2fe8bf70ca450fc34dc8abeb9f69873a4cc2196018dd5` | non inviato | Maiko Pruett (non ufficiale) |
| op1_247.op1 | https://maikopruett.com/firmware/op1_247.op1 | 13 057 482 | `226e4e287ca5f1834fc6e505fbc1f6ae45026430ee403da39044bc55cea034c0` | non inviato | Maiko Pruett (non ufficiale) |
| op1_246.op1 | https://teenage.engineering/_software/op-1/op1_246.op1 | 13 039 128 | `c5315218f825f143b415ca554516541898abee843d3a236df0b54c04e1fb13a9` | 2022-12-09 08:30:10 GMT | teenage engineering |
| op1_245.op1 | https://teenage.engineering/_software/op-1/op1_245.op1 | 13 027 720 | `d499f6a72dee13c9e7968c83ce3daefd780572020d5aacbda933bb79c7ba36d6` | 2022-09-09 11:58:06 GMT | teenage engineering |

La pagina https://maikopruett.com/op-1 (letta il 2026-10-10) descrive #248 così: "UNDO / REDO popup… name your snapshots… includes #247's tape undo / redo… based on te's #246", data 2026.10.09. Precisa inoltre che si tratta di una build non ufficiale, non affiliata a teenage engineering.

I file #245 e #246 provengono dal sito di teenage engineering. Il firmware resta materiale proprietario di teenage engineering. Su #247 e #248 Maiko non indica alcuna licenza. In questo repository si pubblicano soltanto hash, indirizzi, istruzioni decodificate di poche parole nei punti di patch, nomi di moduli ricavati dalle stringhe di assert e strumenti scritti da noi. Non si pubblicano binari, immagini di memoria o risorse.

## Repository consultati

| Repository | Commit esaminato | Data commit | Licenza |
|---|---|---|---|
| op1hacks/docs | 05e13a28859b | 2022-09-06 | nessun file LICENSE |
| op1hacks/op1repacker | 390b18e4193a | 2026-05-05 | MIT |
| op1emu/op1emu | 5ad01e5acb65 | 2026-10-10 | MIT |
| ↳ submodule op1emu/bcore | b7ff3c8a1a80 | – | MIT |
| ↳ submodule op1emu/bfin_sim | 08e915326ae7 | – | derivato da GDB sim (GPL-3.0, secondo l'intestazione dei file) |
| AlbertSmit/TOP-1 | e641d273fb8d | 2017-12-25 | MPL-2.0 |
| hugelton/Felucca | 129a4cf4e98e | 2026-10-09 | GPL-3.0 |
| zvenson/dinghy | 929730019a9d | 2026-10-09 | GPL-3.0 |
| isod89/sloop-fm1 | fa9ce5742f4a | 2026-10-08 | GPL-3.0 |
| zvenson/dxsloop | 17e395b878f0 | 2026-10-09 | GPL-3.0 |
| ip2k/lunar-modulator | 40c94300428a | 2026-10-10 | MIT |
| AL-255/FM-1-RE | ec832f281ca1 | 2026-09-08 | WTFPL |
| kurogedelic/FM-1-transporter | a632d923203 | 2026-10-01 | MIT |

## Toolchain

- **JieLi Linux toolchain** `jieli-linux-toolchains-20260730.1`, scaricata da `https://pkgman.jieliapp.com/s/linux-toolchain` come fa `tools/get_toolchain.sh` di Dinghy. `pi32v2/bin/clang --version` riporta `clang version 4.0.1`. È installata ma non ancora usata per compilare nulla.
- **LLVM 15** (pacchetto Ubuntu `llvm-15-dev 1:15.0.7`). Serve per compilare bcore.

## Note legali (non sono un parere)

- La ricerca sull'interoperabilità e la decompilazione per scopi di interoperabilità sono regolate da norme nazionali. Nell'UE il riferimento è l'art. 6 della Direttiva 2009/24/CE: la decompilazione è ammessa a condizioni precise, tra cui il fatto di essere indispensabile e la non ridistribuzione. Il carattere personale o scientifico della ricerca non è un'autorizzazione automatica.
- Di conseguenza non sono stati ridistribuiti né ricavati materiali crittografici. Il codice cifrato in L1 non è stato decifrato: vedi `OP1_BINARY_ANALYSIS.md` §4.
- Un futuro firmware basato su Dinghy o Felucca sarebbe soggetto alla GPL-3.0. Codice OP-1 originale tradotto o incorporato al suo interno non sarebbe ridistribuibile senza il consenso del titolare dei diritti.
