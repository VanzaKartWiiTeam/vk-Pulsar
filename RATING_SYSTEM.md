# Sistema Rating e Rank di VanzaKart Beta

Questo documento descrive il funzionamento del sistema rating della beta di VanzaKart, incluse le modifiche ai VR, il salvataggio in `VKRating.pul`, la trasmissione online, la visualizzazione dei badge e lo sblocco del Mii Outfit C.

> **Aggiornato dopo il refactoring del 2026-07-29.** Il sistema non è più diviso fra file attivi e `.disabled`: tutto sotto `PulsarEngine/Network/Rating/` è compilato. La struttura dei file descritta nella §2 è quella storica; la mappa dei moduli attuali è nella §2-bis. Le §8 e §9 riportano lo stato reale dei problemi.

## 1. Regole generali

Il rating viene conservato internamente come `float` diviso per 100.

| Valore interno | VR visualizzati |
|---:|---:|
| `50.00` | 5.000 VR |
| `250.00` | 25.000 VR |
| `500.00` | 50.000 VR |
| `5000.00` | 500.000 VR |

Configurazione attuale:

- rating iniziale: **5.000 VR**;
- minimo: **100 VR**;
- massimo: **500.000 VR**;
- rank disponibili: **8**;
- promozione ogni **25.000 VR**;
- il rating non viene resettato dopo una promozione;
- si perde un rank scendendo **500 VR sotto la sua soglia**.

### Soglie

| Rank | Promozione | Derank sotto |
|---:|---:|---:|
| 1 | 25.000 VR | 24.500 VR |
| 2 | 50.000 VR | 49.500 VR |
| 3 | 75.000 VR | 74.500 VR |
| 4 | 100.000 VR | 99.500 VR |
| 5 | 125.000 VR | 124.500 VR |
| 6 | 150.000 VR | 149.500 VR |
| 7 | 175.000 VR | 174.500 VR |
| 8 | 200.000 VR | nessun rank superiore |

Dopo Rank 8 si possono continuare ad accumulare VR fino a 500.000.

Le soglie sono implementate in `PlayerRating.cpp`:

```cpp
while (rank < 8 && next >= (float)(rank + 1) * 250.0f) ++rank;
while (rank > 0 && next < (float)rank * 250.0f - 5.0f) --rank;
```

`250.0` equivale a 25.000 VR e `5.0` equivale a 500 VR.

## 2. File in `PulsarEngine/Network/Rating`

### `PlayerRating.hpp`

È l'interfaccia centrale del sistema.

Definisce:

- `MIN_RATING`;
- `MAX_RATING`;
- `DEFAULT_RATING`;
- lettura e scrittura di VR, BR e rank;
- gestione dei rank remoti;
- decimali dei VR remoti;
- moltiplicatore;
- funzioni usate da menu, gara e rete.

Valori attuali:

```cpp
static const u16 MIN_RATING = 1;
static const u16 MAX_RATING = 5000;
static const float DEFAULT_RATING = 50.0f;
```

### `PlayerRating.cpp`

È il cuore del calcolo.

Sostituisce la funzione originale di Mario Kart Wii che aggiorna i punti a fine gara con `RR_UpdatePoints()`.

Per ogni giocatore:

1. legge il rating locale o remoto;
2. confronta il risultato contro ogni avversario;
3. aggiunge punti per gli avversari battuti;
4. sottrae punti per gli avversari arrivati davanti;
5. applica il moltiplicatore;
6. applica i limiti di guadagno e perdita;
7. aggiorna VR/BR;
8. aggiorna l'eventuale promozione o retrocessione;
9. salva la variazione dell'ultima gara.

Comportamenti aggiuntivi:

- sotto 15.000 VR le perdite vengono ridotte;
- sopra 90.000 VR il guadagno massimo viene fortemente limitato;
- la perdita massima normale è circa 209 VR;
- se tutti gli altri giocatori si disconnettono, con almeno quattro partecipanti viene assegnato solo `-1 VR`;
- perdite negative quasi nulle vengono trasformate in zero;
- il risultato viene troncato al singolo VR visualizzato.

Il file mantiene anche:

```cpp
u8 remoteDecimalVR[12][2];
static u8 remotePrestigeRanks[12][2];
float lastRaceDeltas[12];
```

