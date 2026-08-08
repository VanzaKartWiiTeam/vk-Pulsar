#include <kamek.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Network {

// Reduce PING retry time from 700 to 80 [Wiimmfi]
kmWrite16(0x8011B47A, 80);

// Do not wait the retry time in case of successful NATNEG [Wiimmfi]
kmWrite32(0x8011B4B0, 0x60000000);

// Do not wait the idle time after a successful NATNEG [WiiLink24]
kmWrite16(0x8011BC3A, 0);

// Change the SYN-ACK timeout to 7 seconds instead of 5 seconds per node [Wiimmfi]
kmWrite32(0x800E1A58, 0x38C00000 | 7000);

// Fix the "suspend bug" where DWC stalls suspending due to ongoing NATNEG [WiiLink24, MrBean35000vr]
kmWrite32(0x800E77F8, 0x60000000);
kmWrite32(0x800E77FC, 0x60000000);

// Slower High Data Rate [MrBean35000vr, Chadderz]
kmWrite32(0x80657EA8, 0x2804000C);

// Pulsar Network Optimizations [ZPL]
// Reduce server polling interval from 15000ms to 10000ms (li r6, 0x3a98 -> 0x2710)
kmWrite16(0x800E6E1E, 10000);

// Reduce match state 1 timeout from 3000ms to 2000ms (li r6, 0xbb8 -> 0x7d0)
kmWrite16(0x800D69AA, 2000);

// Reduce connection check delay from 5000ms to 3000ms (li r6, 0x1388 -> 0xbb8)
kmWrite16(0x800D771E, 3000);

// Reduce NATNEG report retry delay from 1000ms to 500ms (addi r0, r3, 0x3e8 -> 0x1f4)
kmWrite16(0x8011B6F6, 500);

/*
    Fix Ghost Player Bug [ImZeaora] -- SOSPESO, sotto test.

    0x80662f5c cade dentro USERHandler::ImportNewPackets (0x80662ebc -> 0x8066300c), la funzione
    che importa i pacchetti USER ricevuti dagli altri giocatori: sono quelli che portano i loro
    Mii. Mettere un nop li' e' il candidato piu' probabile per il "?" al posto del Mii altrui.

    rr-pulsar, dove i Mii online si vedono, questa patch non ce l'ha (il resto di questo file e'
    identico al suo). Se togliendola i Mii tornano, va rifatta in modo mirato invece che con un
    nop cieco; se non cambia nulla si rimette e si cerca altrove.
*/
//kmWrite32(0x80662f5c, 0x60000000);

/*
    Qui c'erano due cose costruite sulla premessa che gli altri vedessero un "?" al posto del
    nostro Mii: un retry che richiamava USERHandler::CreateSendPacket() ogni 20 frame finche' il
    pacchetto non usciva con StoreData::invalid a 0, e una diagnostica sui pacchetti ricevuti.

    La premessa era sbagliata. Il "?" nelle bolle in alto in GlobeSearch e' il segnaposto normale
    per gli slot ancora liberi; quello che mancava davvero era la testa Mii sopra il globo, che
    veniva cancellata a ogni frame da PatchGlobeSearchBMG (vedi Network/UI/NetworkUI.cpp).

    Per la cronaca, verificato disassemblando: RFL::SetToWiFiPacket (0x800cc048) copia il bit
    invalid senza guardarlo, e RFL::CheckValidRaw (0x800cb840) lato ricevente controlla CRC16,
    createID e range dei campi ma non quel bit -- l'header Pulsar stesso lo commenta "doesn't
    seem to have any effect?". Il retry inseguiva quindi un flag che non decide nulla, e comunque
    arrivava dopo che il peer aveva gia' importato il primo pacchetto (GlobeSearch carica i Mii
    una volta sola, latch a GlobeSearch+0x1cf0).
*/

}  // namespace Network
}  // namespace Pulsar