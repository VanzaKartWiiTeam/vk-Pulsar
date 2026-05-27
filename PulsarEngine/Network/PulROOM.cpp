#include <kamek.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <Network/Network.hpp>
#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace Network {

//Implements the ability for a host to send a message, allowing for custom host settings

//If we are in a room, we are guaranteed to be in a situation where Pul packets are being sent
//however, no reason to send the settings outside of START packets and if we are not the host, this is easily changed by just editing the check

static void ConvertROOMPacketToData(const PulROOM& packet) {
    System* system = System::sInstance;
    system->netMgr.hostContext = packet.hostSystemContext;
    system->netMgr.racesPerGP = packet.raceCount;
}

static void BeforeROOMSend(RKNet::PacketHolder<PulROOM>* packetHolder, PulROOM* src, u32 len) {
    packetHolder->Copy(src, len); //default

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    Pulsar::System* system = Pulsar::System::sInstance;
    PulROOM* destPacket = packetHolder->packet;
    if (destPacket->messageType == 1 && sub.localAid == sub.hostAid) {
        packetHolder->packetSize += sizeof(PulROOM) - sizeof(RKNet::ROOMPacket); //this has been changed by copy so it's safe to do this

        // Save original message before remapping.
        // Messages 4 and 5 are OPT WW and OTT WW starts (added by ExpFroomMessages).
        // Remap them to 0 so the base game handles them as a normal VS start.
        const u8 originalMessage = destPacket->message;
        if (originalMessage >= 4 && originalMessage <= 9) {
            destPacket->message = 0;
        }

        const u8 isStartVKWW = (originalMessage == 4);
        const u8 isStartOTWW  = (originalMessage == 5);

        const Settings::Mgr& settings = Settings::Mgr::Get();

        const u8 koSetting = settings.GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_ENABLED) && destPacket->message == 0; //KO only enabled for normal GPs
        //invert mii setting as the first button is enabled, not disabled, so a value of 1 indicates disabled
        const u8 ottOnline = settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ONLINE);
        destPacket->hostSystemContext = (ottOnline != OTTSETTING_OFFLINE_DISABLED) << PULSAR_MODE_OTT //ott
            | (ottOnline == OTTSETTING_ONLINE_FEATHER) << PULSAR_FEATHER //ott feather
            | (settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ALLOWUMTS) ^ true) << PULSAR_UMTS //ott umts
            | koSetting << PULSAR_MODE_KO
            | (settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_ALLOW_MIIHEADS) ^ true) << PULSAR_MIIHEADS
            | settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_HOSTWINS) << PULSAR_HAW
            | isStartVKWW << PULSAR_STARTVKWW   // OPT WW start from friend room
            | isStartOTWW  << PULSAR_STARTOTTWW;  // OTT WW start from friend room

        u8 raceCount;
        if (koSetting == KOSETTING_ENABLED) raceCount = 0xFE;
        else switch (settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_SCROLL_GP_RACES)) {
        case(0x2):
            raceCount = 7;
            break;
        case(0x4):
            raceCount = 11;
            break;
        case(0x6):
            raceCount = 23;
            break;
        case(0x8):
            raceCount = 31;
            break;
        case(0xA):
            raceCount = 63;
            break;
        case(0xC):
            raceCount = 1;
            break;
        default:
            raceCount = 3;
        }
        destPacket->raceCount = raceCount;
        ConvertROOMPacketToData(*destPacket);
    }
}
kmCall(0x8065b15c, BeforeROOMSend);

kmWrite32(0x8065add0, 0x60000000);
static void AfterROOMReception(const RKNet::PacketHolder<PulROOM>* packetHolder, const PulROOM& src, u32 len) {
    register RKNet::ROOMPacket* packet;
    asm(mr packet, r28;);
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    Pulsar::System* system = Pulsar::System::sInstance;
    //START msg sent by the host, size check should always be guaranteed in theory
    if (src.messageType == 1 && sub.localAid != sub.hostAid && packetHolder->packetSize == sizeof(PulROOM)) {
        ConvertROOMPacketToData(src);

        // Apply OPT WW / OTT WW contexts locally on all non-host clients.
        // The host packed these into hostSystemContext in BeforeROOMSend.
        const u32 hostContext = src.hostSystemContext;
        const bool isStartVKWW = hostContext & (1 << PULSAR_STARTVKWW);
        const bool isStartOTWW  = hostContext & (1 << PULSAR_STARTOTTWW);
        system->SetContext((isStartVKWW << PULSAR_STARTVKWW) | (isStartOTWW << PULSAR_STARTOTTWW));

        //Also exit the settings page to prevent weird graphical artefacts
        Page* topPage = SectionMgr::sInstance->curSection->GetTopLayerPage();
        PageId topId = topPage->pageId;
        if (topId == UI::SettingsPanel::id) {
            UI::SettingsPanel* panel = static_cast<UI::SettingsPanel*>(topPage);
            panel->OnBackPress(0);
        }
    }
    memcpy(packet, &src, sizeof(RKNet::ROOMPacket)); //default
}
kmCall(0x8065add8, AfterROOMReception);

//Implements that setting
kmCall(0x806460B8, System::GetRaceCount);
kmCall(0x8064f51c, System::GetRaceCount);
}//namespace Network
}//namespace Pulsar
