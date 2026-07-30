# Sessione di debug — 28/07/2026 (VKBeta)

Punto di partenza: la beta si avviava ma **crashava aprendo qualsiasi voce di menu**
(1 Player, Multiplayer, VanzaWFC). Il documento precedente (`debug_summary.md`, in Downloads)
attribuiva la causa a `FavouriteCombo` / puntatore Mii NULL: **quella diagnosi era sbagliata**,
vedi la sezione "Ipotesi scartate".

---

## 1. Bug risolti

### 1.1 Il linker Kamek non compilava più (bloccante)

**Sintomo:** ogni build falliva con `IndexOutOfRangeException` in `BranchHook..ctor`.

**Causa:** `KamekLinker/Kamek.exe` è un binario committato che **non viene ricompilato** dal
build script (`KamekLinker/*` è in `.gitignore`). Il sorgente `BranchHook.cs` su disco aveva
già il fix (`args[2]`), ma l'exe era fermo a novembre 2024 con il bug `args[3]`, che va fuori
dai limiti su qualsiasi `kmRegionCall`/`kmRegionBranch`. Il commit `c14508c` ha introdotto
proprio quei costrutti in `kamek.cpp`, quindi da lì in poi **nessuna build era valida** e
Dolphin caricava un `Code.pul` non corrispondente ai sorgenti.

**Fix:** ricompilato il linker con `dotnet build -c Release` e copiato l'exe in `KamekLinker/`.

> Se il linker rilancia un'eccezione .NET, non cercare il bug nel gioco: ricompila il linker.

**Nota sul build script:** `BuildPulsar.bat` copia `build/*.pul` nelle cartelle Riivolution
**prima** di compilare, e mai dopo (la variabile `RIIVO` del blocco finale non è mai impostata).
Il `Code.pul` appena linkato va copiato a mano, altrimenti si testa sempre la build precedente.

### 1.2 `BootHook::Exec` non veniva mai eseguito → `System::sInstance` NULL

**Sintomo:** ogni pagina Pulsar crashava al primo accesso. Si manifestava in punti diversi
(`MiiHeadsModel`, `CorrectButtonCount`, `ExpSection::AddPageLayerAnimatedReturnTopLayer`),
il che faceva sembrare bug distinti.

**Causa:** la beta ha l'integrazione col canale homebrew (commit `03a99e1`), che segue un
percorso di avvio diverso da quello Riivolution della stabile. Su quel percorso la funzione
del REL a **`0x80543B84` (PAL) non viene mai eseguita**, ed è lì dentro che Pulsar aggancia
`BootHook::Exec`. Nessun BootHook partiva, quindi `System` non veniva creato.

