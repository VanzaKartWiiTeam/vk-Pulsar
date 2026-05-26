#include <AutoTrackSelect/ExpFroomMessages.hpp>
#include <Settings/Settings.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <Gamemodes/OnlineTT/OTTRegional.hpp>

namespace Pulsar {
namespace UI {
bool ExpFroomMessages::isOnModeSelection = false;
s32 ExpFroomMessages::clickedButtonIdx = 0;

void ExpFroomMessages::OnModeButtonClick(PushButton& button, u32 hudSlotId) {
    this->clickedButtonIdx = button.buttonId;
    // WW modes (4 = VK WW, 5 = OTT WW) bypass track selection entirely
    if (button.buttonId >= 4) {
        Pages::FriendRoomMessages::OnModeButtonClick(button, hudSlotId);
        return;
    }
    this->OnActivate();
}

void ExpFroomMessages::OnCourseButtonClick(PushButton& button, u32 hudSlotId) {
    CupsConfig* cupsConfig = CupsConfig::sInstance;
    u32 clickedIdx = clickedButtonIdx;
    s32 id = button.buttonId;
    PulsarId pulsarId = static_cast<PulsarId>(id);
    if (clickedIdx < 2) {
        if (id == this->msgCount - 1) {
            pulsarId = cupsConfig->RandomizeTrack();
        }
        else {
            PulsarCupId cupId = static_cast<PulsarCupId>(cupsConfig->ConvertTrack_IdxToPulsarId(id) / 4);
            pulsarId = cupsConfig->ConvertTrack_PulsarCupToTrack(cupId, id % 4);
        }
    }
    else pulsarId = static_cast<PulsarId>(pulsarId + 0x20U); //Battle
    cupsConfig->SetWinning(pulsarId);
    PushButton& clickedButton = this->messages[0].buttons[clickedIdx];
    clickedButton.buttonId = clickedIdx;
    Pages::FriendRoomMessages::OnModeButtonClick(clickedButton, 0);
}

static void OnStartButtonFroomMsgActivate() {
    register ExpFroomMessages* msg;
    asm(mr msg, r31;);

    if (!Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_HOSTWINS)) {
        msg->onModeButtonClickHandler.ptmf = &Pages::FriendRoomMessages::OnModeButtonClick;
        msg->msgCount = 6; // 4 standard + VK WW (4) + OTT WW (5)
    }
    else {
        for (int i = 0; i < 4; ++i) msg->messages[0].buttons[i].HandleDeselect(0, -1);
        if (msg->isOnModeSelection) {
            msg->isOnModeSelection = false;
            if (msg->clickedButtonIdx >= 4) return; // WW VKions already handled
            if (msg->clickedButtonIdx >= 2) msg->msgCount = 10;
            else msg->msgCount = CupsConfig::sInstance->GetEffectiveTrackCount() + 1;
            msg->onModeButtonClickHandler.ptmf = &ExpFroomMessages::OnCourseButtonClick;
        }
        else {
            msg->isOnModeSelection = true;
            msg->msgCount = 6; // 4 standard + VK WW (4) + OTT WW (5)
            msg->onModeButtonClickHandler.ptmf = &ExpFroomMessages::OnModeButtonClick;
        }
    }
}
kmCall(0x805dc480, OnStartButtonFroomMsgActivate);

static void OnBackPress(ExpFroomMessages& msg) {
    if (Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_HOSTWINS) && msg.location == 1) {
        if (!msg.isOnModeSelection) {
            msg.isEnding = false;
            msg.OnActivate();
        }
        else msg.isOnModeSelection = false;
    }
}
kmBranch(0x805dd32c, OnBackPress);

static void OnBackButtonClick() {
    OnBackPress(*SectionMgr::sInstance->curSection->Get<ExpFroomMessages>());
}
kmBranch(0x805dd314, OnBackButtonClick);

u32 CorrectModeButtonsBMG(const RKNet::ROOMPacket& packet) {
    register u32 absRowIdx;   // r24: absolute index (0-5), used to detect WW rows
    asm(mr absRowIdx, r24;);
    register u32 pageRowIdx;  // r22: within-page index (0-3), used for track selection
    asm(mr pageRowIdx, r22;);
    register const ExpFroomMessages* messages;
    asm(mr messages, r19;);

    // WW VKions always checked first using absolute index
    if (absRowIdx == 4) return BMG_VKWW_START_MESSAGE;
    if (absRowIdx == 5) return BMG_OTTWW_START_MESSAGE;

    // Hostwins track/battle selection phase
    const bool hostwins = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_HOSTWINS);
    if (hostwins && !messages->isOnModeSelection) {
        if (messages->clickedButtonIdx >= 2 && messages->clickedButtonIdx < 4) {
            return BMG_BATTLE + messages->curPageIdx * 4 + pageRowIdx + DELFINO_PIER;
        }
        else {
            if (pageRowIdx + messages->curPageIdx * 4 == messages->msgCount - 1) {
                return BMG_RANDOM_TRACK;
            }
            else {
                CupsConfig* cupsConfig = CupsConfig::sInstance;
                bool hasRegs = cupsConfig->HasRegs();
                u32 idx = messages->curPageIdx;
                if (!hasRegs) idx += 8;
                return GetTrackBMGId(cupsConfig->ConvertTrack_PulsarCupToTrack(CupsConfig::ConvertCup_IdxToPulsarId(idx), pageRowIdx), true);
            }
        }
    }

    return Pages::FriendRoomManager::GetMessageBmg(packet, 0);
}
kmCall(0x805dcb74, CorrectModeButtonsBMG);

void CorrectRoomStartButton(Pages::Globe::MessageWindow& control, u32 bmgId, Text::Info* info) {
    Network::SetGlobeMsgColor(control, -1);
    if (bmgId == BMG_PLAY_GP || bmgId == BMG_PLAY_TEAM_GP) {
        const u32 hostContext   = System::sInstance->netMgr.hostContext;
        const bool isStartVKWW = hostContext & (1 << PULSAR_STARTVKWW);
        const bool isStartOTTWW = hostContext & (1 << PULSAR_STARTOTTWW);
        const bool isOTT        = hostContext & (1 << PULSAR_MODE_OTT);
        const bool isKO         = hostContext & (1 << PULSAR_MODE_KO);

        if (isStartVKWW) {
            bmgId = BMG_VKWW_START_MESSAGE;
        }
        else if (isStartOTTWW) {
            bmgId = BMG_OTTWW_START_MESSAGE;
        }
        else if (isOTT || isKO) {
            const bool isTeam = bmgId == BMG_PLAY_TEAM_GP;
            bmgId = (BMG_PLAY_OTT - 1) + isOTT + isKO * 2 + isTeam * 3;
        }
    }
    control.SetMessage(bmgId, info);
}
kmCall(0x805e4df4, CorrectRoomStartButton);

// Remap WW message IDs (4/5) to 0 when stored locally
static void RemapAndStoreSentMessage() {
    register u32 packet;
    register u32 manager;
    asm(mr packet, r30;);
    asm(mr manager, r28;);
    u32 message = (packet >> 8) & 0xFFFF;
    if (message >= 4 && message <= 9) {
        packet = packet & 0xFF0000FF;
    }
    *(volatile u32*)((u8*)manager + 0x2c60) = packet;
}
kmCall(0x805dce38, RemapAndStoreSentMessage);

}//namespace UI
}//namespace Pulsar
