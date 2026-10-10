# Strategie di porting: stato delle prove

Riepilogo onesto: **nessuna delle tre strategie è stata ancora sperimentata fino in fondo.** Nessuna porzione del firmware OP-1 gira su pi32v2, né tradotta né emulata. Questo documento raccoglie i fatti verificati che vincolano ciascuna strategia, i limiti misurati e l'esperimento minimo previsto, con i suoi criteri di successo. Per gli esiti dei test vedi `TEST_RESULTS.md`.

## Vincoli comuni (verificati)

1. **ISA incompatibile.** Il codice Blackfin non è codice pi32v2: stessa sintassi, codifiche diverse (`FM1_HARDWARE_LIMITS.md` §3).
2. **Codice analizzabile.**
   - ~1,03 MB di codice SDRAM è in chiaro e disassemblabile.
   - I 49 076 B di codice L1 e i 31 488 B di dati L1 cifrati non sono leggibili senza l'OTP del dispositivo (`OP1_BINARY_ANALYSIS.md` §4).
   - 673 funzioni SDRAM fanno riferimento a L1: anche dove il codice è leggibile, i dati L1 che usa possono essere inizializzati da blocchi cifrati.
3. **Memoria.**
   - Il firmware intero riserva ~23 MB di SDRAM; l'FM1 ha ~432 KiB di RAM dati.
   - L'immagine inizializzata (2,08 MB) è 3,6 volte lo slot XIP (581 564 B).
   - **L'esecuzione dell'intero firmware OP-1 sull'FM1 non modificato è esclusa per capacità**, con qualunque strategia. Si può discutere solo di porzioni.

## Strategia 1: emulazione della CPU Blackfin sull'FM1

**Fatti verificati**
- `op1emu` usa **bcore**, un JIT Blackfin → LLVM IR → codice host (LLVM ORC/LLJIT). Compila ed esegue su x86-64 (`TEST_RESULTS.md` T6, T7).
- bcore richiede LLVM 15 a runtime. Non esiste un backend pi32v2 in LLVM upstream, e un JIT LLVM pesa diversi MB. **bcore non è eseguibile sull'FM1.** È una conclusione architetturale; non è stato tentato di compilarlo per pi32v2.
- op1emu include anche `bfin_sim`, l'interprete di GDB sim in C, usato prima dell'introduzione di bcore (commit `9632059 Replace bfin_sim interpreter with bcore JIT emulator`). È l'unico candidato realistico per un interprete su pi32v2. **Non è stato ancora compilato per pi32v2:** dimensione, dipendenze dal framework sim di GDB e licenza GPL-3 sono da valutare.

**Costo atteso.** Un interprete paga tipicamente decine di istruzioni host per ogni istruzione guest. È una stima generica, non misurata su questa combinazione. Su 240 MHz pi32v2 contro i 400 MHz assunti da op1emu per il BF524, l'emulazione di codice DSP in tempo reale appare improbabile, ma **va misurata**.

**Esperimento minimo previsto**
1. Programma Blackfin di test con risultato noto, poi la funzione OP-1 `0x01A53F08` con stato sintetico.
2. Interprete su host: misura di istruzioni guest al secondo.
3. Compilazione dello stesso interprete per pi32v2: misura di `.text` e `.bss`.
4. Stima del throughput su FM1 dal rapporto tra cicli, da validare sul dispositivo solo dopo autorizzazione esplicita.

**Valutazione attuale.** Fattibile solo per frammenti piccoli e non real-time. Per l'audio è sconsigliata finché non ci sono misure.

## Strategia 2: traduzione binaria statica / lifting

**Fatti verificati**
- bcore contiene già un **lifter Blackfin → LLVM IR** completo per blocco base (`lift_visitor.cpp`, ~7 800 righe) con il supporto runtime a CEC, hwloop e memoria. È lo strumento di lifting Blackfin più completo trovato.
- La toolchain del target è JieLi `clang 4.0.1` (LLVM 4). L'IR generato da LLVM 15 usa i puntatori opachi, che LLVM 4 non conosce. **L'IR di bcore non è quindi consumabile direttamente da clang pi32v2.** È un'incompatibilità di versione nota; il passaggio non è stato provato qui.
- Ghidra (P-code) non è stato provato: il supporto Blackfin non è verificato.