### `RatingSave.cpp`

Gestisce il file:

```text
<cartella della mod>/VKRating.pul
```

Il formato attuale usa:

- magic `RRRT`;
- versione `2`;
- massimo 100 profili;
- associazione tramite `gsProfileId`;
- VR come `float`;
- BR come `float`;
- rank nel campo `flags`;
- flag di validità della voce.

Ogni voce contiene:

```cpp
struct PackedEntry {
    s32 profileId;
    float vr;
    float br;
    u32 flags;
};
```

Il rank occupa i bit 8–11 di `flags`.

Il salvataggio viene associato al profilo online e non solamente allo slot della patente. Questo impedisce che due patenti si scambino accidentalmente rating e rank.

Alla prima importazione:

1. legge VR e BR originali dal `rksys.dat`;
2. li divide per 100;
3. crea una voce in `VKRating.pul`;
4. conserva una copia dei valori originali;
5. usa il valore originale come mirror compatibile con il salvataggio vanilla.

### `RatingMultiplier.cpp`

Scarica il moltiplicatore da:

```text
http://sitodaking.it:8000/VanzaKart/multiplierBeta.txt
```

Accetta valori compresi fra `0.0` e `100.0`.

Esempi:

| File remoto | Effetto |
|---:|---|
| `1.0` | rating normale |
| `2.0` | variazioni raddoppiate |
| `3.0` | variazioni triplicate |
| `0.5` | variazioni dimezzate |
| `0.0` | nessuna variazione |

Il moltiplicatore viene applicato prima dei limiti massimi. Per questo un moltiplicatore `3.0` non garantisce sempre esattamente il triplo.

Se il download fallisce, il valore predefinito è `1.0`.

### `RatingConnectionBlock.cpp`

Prima del login DWC:

1. legge il `gsProfileId` della patente attiva;
2. associa la patente alla voce corretta di `VKRating.pul`;
3. chiama normalmente `DWC_LoginAsync`.

Non scarica il rating da un server. Serve a collegare correttamente patente e profilo.

### `RatingQR2.cpp`

Sostituisce i valori VR/BR pubblicati tramite QR2.

Chiavi:

- `0x65`: VR;
- `0x66`: BR.

Il rating interno viene moltiplicato per 100:

```text
627.30 interno -> 62730 pubblicato
```

Il valore pubblicato viene limitato fra 1 e 500.000.

### `RatingSync.cpp` e `RatingSync.hpp`

Sono placeholder.

Le funzioni esistono:

```cpp
SetSyncReportingSuppressed();
ReportCurrentRatings();
StartLoginRatingDownload();
```

ma non eseguono operazioni.

Conseguenze:

- `VKRating.pul` è la fonte autoritativa;
- non esiste un download del rating al login;
- spostandosi su un'altra console o installazione bisogna copiare `VKRating.pul`;
- QR2 pubblica il valore, ma questo modulo non lo salva su un database remoto.

### `RatingUI.cpp`

Gestisce la visualizzazione del rating nelle schermate online e nei record WFC.

`FormatRatingDigits()` concatena parte intera e centesimi:

| Rating interno | Testo |
|---:|---:|
| `50.00` | `5000` |
| `62.73` | `6273` |
| `627.30` | `62730` |

Il file modifica:

- schermata Check Members;
- VR e BR estesi;
- decimali dei giocatori remoti;
- record WFC;
- icona accanto al nome.

Problema noto: `GetNameRatingIcon()` continua a usare `wheelType` e `starRating` originali. Non legge direttamente il prestige rank. Questa parte deve essere collegata a `GetUserRank()` o `GetRemotePrestigeRank()` per evitare badge casuali.

## 2-bis. Moduli attuali

Il codice della §2 è stato ridistribuito in moduli a responsabilità singola. Le funzioni pubbliche non sono cambiate: `PlayerRating.hpp` resta l'header che il resto della mod include.

