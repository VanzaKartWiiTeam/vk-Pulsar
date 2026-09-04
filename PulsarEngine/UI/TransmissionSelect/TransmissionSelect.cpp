#include <PulsarSystem.hpp>
#include <Debug/BetaLog.hpp>
#include <Race/RoomContext.hpp>
#include <UI/TransmissionSelect/TransmissionSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/KartSelect.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace UI {

/*
Ported from rr-pulsar (PulsarEngine/UI/TransmissionSelect). Two differences worth knowing:

- rr loads a THP preview next to the buttons. That movie is not in this pack, so it is left out;
  adding it later is one LoadMovies call.
- rr decides where the page may appear from its own PULSAR_TRANSMISSION* contexts. Here the rule
  is the one already applied to the rest of the transmission system: offline and friend rooms
  only, never mogi, never a worldwide. Everyone in a public room has to be on the same physics.
*/

static VanzaKart::Transmission selectedTransmission[4] = {
    VanzaKart::TRANSMISSION_DEFAULT,
    VanzaKart::TRANSMISSION_DEFAULT,
    VanzaKart::TRANSMISSION_DEFAULT,
    VanzaKart::TRANSMISSION_DEFAULT,
};

VanzaKart::Transmission GetSelectedTransmission(u32 hudSlotId) {
    if(hudSlotId >= 4) return VanzaKart::TRANSMISSION_DEFAULT;
    return selectedTransmission[hudSlotId];
}

void SetSelectedTransmission(u32 hudSlotId, VanzaKart::Transmission transmission) {
    if(hudSlotId < 4) selectedTransmission[hudSlotId] = transmission;
}

//Button 0 is the left one and carries the Inside label set in SetTransmissionMessages. The
//order matters in three other places below, so it is stated once here and derived everywhere
//else: getting it wrong means the page promises one drift type and hands over the other.
static VanzaKart::Transmission GetTransmissionFromButton(const PushButton& button) {
    return button.buttonId == 0 ? VanzaKart::TRANSMISSION_INSIDE : VanzaKart::TRANSMISSION_OUTSIDE;
}

static u32 GetButtonForTransmission(VanzaKart::Transmission transmission) {
    return transmission == VanzaKart::TRANSMISSION_INSIDE ? 0 : 1;
}

static bool ShouldSkipTransmissionSelect() {
    if(!VanzaKart::RoomContext::IsTransmissionAllowed()) return true;
    //Two local players share one drift page in vanilla; rather than teach this page to handle
    //split screen, hand those sessions straight to the drift select.
    const SectionParams* params = SectionMgr::sInstance->sectionParams;
    return params != nullptr && params->localPlayerCount > 1;
}

static void SetTransmissionMessages(Pages::Menu& menu) {
    menu.titleText->SetMessage(BMG_TRANSMISSION_SELECT);
    menu.externControls[0]->SetMessage(BMG_INSIDE_TRANSMISSION);
    menu.externControls[1]->SetMessage(BMG_OUTSIDE_TRANSMISSION);
}

//The drift page has a third button this page has no use for.
static void HideTransmissionExtras(Pages::Menu& menu) {
    if(menu.externControlCount > 2) {
        menu.externControls[2]->isHidden = true;
        menu.externControls[2]->manipulator.inaccessible = true;
    }
}

static void SelectCurrentTransmission(Pages::Menu& menu) {
    menu.SelectButton(*menu.externControls[GetButtonForTransmission(GetSelectedTransmission(0))]);
}

//Online the kart select runs on a countdown; the page that follows has to keep the same clock or
//the room desynchronises while one player dithers over the choice.
static void CopyKartTimerToTransmission(Pages::Menu& menu) {
    ExpSection* section = ExpSection::GetSection();
    if(section == nullptr) return;
    TransmissionSelect* page = section->GetPulPage<TransmissionSelect>();
    if(page == nullptr) return;
    page->timer = static_cast<Pages::KartSelect&>(menu).timer;
}

void TransmissionSelect::OnInit() {
    Pages::DriftSelect::OnInit();
    SetTransmissionMessages(*this);
    HideTransmissionExtras(*this);
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf =
        static_cast<void (Pages::MenuInteractable::*)(PushButton&, u32)>(&TransmissionSelect::OnButtonClick);
}

void TransmissionSelect::OnActivate() {
    this->Pages::Menu::OnActivate();
    SetTransmissionMessages(*this);
    HideTransmissionExtras(*this);
    SelectCurrentTransmission(*this);
}

//When the online timer runs out nobody has pressed anything, so commit whatever is highlighted
//(or the previous choice) and move on, exactly like the vanilla drift page does.
void TransmissionSelect::AfterControlUpdate() {
    if(this->currentState != STATE_ACTIVE || this->timer == nullptr) return;
    if(this->timer->countdown > 0.0f) return;

    PushButton* button;
    if(this->externControls[0]->IsSelected()) button = this->externControls[0];
    else if(this->externControls[1]->IsSelected()) button = this->externControls[1];
    else {
        button = this->externControls[GetButtonForTransmission(GetSelectedTransmission(0))];
    }

    this->OnExternalButtonSelect(*button, 0);
    button->SelectFocus();
    this->OnButtonClick(*button, 0);
}

void TransmissionSelect::OnExternalButtonSelect(PushButton& button, u32) {
    if(button.buttonId == -100) { //the back button
        this->bottomText->SetMessage(0);
        return;
    }
    this->bottomText->SetMessage(button.buttonId == 0 ? BMG_INSIDE_TRANSMISSION_BOTTOM
                                                      : BMG_OUTSIDE_TRANSMISSION_BOTTOM);
}

void TransmissionSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    if(button.buttonId == -100) {
        this->LoadPrevPage(button);
        return;
    }
    const VanzaKart::Transmission chosen = GetTransmissionFromButton(button);
    SetSelectedTransmission(hudSlotId, chosen);
    PUL_BETA_LOG("[Transmission] slot %d picked %s\n", (int)hudSlotId,
                 chosen == VanzaKart::TRANSMISSION_OUTSIDE ? "outside" : "inside");
    this->LoadNextPageById(PAGE_DRIFT_SELECT, button);
}

/*
The kart select asks for the drift page from four different places depending on how it was
reached. Each of those calls is redirected here so the transmission page gets a turn first; when
it is not wanted the original destination is loaded and nothing about the flow changes.
*/
void LoadTransmissionSelectBeforeDrift(Pages::Menu& menu, PageId id, PushButton& button) {
    if(ShouldSkipTransmissionSelect()) {
        menu.LoadNextPageById(id, button);
        return;
    }
    CopyKartTimerToTransmission(menu);
    menu.LoadNextPageById(static_cast<PageId>(TransmissionSelect::id), button);
}
kmCall(0x80846d2c, LoadTransmissionSelectBeforeDrift);
kmCall(0x80846d64, LoadTransmissionSelectBeforeDrift);
kmCall(0x80846e1c, LoadTransmissionSelectBeforeDrift);
kmCall(0x80846e40, LoadTransmissionSelectBeforeDrift);

}  // namespace UI
}  // namespace Pulsar
