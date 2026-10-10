# Build per M-VAVE FM1 e sicurezza

> **Nessun flash è stato eseguito e nessuno strumento di questo repository installa qualcosa automaticamente.**
> Il firmware **non è mai stato eseguito su un FM1**. La riuscita dei test nel browser non equivale a una validazione hardware.

## 1. Requisiti (Windows x64 → WSL2 Ubuntu)

La toolchain JieLi esiste solo per Linux x86-64. Su Windows serve WSL2:

```powershell
wsl --install -d Ubuntu-24.04
```

Dentro Ubuntu:

```sh
sudo apt install python3 python3-pil git curl xz-utils
git clone <questo repository> && cd OP-FM-1
firmware/tools/get_toolchain.sh              # scarica clang 4.0.1 pi32v2 in ~/.jieli/toolchain (script di Dinghy)
export JIELI_TOOLCHAIN=~/.jieli/toolchain
python3 firmware/tools/build.py
```

Facoltativo, per il pacchetto `.fwsc`: i tre file dell'SDK AC79 (Apache-2.0) che Dinghy richiede.

```sh
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
export AC79_SDK=~/fw-AC79_AIoT_SDK
python3 firmware/tools/build.py               # ora produce anche build/fm1/tapeos.fwsc
```

## 2. Esito verificato in questa sessione (Linux x86-64)

```
loader: image 8232 B at 0x1c0a800, bss end 0x1c0f400
loader: ota.bin 6954 B
  ok    .ram_text: 901 insns, no calls
  ok    image 54376 B of 581564; RAM .data+.bss 25632 B of 98304; pool 283288 B of 344064
  ok    register access: hal/ only (src/, loader/ clean)
app      build/fm1/tapeos.bin  54376 B
package  not made: needs uboot.boot, cfg_tool.bin, cfg/eq_cfg_hw.bin of the JieLi AC79 SDK (AC79_SDK)
```

| Risorsa | Uso | Limite | % |
|---|---|---|---|
| Immagine app (XIP) | 54 376 B | 581 564 B | 9,3 % |
| RAM `.data` + `.bss` | 25 632 B | 98 304 B | 26 % |
| Pool | 283 288 B | 344 064 B | 82 % |

Il pool contiene: framebuffer 115 200 B, nastro 49 168 B, delay 88 200 B, canvas di Dinghy 30 720 B.

- **Controlli di Dinghy superati:** nessun riferimento alla ROM del chip; registri solo in `hal/`; `.ram_text` senza chiamate; `_start` all'indirizzo giusto.
- **Pacchetto `.fwsc` non prodotto:** gitee.com ha resettato le connessioni da questo ambiente. I file che FM-1-RE estrae dal firmware stock hanno hash diversi da quelli richiesti (eccetto `cfg_tool.bin`), quindi non sono stati usati.

## 3. Prima di qualunque installazione (non eseguita, richiede la tua autorizzazione esplicita)

1. **Backup completo della flash** (1 MiB), con una via che non dipende dal firmware: FM-1-transporter (XIAO RP2040 sulle linee USB, UBOOT della mask-ROM, lettura in ~3 s).
   - Verificare due letture identiche (SHA-256) e conservarle fuori dal dispositivo.
   - Questo passo **non è stato eseguito** e richiede hardware dedicato.
2. **Analisi del layout:** confrontare il dump con il layout atteso.
   - SPL a 0xA0, area app 0x4000, VM 0x94000, USR 0xEA000, settings 0xFC000 (FM-1-RE `architecture.md`).
   - FM1 Tape OS scrive solo nell'area di staging dell'updater e nel settore settings (guardia `FL_RANGE_OK` di Dinghy), e solo tramite l'updater.
3. **Vie di recupero, dalla più alta alla più bassa:**
   - updater di Dinghy dall'app (web installer, FM-1 Firmware Hub);
   - rescue USB di Dinghy (OCT− tenuto all'accensione, oppure automatico dopo due crash al boot);
   - UBOOT della mask-ROM via FM-1-transporter con riscrittura del backup.
   - **Nessuna è stata provata da noi.** FM-1-RE avverte che il recupero da una scrittura interrotta non è dimostrato per il layout a banco singolo.
4. **Rischi:**
   - un'interruzione durante la scrittura dello slot app;
   - un bug che disabilita USB prima del boot guard (mitigato dal rescue e dalla ROM);
   - l'usura della flash (non scriviamo flash a runtime);
   - un carico CPU dell'ISR oltre il budget, che produce audio a scatti o il watchdog (misurato dal meter "CPU" della schermata MIXER).
5. **Primo test sul dispositivo** (solo dopo l'autorizzazione):
   - installazione via web installer o `firmware/tools/fm1_install.py`, script di Dinghy che chiede conferma prima di scrivere;
   - verifiche: schermo, tasti, encoder, audio, carico CPU con 8 voci, ritorno al firmware precedente tramite il Hub.