| File | Responsabilità |
|---|---|
| `RatingConfig.hpp` | Ogni costante regolabile: limiti, soglie rank, curva dei delta, cap, URL, interruttori. Nessuna logica. |
| `RatingTypes.hpp` | Tipi condivisi: `RatingValue`, `RankId`, `RatingKind`, `PlayerSnapshot`, `RaceContext`. |
| `RatingCalculator.*` | Matematica pura di fine gara. Nessun hook, nessun I/O, nessun globale: riceve uno snapshot e restituisce i delta. |
| `RankManager.*` | Unica fonte di verità sul rank: soglie, promozione/retrocessione, cache dei rank remoti, glifo del badge. |
| `RatingStorage.*` | `VKRating.pul`: lettura v1/v2/v3, scrittura v3, binding `gsProfileId`, mirror verso RKSYS, flush batch. |
| `MultiplierManager.*` | Moltiplicatore a strati componibili (`base × evento × weekend × remoto`). Solo il remoto è attivo. |
| `RatingNetwork.*` | Cosa va sul filo: payload `PulSELECT`, clamp QR2, binding del profilo al login. |
| `RatingSync.*` | Upload GPReport e download HTTP. Compilato, inerte finché `RATING_SYNC_ENABLED` è 0. |
| `RatingUI.*` | Formattazione VR/BR, schermata VR, record WFC, icona accanto al nome. |
| `RatingManager.*` | Orchestrazione: decide se la gara conta, snapshotta, chiama il calcolatore, salva. Più la facciata pubblica. |
| `RatingHooks.cpp` | Tutti gli indirizzi patchati, come thunk di poche righe. Unica eccezione documentata: i tre `kmBranch` di `RatingUI.cpp`, che sostituiscono intere funzioni. |

### Formato di salvataggio

`VKRating.pul` conserva magic `RRRT`. Il lettore accetta **v1** (Retro Rewind, senza rank), **v2** (rank nei bit 8–11 di `flags`) e **v3**; lo scrittore emette solo v3. La migrazione avviene al primo salvataggio ed è non distruttiva: il mirror del rank nei `flags` resta popolato, quindi una build precedente continua a leggere il file.

La voce v3 occupa 32 byte e mantiene il prefisso legacy byte per byte, aggiungendo `rank` esplicito più `totalRaces` e `wins` riservati alle statistiche future.

## 3. Trasmissione online

### `PacketExpansion.hpp`

Estende `PulSELECT` con:

```cpp
u8 prestigeRank[2];
u8 decimalVR[2];
```

Ogni console può quindi trasmettere rank e decimali di entrambi i giocatori locali.

### `PulSELECT.cpp`

Prima dell'invio:

- richiede il moltiplicatore;
- legge rank e rating locali;
- inserisce rank e decimali in `PulSELECT`.

Alla ricezione:

- controlla che il pacchetto abbia la dimensione completa di `PulSELECT`;
- salva rank e decimali associandoli all'AID remoto.

Problema attuale: viene riempito solamente lo slot zero.

```cpp
src->prestigeRank[0] = rank;
src->decimalVR[0] = ...;
```

Il secondo giocatore sulla stessa console rimane con rank e decimali pari a zero.

### `PulROOM.cpp`

La modifica contenuta nel commit rating riguarda Item Rain e non il calcolo dei rank.

### `PulsarSystem.hpp`

Aggiunge:

```cpp
PULSAR_VR
```

Questo contesto identifica una friend room ranked. Deve essere realmente inserito nel contesto della stanza, altrimenti le friend room restano non-ranked.

## 4. Visualizzazione dei badge

### `LeaderboardDisplay.cpp`

Quando la leaderboard mostra i nomi:

1. ottiene il prestige rank del giocatore;
2. converte il rank nel glifo `0xF07C + rank`;
3. aggiunge il glifo davanti al nome.

Il rank zero non mostra nulla.

### `ExtendedTeamSelectMisc.cpp`

Aggiunge il rank davanti al nome nella bolla in gara.

Il giocatore corretto viene identificato tramite:

```cpp
const u8 playerId = static_cast<u8>(_this->nameSlotId);
```

Il parametro ricevuto dalla funzione rappresenta l'HUD locale e non il giocatore mostrato. Usare quel parametro causava badge assegnati al giocatore sbagliato.

Il badge viene inserito dopo `UpdateInfo()`, in modo che il gioco non cancelli immediatamente il testo.

Questo file gestisce anche i colori delle squadre estese: entrambe le funzioni devono essere preservate.

