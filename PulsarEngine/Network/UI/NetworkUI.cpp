#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Other/GlobeSearch.hpp>
#include <PulsarSystem.hpp>
#include <UI/UI.hpp>

kmWrite32(0x80609268, 0x7f63db78);
namespace Pulsar {
namespace UI {

void PatchGlobeSearchBMG(Pages::GlobeSearch* globeSearch) {
    globeSearch->countdown.Update();
    if(System::sInstance->netMgr.deniesCount >= 3) globeSearch->messageWindow.LayoutUIControl::SetMessage(UI::BMG_TOO_MANY_DENIES);

    /*
        Qui c'era un blocco che, ogni frame, faceva isMiiShown = false + ResetGlobeMii(): cancellava
        la testa Mii che il gioco fa spuntare dal globo sopra il giocatore trovato, appena la
        mostrava. Da qui il nome del giocatore sul globo senza Mii accanto. rr-pulsar non ha nulla
        del genere in questo hook; il reset del Mii del globo lo fa solo ExpFroom::AfterControlUpdate
        e solo quando i controlli della froom sono nascosti.
    */
}
kmCall(0x8060926c, PatchGlobeSearchBMG);

}//namespace UI
}//namespace Pulsar