**Percorsi possibili, ordinati per realismo**
1. **Traduttore Blackfin → C per singola funzione.** Ogni istruzione diventa C portabile; i registri sono variabili locali; la memoria guest passa per macro di accesso. Il C risultante si compila per x86 (test differenziali contro bcore che esegue i byte originali) e per pi32v2 (clang 4). Va scritto un generatore per il sottoinsieme di istruzioni usato dalle funzioni scelte.
   - È il percorso **più promettente per l'obiettivo immediato** (una funzione autentica tradotta e compilata per FM1).
   - Il primo tentativo in questa sessione **non è stato completato**: il file è stato scartato e non è nel repository.
2. IR di bcore → C con un backend C per LLVM 15 (llvm-cbe), poi C → pi32v2. Il backend non è disponibile in questo ambiente; da valutare.
3. IR di bcore → downgrade verso il formato LLVM 4: non esiste uno strumento affidabile, percorso sconsigliato.

**Punti tecnici da gestire**, emersi dal disassemblato della funzione candidata `0x01A36490`:
- accumulatori a 40 bit con modalità di moltiplicazione `FU`, `IS`, `(M, IS)` e saturazione per modalità;
- `PACK`, `MAX` e shift aritmetici e logici su accumulatore;
- pacchetti paralleli a 64 bit (letture prima delle scritture);
- loop hardware `lsetup` con `LC0`/`LT0`/`LB0` dinamici, perché più `lsetup` nella stessa funzione condividono LC0, come in `0x01A53F08`;
- epilogo VisualDSP `P0 = [FP+4]; unlink; jump (P0)` da trattare come ritorno;
- argomenti oltre il terzo passati sullo stack (`[SP + 0x24]` dopo il push).
- Su FM1 gli indirizzi guest a 32 bit possono coincidere con quelli host (puntatori a 32 bit). Su x86-64 serve una base.
- Mancanza di `int64_t` per la divisione: i MAC usano moltiplicazioni a 64 bit, che sono consentite.

**Criteri di successo dell'esperimento**
1. Output **identici bit per bit** tra bcore (byte originali) e C tradotto, su input deterministici e casuali, confrontando registri e memoria.
2. Compilazione pulita per pi32v2 con misura di `.text`.
3. Nessuna esecuzione sull'FM1 senza autorizzazione.

## Strategia 3: porting nativo selettivo

**Candidati con prove già raccolte**
- **Logica `clips` del nastro** (`db2/Clips.cpp`, 0x0195CCEC…0x0195E4A4): manipolazione di intervalli `(ch, start, stop)` con insert, split, delete ed edit. Dipende da SQLite: il porting richiederebbe di tradurre la logica sostituendo SQLite con un adapter, cioè codice ADAPTED.
- **Meccanismo di undo di Maiko**: è documentato a livello di hook (`OP1_246_VS_248.md`), ma è codice di Maiko e non di TE.
- **Foglie DSP/tape** (`0x01A36490`, `0x01A53F08`, `0x01A22F3C`): senza chiamate e senza MMR. Sono i primi candidati sia per la traduzione sia per il recupero.

**Limite.** I motori di sintesi con buona probabilità hanno il loop per campione in L1 cifrata (ipotesi, vedi FUNCTION_MAP §3). Il loro recupero fedele non è possibile senza l'OTP del dispositivo. Una reimplementazione sarebbe REIMPLEMENTED e non ORIGINAL.

## Simulatore web (fase H): cosa è stato studiato

- **Felucca** (`BUILDING.md`, `web/emu/`) compila **gli stessi sorgenti C del firmware** contro un FM1 simulato, con Emscripten → WebAssembly. È l'approccio B della specifica: un core C condiviso tra WASM e FM1. È adatto a ospitare una funzione OP-1 tradotta in C, quando esisterà.
- **op1emu in WASM**: bcore dipende da un JIT LLVM che genera codice nativo a runtime. In WebAssembly non è possibile generare ed eseguire codice nativo così; servirebbe l'interprete (`bfin_sim`) o un JIT verso WASM. Non è stato provato.

Nessun simulatore web è stato costruito, perché non esiste ancora un backend autentico da eseguire.