### `ExpWFCMainPage.cpp` e `ExpWFCMainPage.hpp`

Aggiungono:

- `PlayerButton`;
- `RankButton`;
- `VRButton`.

La versione destinata al rating dovrebbe:

- mostrare `RankButton`;
- leggere i VR tramite `PointRating::GetUserVR()`;
- moltiplicare il valore interno per 100.

Nel file attualmente presente:

```cpp
rankInfo.isHidden = true;
```

e il VR viene nuovamente letto da:

```cpp
license.vr.points
```

Quindi il pulsante rank è nascosto e il VRButton non usa il rating esteso.

## 5. Mii Outfit C

### `MiiOutfitC.cpp`

Implementa il terzo outfit Mii e lo collega al rank.

Lo sblocco è controllato da:

```cpp
if (PointRating::GetUserRank(rksys->curLicenseId) < 1)
    return CHARACTER_NONE;
```

Con le soglie attuali, Mii C si sblocca a 25.000 VR.

Il file inoltre:

- aggiunge la terza colonna Mii;
- converte gli ID A/B negli ID C;
- corregge il nome visualizzato;
- ripristina il focus su Mii C;
- espande gli array degli outfit da 2 a 3;
- carica lo slot corretto in gara;
- riutilizza i parametri di visualizzazione dell'Outfit A;
- aumenta la memoria destinata ai modelli Mii.

### `MiiBody.szs`

Contiene i modelli richiesti dal Mii Outfit C.

Se il codice sblocca Mii C ma l'archivio non viene caricato, possono verificarsi:

- modello mancante;
- crash entrando in gara;
- risorse pilota non trovate.

## 6. Asset necessari

### `UIAssets.szs`

Deve contenere:

- `RankButton.brctr`;
- `VRButton.brctr`;
- layout e animazioni dei badge;
- texture richieste;
- glifi corrispondenti a `0xF07D`–`0xF084`.

### `RaceAssets.szs`, `Race.szs` e file Race delle lingue

Contengono le risorse necessarie alla visualizzazione in gara.

Se un font o glifo manca in una variante linguistica, il rank può essere corretto internamente ma risultare invisibile.

### `Award.szs` e file Award delle lingue

Servono solo per eventuali badge o testi personalizzati nelle schermate premio e risultato. Non partecipano al calcolo.

## 7. Fonte corretta del rank

Ogni schermata deve utilizzare una di queste due fonti:

```text
Giocatore locale
VKRating.pul -> GetUserRank()

Giocatore remoto
PulSELECT.prestigeRank -> GetRemotePrestigeRank()
```

Non bisogna usare per il prestige rank:

- `starRank`;
- `wheelType`;
- VR vanilla della patente;
- valori non inizializzati della schermata.

## 8. Stato dei problemi

### Risolti

1. ~~I file `.cpp.disabled` non vengono compilati.~~ Tutto il modulo Rating è compilato.
2. ~~`MiiOutfitC.cpp.disabled` non viene compilato.~~ Riabilitato.
3. ~~`PulSELECT.cpp` non trasmette rank e decimali.~~ Delegato a `RatingNetwork::FillLocalPayload`.
4. ~~`PacketExpansion.hpp` conserva i relativi campi.~~ `prestigeRank[2]` e `decimalVR[2]` ripristinati.
6. ~~Il VRButton legge il valore vanilla.~~ Legge `GetUserVR() * 100`.
7. ~~Il secondo giocatore locale non trasmette il proprio rank.~~ Entrambi gli slot pubblicano rank e decimali della patente condivisa, e il badge in gara identifica il giocatore da `nameSlotId` invece che dallo slot HUD.
10. ~~`PULSAR_VR` non viene mai attivato.~~ Nuova radio `SETTINGHOST_RADIO_RANKED` nella pagina Host, impostata in `UpdateContext` e trasmessa in `PulROOM`.

Corretti in aggiunta, non elencati nella versione precedente del documento:

