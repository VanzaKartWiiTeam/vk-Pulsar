#ifndef _PUL_TRANSMISSIONSELECT_
#define _PUL_TRANSMISSIONSELECT_

#include <kamek.hpp>
#include <VanzaKart.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Page/Menu/DriftSelect.hpp>

namespace Pulsar {
namespace UI {

//The choice survives between races, the way the vanilla drift setting does. Four entries because
//only local players ever pick one; CPUs keep using System::transmissions, which the CPU
//randomiser fills in.
VanzaKart::Transmission GetSelectedTransmission(u32 hudSlotId);
void SetSelectedTransmission(u32 hudSlotId, VanzaKart::Transmission transmission);

/*
Outside/Inside picker, slotted in between the kart select and the vanilla Automatic/Manual drift
page. Ported from rr-pulsar, which reuses the drift select page rather than building a new one:
the layout, the buttons and the timer all come for free, only the texts change.
*/
class TransmissionSelect : public Pages::DriftSelect {
public:
    static const u32 id = static_cast<u32>(PULPAGE_TRANSMISSIONSELECT);

    void OnInit() override;
    void OnActivate() override;
    void AfterControlUpdate() override;
    void OnExternalButtonSelect(PushButton& button, u32 hudSlotId) override;
    void OnButtonClick(PushButton& button, u32 hudSlotId);
};

void LoadTransmissionSelectBeforeDrift(Pages::Menu& menu, PageId id, PushButton& button);

}  // namespace UI
}  // namespace Pulsar

#endif