**Dimostrato, non dedotto:** agganciando due istruzioni diverse della stessa funzione
(`0x80543BB4`, che ha una rilocazione REL, e `0x80543BB8`, che non ce l'ha) **nessuna delle
due ha mai sparato** → la funzione non è raggiunta. Il log a runtime mostrava
`BootHook::list count=6`, quindi i costruttori globali giravano e gli hook erano registrati:
mancava solo la chiamata.

**Fix:** `BootHook::Exec` resa idempotente (`BootHook::executed` in `kamek.hpp`), più un
innesco di riserva in `ArchiveDecompressSafety.cpp` che la esegue alla prima decompressione
di archivio (`Font.szs`), che il log dimostra avvenire sempre all'avvio. L'hook originale
resta per i percorsi che lo raggiungono; vince chi arriva primo.

**Ordine critico:** l'innesco deve stare **prima** di `ShouldApplyLooseOverrides`, perché
quest'ultima sonda la mods root e ne **memorizza il risultato in modo permanente**; girando
prima che `System` esista si cristallizza un falso negativo.

### 1.3 Crash del modello testa Mii (`option=35`)

**Sintomo:** `nw4r::g3d::ScnObj::SetMtx` con `this == NULL`, raggiunta da
`MiiHeadsModel::InitModel` chiamata a `0x807DC11C`.

**Causa:** il `MiiBody.szs` custom della beta è costruito per **tre outfit Mii (A/B/C)**, ma
`MiiOutfitC.cpp` — che contiene le patch `mulli` che portano il passo degli array da 2 a 3
(`0x80830d50`, `0x80831148`, `0x8083018c`, `0x8059e3bc`) — era **disabilitato**. Archivio
nuovo + codice vecchio = indicizzazione fuori range → modello NULL.

**Dimostrato:** rinominando `Scene/Model/MiiBody.szs` così da usare quello del disco, la
chiamata con `option=35` ha smesso di crashare e ha ritornato un puntatore valido.

**Come è stato isolato:** loggando gli argomenti di `InitModel`. Le chiamate con `option = 3`
riuscivano, quella con `option = 35` (aggiunge `BUFFER_RESMATMISC`) crashava. Tutti i
puntatori (`mii`, `driverModel`) erano **validi e non NULL**.

### 1.4 "My Stuff" non applicava nessun override

**Sintomo:** `sOverrideDatabase.taggedEntries is empty` a ogni archivio; nessun override
applicato, quindi asset vecchi che non corrispondono al codice di `c14508c`.

**Causa:** la patch Riivolution `VKBeta164Load` **non produceva alcun effetto**, nonostante
l'opzione risultasse su "From Pack" nella finestra di Dolphin e gli id combaciassero. Senza
di essa `/My Stuff` non veniva creata nell'FST del disco, quindi `ModsRootExists()` falliva
e il database non veniva mai costruito.

**Dimostrato:** diagnostica che elenca le voci di primo livello dell'FST →
`voci=2995, trovataNelFST=0`, e `/My Stuff` assente mentre `Binaries` ed `Experts`
(patch *Pack*) erano presenti.

**Fix:** spostate le due righe `<folder>` di My Stuff dentro `VKBeta164LoadPack`, la patch
sempre applicata. Risultato: `voci=3116`, `DIR 'My Stuff'`, e
`Loose overrides database built successfully: tagged=7, wholeFile=104`.
`MenuSingle.szs` è passato da 6.749.728 a **8.030.752**, lo stesso valore della stabile.

> Coerente con l'intento del progetto: `AreLooseArchiveOverridesEnabled()` ritorna `true`
> incondizionatamente, quindi My Stuff dev'essere sempre attivo e non dietro un'opzione
> disattivabile.

### 1.5 Patch mancante rispetto a stabile e fork

Aggiunta in `Codes.cpp`:

```cpp
kmWrite32(0x80544928, 0x7C601B78);  // Disable Data Save Reset for Region ID Change [Vega]
```

Presente in rr-pulsar (`Extra/Extra.cpp`) e nella build stabile, assente dalla beta. Senza,
il gioco confronta la region id salvata con quella corrente e, se differiscono, marca il
salvataggio per il reset. Non era la causa del crash, ma è una mancanza reale.

---

## 2. Bug non risolti

### 2.1 Crash creando le pagine del menu Single Player — APERTO

Stato all'ultimo test: `SRR0=0x8063CA74`, `DAR=0x10`, `R3=0`, raggiunto da
`ExpSection::CreateAndInitPage +0x474`. Prima del fix di My Stuff il crash era in
`SettingsPanel::CreateControl +0x134`; ora è più a fondo nel codice di gioco.

**Ipotesi corrente, non ancora testata:** l'XML della beta mappa **tutte** le varianti
linguistiche di `MenuSingle` allo stesso archivio completo:

```xml
<file disc="/Scene/UI/MenuSingle_E.szs" external="/VKBeta/Scene/UI/MenuSingle.szs"/>
<file disc="/Scene/UI/MenuSingle_U.szs" external="/VKBeta/Scene/UI/MenuSingle.szs"/>
<file disc="/Scene/UI/MenuSingle_J.szs" external="/VKBeta/Scene/UI/MenuSingle.szs"/>
<file disc="/Scene/UI/MenuSingle_I.szs" external="/VKBeta/Scene/UI/MenuSingle.szs"/>
```

La stabile **non ha nessuna di queste mappature**. In MKW i `MenuSingle_X.szs` sono piccoli
overlay di lingua (nella stabile `_I` pesa 109.088 byte, l'originale del disco), non copie
dell'archivio principale. Ora che My Stuff sostituisce correttamente `MenuSingle.szs` con la
versione da 8 MB, l'overlay italiano resta una copia del vecchio archivio da 6,7 MB: i due
sono disallineati.

**Prossimo passo:** commentare la sola riga `_I` (l'unica caricata, gioco in italiano) e
verificare. Se risolve, decidere cosa fare con `_E`/`_U`/`_J`.

### 2.2 `Rating/*` e `MiiOutfitC.cpp` non compilano — APERTO

Usano `PULSAR_VR`, definito nel `PulsarSystem.hpp` di `c14508c`, mentre nel working tree c'è
la versione revertata dalla sessione precedente. Finché non si sbroglia questo intreccio:

- `MiiOutfitC.cpp` resta disabilitato (dipende da `GetUserRank`, in `RatingSave.cpp`);
- di conseguenza il `MiiBody.szs` custom **non va riattivato**, o torna il crash 1.3.

### 2.3 Da ripulire prima di una release

- Log diagnostici `[VK ...]` in: `EnhancedReplay.cpp`, `ArchiveDecompressSafety.cpp`,
  `LooseArchiveOverrides.cpp`, `MiscUI.cpp`.
- Hook diagnostico `kmCall(0x807dc11c, LogMiiHeadsInit)` in `EnhancedReplay.cpp`.
- Commento **sbagliato** in `kamek.cpp`: attribuisce la causa del bug 1.2 alla rilocazione di
  `0x80543BB4`, mentre la causa reale è che la funzione non viene eseguita.
- `DebugMenuDriver.cpp` resta `.disabled`: aggancia `0x80831100` credendola
  `SetPlayerCharacter`, che invece sta a `0x80830D00`. Interpretava un **puntatore** come
  `CharacterId` e lo sostituiva con 0, peggiorando le cose.

---

## 3. Ipotesi scartate (non ripercorrerle)

Otto ipotesi cadute su questo secondo filone, tutte smentite da un test:

| # | Ipotesi | Come è caduta |
|---|---|---|
| 1 | `kmBranch(0x805e4228)` salta l'init dei Mii (tesi del `debug_summary`) | `0x805e4228` è un **`blr`**: è un tail hook corretto. Cambiarlo in `kmCall` fa cadere l'esecuzione nel prologo della funzione successiva → blocco al boot |
| 2 | Rilocazione REL che sovrascrive il boot hook | Reale, ma irrilevante: anche `0x80543BB8`, non rilocato, non spara |
| 3 | Override archivi / scelta dell'heap | Con `AreLooseArchiveOverridesEnabled()` a `false` il crash è identico |
| 4 | Database Mii mancante sulla NAND | RR gira sullo stesso Dolphin senza `RFL_DB.dat`. Inoltre l'offset del Mii nella licenza che avevo usato era **inventato**: il Mii sta a `+0x14`, e la licenza conteneva `"Vanza Mii"` |
| 5 | Personaggio Mii outfit C | Log: `favChar = 0xFFFFFFFF`, nessun preferito |
| 6 | Ramo `else` aggiunto in `FavouriteCombo` | Rimosso (allineando a upstream), crash identico |
| 7 | `ExpSinglePlayer.cpp` | Disabilitato per intero, crash identico. È anche identico a rr-pulsar salvo formattazione |
| 8 | Creazione di `SettingsPanel` in `CreatePulPages` | Disabilitata, crash identico |

Altre due smentite:

- **charId spazzatura `0x924A0CDC`** (dal `debug_summary`): non è spazzatura, è un
  **puntatore MEM2 valido**. La diagnostica agganciava la funzione sbagliata.
- **Nome dell'opzione `My Stuff` vs `MyStuff`**: il config di Dolphin era davvero
  disallineato, ma sistemarlo non è bastato — l'opzione risultava già su "From Pack".

---

## 4. Metodi utilizzati

Cosa ha funzionato, in ordine di resa:

1. **Misurare a runtime invece di dedurre.** Ogni volta che ho ragionato per somiglianza ho
   sbagliato; ogni volta che ho messo un `OSReport` ho avuto la risposta al primo colpo.
2. **Confronto con una build funzionante.** La stabile (VanzaKart) come riferimento, e il
   diff a livello di **comandi nel binario** (`Code.pul`), non di sorgenti: ha ridotto le
   differenze a tre voci.
3. **Isolamento per file.** Disabilitare interi file (`ExpSinglePlayer.cpp`) invece di
   cercare la riga colpevole leggendo il codice.
4. **Verificare le assunzioni di base.** Ho scoperto tardi di non aver mai controllato che la
   stabile entrasse davvero in Single Player: otto tentativi poggiavano su un presupposto non
   verificato.

Strumenti costruiti durante la sessione:

- **Risoluzione simboli dai crash dump.** Base del codice Kamek = **`0x803992E0`**
  (l'anchor è la riga `-debug=0x803992E0` in `BuildPulsar.bat`); offset = `SRR0 - base`,
  simbolo cercato in `build/Code.P.map`. Le funzioni `static` non compaiono nella map: si
  identificano dai buchi tra simboli noti.
- **Disassemblatore PPC** per `main.dol` e `StaticR.rel`.
- **Parser della lista comandi di `Code.pul`** (header 0x20 byte, poi code blob, poi comandi;
  `id<<24 | 0xFFFFFE` + indirizzo assoluto).
- **Parser della tabella di rilocazione del REL**, per sapere quali indirizzi OSLink riscrive.
- **Dump dell'FST** a runtime, per vedere cosa Riivolution crea davvero.

Errori di metodo da non ripetere: ho dedotto **due offset del salvataggio** (`0x9C` per il
Mii, `0x92CA` per i personaggi — il secondo cade oltre la fine del blocco licenza) e ci ho
costruito sopra conclusioni sbagliate. Gli offset vanno verificati, non ipotizzati.

---

## 5. Percorsi e file controllati

### Progetto

| Percorso | Esito |
|---|---|
| `KamekInclude/kamek.cpp`, `kamek.hpp` | **modificati** — fix 1.2 |
| `KamekLinker/` (sorgenti C# + exe) | **ricompilato** — fix 1.1 |
| `PulsarEngine/IO/ArchiveDecompressSafety.cpp` | **modificato** — innesco di riserva |
| `PulsarEngine/IO/LooseArchiveOverrides.cpp` | analizzato; `AreLooseArchiveOverridesEnabled()` ritorna `true` (diverso da rr, ma intenzionale) |
| `PulsarEngine/Codes/Codes.cpp` | **modificato** — patch 1.5 |
| `PulsarEngine/UI/MiscUI.cpp` | `FavouriteCombo` riallineata a upstream |
| `PulsarEngine/UI/UI.cpp` | `CreatePulPages` verificata, nessun problema |
| `PulsarEngine/Settings/UI/ExpSinglePlayer.cpp` | identico a rr-pulsar salvo formattazione |
| `PulsarEngine/Settings/UI/SettingsPanel.cpp` | `CreateControl` è nel percorso del crash aperto |
| `PulsarEngine/Extra/MiiOutfitC.cpp` | disabilitato, dipendenze non risolte |
| `PulsarEngine/Extra/DebugMenuDriver.cpp` | disabilitato: firma sbagliata, era dannoso |
| `PulsarEngine/Network/Rating/*` | disabilitati, non compilano |
| `GameSource/symbols.txt`, `versions.txt` | consultati; `[P]` è la versione base con mappatura identità |

### Gioco estratto

- `C:\Users\brutt\Desktop\Dolphin-x64\User\extracted game\DATA\sys\main.dol`
- `...\DATA\files\rel\StaticR.rel` — **base 0x805102E0**, la gran parte degli hook Pulsar sta qui, non nel DOL

### Installazioni Riivolution

| Percorso | Note |
|---|---|
| `E:\Dolphin-x64\User\Load\Riivolution\VKBeta\` | **modificato**: My Stuff spostato nella patch Pack |
| `...\VanzaKart\` | riferimento funzionante |
| `...\WheelWizard\` (RetroRewind) | riferimento; conferma che i percorsi `/Iace/`, `/Icene/` sono ereditati dal progetto di traduzione, non corrotti |
| `E:\Dolphin-x64\User\Wii\shared2\menu\FaceLib\` | inizialmente assente, poi popolato |

### Fork di riferimento

- `C:\Users\brutt\Desktop\rr-pulsar` — usato per i diff (`FavouriteCombo`, `EnhancedReplay`, `ExpSinglePlayer`, `LooseArchiveOverrides`, `Extra.cpp`)
- `E:\vk-Pulsar-main\Insane-Kart-Wii-main` — secondo riscontro

> Il `Kamek.exe` di rr-pulsar è **byte-identico** al vecchio nostro e contiene lo stesso bug:
> non copiarlo. rr non usa `kmRegionCall`, quindi non lo incontra mai.

---

## 6. Stato dell'ambiente e backup

Modifiche temporanee ancora attive:

| Cosa | Ripristino |
|---|---|
| `MiiBody.szs` rinominato | `Scene/Model/MiiBody.szs.test-disattivato` → togliere il suffisso |
| Salvataggio della beta sostituito con quello della stabile | `save/VanzaWFC2/RMCP/rksys.dat.bak-prima-del-test` |
| XML della beta modificato | `Riivolution/VKBeta.xml.bak-prima-spostamento-mystuff` |

Il `MiiBody.szs` va rimesso **solo dopo** aver riattivato `MiiOutfitC.cpp` (vedi 2.2),
altrimenti torna il crash 1.3.