- **Il più grave: nessun profilo veniva accettato.** `IsUsableProfileId` rifiutava ogni id
  maggiore o uguale a `1000000000`, soglia ereditata da Retro Rewind. Il server VanzaKart
  assegna però PID proprio in quell'intervallo (`1000000134`, `1000000211`, …), quindi
  `ResolveProfileIdForLicense` ritornava sempre 0 e **niente veniva mai né letto né
  scritto**: il rating restava eternamente al default di 5000 VR, il mirror sulla patente
  non partiva, e sul filo viaggiava il VR vanilla che il display reinterpretava come
  valore interno rendendolo cento volte più grande (`ev=1000` mostrato come 100000).
  Ora il limite superiore non esiste: basta che l'id sia positivo.

- Il cap di perdita diventava **positivo** sotto ~3.994 VR, trasformandosi in un guadagno minimo garantito per chi arrivava ultimo. La rampa ora è limitata ai due estremi.
- Il rating veniva riletto dal file circa 168 volte per gara dentro un ciclo O(n²); ora si fa un solo snapshot.
- Il file di salvataggio veniva riscritto due volte per gara; ora un solo flush per commit.
- Lo sfratto round-robin dei profili poteva eliminare la patente in uso.
- Il bound del rank in trasmissione era 9 mentre la ricezione ne accetta al massimo 8.
- `ApplyRatingPatch` dereferenziava `RKNet::Controller::sInstance` senza verificarlo.

### Aperti

5. `ExpWFCMainPage.cpp` nasconde ancora `RankButton`. Il controllo si carica, ma nulla ne popola il testo: serve `RankButton.brctr` in `UIAssets.szs` più le stringhe BMG dei nomi rank.
8. `GetNameRatingIcon()` usa ancora lo star rank originale. È la metà mancante del rank al posto delle stelle: vedi §8-bis.
9. La sincronizzazione HTTP è implementata ma **disattivata** da `RATING_SYNC_ENABLED`, perché l'endpoint lato server non esiste ancora.
11. Tutte le lingue devono contenere gli stessi glifi dei rank.
12. Il `MiiBody.szs` custom va riattivato ora che `MiiOutfitC.cpp` è compilato: archivio a 3 outfit e codice a stride 3 devono corrispondere, in entrambe le direzioni.
13. Serve una stringa BMG per la radio "Friend Room Ranked", altrimenti l'opzione compare senza etichetta.

## 8-bis. Rank al posto delle stelle vanilla

Mario Kart Wii trasmette già un rank per giocatore e per slot console, ed è quello che disegna l'icona sopra il nome:

- `SELECTPlayerData::starRank` (0x7), trasmesso per aid e per hudSlot;
- `RACEHEADER1Packet::starRank[2]` (0x14), il percorso in gara;
- `OnlineParams::CalcRank(wheelType, starRank)` a `0x805e3d38`, che produce l'indice;
- `OnlineParams::rankBMG[12]`, che è un array di **id di messaggio BMG**, non di frame di texture.

Scriverci il prestige costa zero byte aggiuntivi e copre entrambi i giocatori locali gratuitamente. Il lato invio è già implementato in `RatingNetwork::FillLocalPayload`, dietro `Config::RATING_RANK_REPLACES_STARS`.

Manca il lato display. `CalcRank` restituisce `wheelType * 4 + starRank`, e il risultato indicizza `rankBMG`: un rank fino a 8 sfonderebbe il range degli id che il gioco possiede, rompendo l'icona per tutti. Per chiudere: individuare in gioco da quale id BMG è calcolato quel ritorno, aggiungere un messaggio per rank con il glifo corrispondente, far restituire a `GetNameRatingIcon` il nostro indice, e accendere l'interruttore. **Le due metà vanno mosse insieme.**

Nel frattempo il rank viene comunque condiviso e mostrato, tramite `prestigeRank[2]` in `PulSELECT` e il glifo `0xF07C + rank` davanti al nome in leaderboard e nella bolla in gara.

## 9. Ordine corretto di inizializzazione

```text
Avvio Pulsar
  -> inizializzazione IO
  -> apertura VKRating.pul
  -> lettura del profilo
  -> associazione patente/gsProfileId
  -> caricamento VR, BR e rank
  -> login DWC
  -> pubblicazione QR2
  -> invio rank e decimali tramite PulSELECT
  -> cache dei dati remoti
  -> visualizzazione nei menu e in gara
  -> calcolo a fine gara
  -> promozione/derank
  -> salvataggio VKRating.pul
```

