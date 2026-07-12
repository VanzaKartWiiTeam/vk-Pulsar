#include <kamek.hpp>
#include <MarioKartWii/RKNet/ROOM.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/Settings.hpp>
#include <Network/Network.hpp>
#include <Network/PacketExpansion.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamSelect.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>

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

static void WriteBlockedTracksToPacket(PulROOM* packet) {
    System* system = System::sInstance;
    if (!system) return;

    const Network::Mgr& netMgr = system->netMgr;
    const u32 blockingCount = system->GetInfo().GetTrackBlocking();

    const u32 writeCount = (blockingCount < MAX_TRACK_BLOCKING) ? blockingCount : MAX_TRACK_BLOCKING;
    packet->blockedTrackCount = static_cast<u8>(writeCount);
    packet->curBlockingArrayIdx = netMgr.curBlockingArrayIdx;
    packet->lastGroupedTrackPlayed = netMgr.lastGroupedTrackPlayed;

    for (u32 i = 0; i < writeCount; ++i) {
        packet->blockedTracks[i] = (netMgr.lastTracks != nullptr && netMgr.lastTracks[i] != PULSARID_NONE) ? static_cast<u16>(netMgr.lastTracks[i]) : 0xFFFF;
    }
    for (u32 i = writeCount; i < MAX_TRACK_BLOCKING; ++i) {
        packet->blockedTracks[i] = 0xFFFF;
    }
}

extern "C" void OSReport(const char* format, ...);

static void HandleExtendedTeamUpdates(const PulROOM& packet) {
    UI::ExtendedTeamSelect* ets = SectionMgr::sInstance->curSection->Get<UI::ExtendedTeamSelect>();
    OSReport("[Pulsar LOG] HandleExtendedTeamUpdates: ets=%p\n", ets);
    for (int id = 0; id < 12; ++id) {
        const u8 byte = id / 2;
        const u8 shift = (id % 2) * 4;
        UI::ExtendedTeamID team = static_cast<UI::ExtendedTeamID>(packet.extendedTeams[byte] >> shift & 0x0F);
        OSReport("[Pulsar LOG] HandleExtendedTeamUpdates: id=%d, team=%d\n", id, team);
        if (team != 0x0F) {
            if (ets != nullptr) {
                ets->UpdatePlayerTeam(id, team);
            } else {
                UI::ExtendedTeamManager::sInstance->SetPlayerTeam(id, team);
            }
        }
    }
}

static void BeforeROOMSend(RKNet::PacketHolder<PulROOM>* packetHolder, PulROOM* src, u32 len) {
    packetHolder->Copy(src, len); //default

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    Pulsar::System* system = Pulsar::System::sInstance;
    PulROOM* destPacket = packetHolder->packet;
    OSReport("[Pulsar LOG] BeforeROOMSend: msgType=%d, localAid=%d, hostAid=%d\n", destPacket->messageType, sub.localAid, sub.hostAid);
    if (destPacket->messageType == 1 && sub.localAid == sub.hostAid) {
        packetHolder->packetSize = sizeof(PulROOM); //this has been changed by copy so it's safe to do this
        OSReport("[Pulsar LOG] BeforeROOMSend: setting packetSize to sizeof(PulROOM) = %d\n", sizeof(PulROOM));

        // Save original message before remapping.
        // Messages 4 and 5 are OPT WW and OTT WW starts (added by ExpFroomMessages).
        // Remap them to 0 or 1 so the base game handles them as a normal VS / Team VS start.
        const u8 originalMessage = destPacket->message;
        const Settings::Mgr& settings = Settings::Mgr::Get();
        const bool isStartMogi = settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_MOGI) == 1;

        if (isStartMogi) {
            destPacket->message = 0; // Force Solo VS
        } else if (originalMessage >= 4 && originalMessage <= 8) {
            if (originalMessage == 8) { // Item Rain Team VS (8)
                destPacket->message = 1; // Team VS
            } else {
                destPacket->message = 0; // VS
            }
        }

        const u8 isStartVKWW = !isStartMogi && (originalMessage == 4);
        const u8 isStartOTWW  = !isStartMogi && (originalMessage == 5);
        const u8 isStartItemRainWW = !isStartMogi && (originalMessage == 6);
        const u8 isStartItemRainVS = !isStartMogi && (originalMessage == 7);
        const u8 isStartItemRainTeamVS = !isStartMogi && (originalMessage == 8);

        const u8 koSetting = settings.GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_ENABLED) && destPacket->message == 0; //KO only enabled for normal GPs
        const u8 koFinal = settings.GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_FINAL) == KOSETTING_FINAL_ALWAYS;
        //invert mii setting as the first button is enabled, not disabled, so a value of 1 indicates disabled
        const u8 ottOnline = settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ONLINE);
        const bool isExtendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;

        destPacket->hostSystemContext = (ottOnline != OTTSETTING_OFFLINE_DISABLED) << PULSAR_MODE_OTT //ott
            | (ottOnline == OTTSETTING_ONLINE_FEATHER) << PULSAR_FEATHER //ott feather
            | (settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ALLOWUMTS) ^ true) << PULSAR_UMTS //ott umts
            | koSetting << PULSAR_MODE_KO
            | koFinal << PULSAR_KOFINAL
            | (!isStartMogi && (settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_ALLOW_MIIHEADS) ^ true)) << PULSAR_MIIHEADS
            | (!isStartMogi && settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_HOSTWINS)) << PULSAR_HAW
            | (isStartMogi || (settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_THUNDERCLOUD) == THUNDERCLOUD_NORMAL)) << PULSAR_THUNDERCLOUD
            | isStartVKWW << PULSAR_STARTVKWW   // OPT WW start from friend room
            | isStartOTWW  << PULSAR_STARTOTTWW  // OTT WW start from friend room
            | isStartItemRainWW << PULSAR_STARTITEMRAIN
            | (!isStartMogi && (isStartItemRainWW || isStartItemRainVS || isStartItemRainTeamVS)) << PULSAR_ITEMMODERAIN
            | isStartMogi << PULSAR_STARTMOGI
            | (isExtendedTeams && !isStartMogi && !isStartVKWW && !isStartOTWW && !isStartItemRainWW) << PULSAR_EXTENDEDTEAMS;

        OSReport("[Pulsar LOG] BeforeROOMSend: hostSystemContext=0x%08X (extendedTeams=%d)\n", destPacket->hostSystemContext, isExtendedTeams);

        u8 raceCount;
        if (koSetting == KOSETTING_ENABLED) raceCount = 0xFE;
        else if (isStartMogi) raceCount = 3;
        else switch (settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_SCROLL_GP_RACES)) {
        case(0): // 4 races
            raceCount = 3;
            break;
        case(1): // 8 races
            raceCount = 7;
            break;
        case(2): // 12 races
            raceCount = 11;
            break;
        case(3): // 24 races
            raceCount = 23;
            break;
        case(4): // 32 races
            raceCount = 31;
            break;
        case(5): // 64 races
            raceCount = 63;
            break;
        case(6): // 2 races
            raceCount = 1;
            break;
        default:
            raceCount = 3;
        }
        destPacket->raceCount = raceCount;
        WriteBlockedTracksToPacket(destPacket);
        ConvertROOMPacketToData(*destPacket);
        system->SetContext(destPacket->hostSystemContext);
        if (isExtendedTeams) {
            UI::ExtendedTeamManager::sInstance->hasFriendRoomStarted = true;
        }
    }

    const bool isExtendedTeams = Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
    const bool isUpdateTeamMessage = destPacket->messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS;
    const bool isStartVSRaceMessage = destPacket->messageType == 1 && (destPacket->message == 0 || destPacket->message == 2 || destPacket->message == 3);
    OSReport("[Pulsar LOG] BeforeROOMSend: isUpdateTeamMessage=%d, isStartVSRaceMessage=%d, isExtendedTeams=%d\n", isUpdateTeamMessage, isStartVSRaceMessage, isExtendedTeams);
    if ((isUpdateTeamMessage || (isStartVSRaceMessage && isExtendedTeams)) && sub.localAid == sub.hostAid) {
        packetHolder->packetSize = sizeof(PulROOM);
        OSReport("[Pulsar LOG] BeforeROOMSend: Setting packetSize to sizeof(PulROOM) = %d and writing teams\n", sizeof(PulROOM));
        const UI::ExtendedTeamPlayer* playerInfo = UI::ExtendedTeamManager::sInstance->GetPlayerInfo();

        memset(destPacket->extendedTeams, 0xff, sizeof(destPacket->extendedTeams));
        for (int i = 0; i < 12; ++i) {
            if (playerInfo[i].playerIdx >= 12)
                continue;

            const u8 byte = i / 2;
            const u8 shift = (i % 2) * 4;

            destPacket->extendedTeams[byte] &= ~(0x0F << shift);
            destPacket->extendedTeams[byte] |= (playerInfo[i].team & 0x0F) << shift;
        }
    }
}
kmCall(0x8065b15c, BeforeROOMSend);

kmWrite32(0x8065add0, 0x60000000);

extern "C" void RealAfterROOMReception(const RKNet::PacketHolder<PulROOM>* packetHolder, const PulROOM& src, u32 len);

asmFunc AfterROOMReception(const RKNet::PacketHolder<PulROOM>* packetHolder, const PulROOM& src, u32 len) {
    nofralloc
    mr r5, r0
    b RealAfterROOMReception
}

extern "C" void RealAfterROOMReception(const RKNet::PacketHolder<PulROOM>* packetHolder, const PulROOM& src, u32 len) {
    register RKNet::ROOMPacket* packet;
    register u32 aid;
    asm(mr packet, r28;);
    asm(mr aid, r29;);
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    Pulsar::System* system = Pulsar::System::sInstance;
    const bool isHost = sub.localAid == sub.hostAid;

    OSReport("[Pulsar LOG] AfterROOMReception: msgType=%d, len=%d, isHost=%d\n", src.messageType, len, isHost);

    //START msg sent by the host, size check should always be guaranteed in theory
    if (src.messageType == 1 && !isHost && len == sizeof(PulROOM)) {
        ConvertROOMPacketToData(src);

        // Apply host context locally on all non-host clients.
        const u32 hostContext = src.hostSystemContext;
        OSReport("[Pulsar LOG] AfterROOMReception: Setting hostContext=0x%08X on client\n", hostContext);
        system->SetContext(hostContext);

        // Sync host's blocked tracks to the client
        const u32 localBlockingCount = system->GetInfo().GetTrackBlocking();
        if (localBlockingCount > 0 && system->netMgr.lastTracks != nullptr && src.blockedTrackCount > 0) {
            const u32 copyCount = (src.blockedTrackCount < localBlockingCount) ? src.blockedTrackCount : localBlockingCount;
            for (u32 i = 0; i < copyCount; ++i) {
                u16 track = src.blockedTracks[i];
                system->netMgr.lastTracks[i] = (track == 0xFFFF) ? PULSARID_NONE : static_cast<PulsarId>(track);
            }
            system->netMgr.curBlockingArrayIdx = src.curBlockingArrayIdx % localBlockingCount;
            system->netMgr.lastGroupedTrackPlayed = src.lastGroupedTrackPlayed;
        }

        //Also exit the settings page to prevent weird graphical artefacts
        Page* topPage = SectionMgr::sInstance->curSection->GetTopLayerPage();
        PageId topId = topPage->pageId;
        if (topId == UI::SettingsPanel::id) {
            UI::SettingsPanel* panel = static_cast<UI::SettingsPanel*>(topPage);
            panel->OnBackPress(0);
        }

        if (system->IsContext(PULSAR_EXTENDEDTEAMS)) {
            OSReport("[Pulsar LOG] AfterROOMReception: PULSAR_EXTENDEDTEAMS is active! Snycing teams...\n");
            HandleExtendedTeamUpdates(src);
            UI::ExtendedTeamManager::sInstance->hasFriendRoomStarted = true;
        }
    }

    if (src.messageType == UI::ExtendedTeamManager::MSG_TYPE_UPDATE_TEAMS &&
        !isHost &&
        len == sizeof(PulROOM)) {
        OSReport("[Pulsar LOG] AfterROOMReception: MSG_TYPE_UPDATE_TEAMS received on client! Syncing teams...\n");
        HandleExtendedTeamUpdates(src);
    }

    if (isHost && src.messageType == UI::ExtendedTeamManager::MSG_TYPE_PING) {
        UI::ExtendedTeamManager::sInstance->SetActiveStatusForAID(aid);
        u8 team1 = src.message & 0x0F;
        u8 team2 = (src.message >> 4) & 0x0F;
        bool changed = false;
        if (team1 < UI::TEAM_COUNT) {
            UI::ExtendedTeamID oldTeam = UI::ExtendedTeamManager::sInstance->GetPlayerTeamByAID(aid, 0);
            if (oldTeam != team1) {
                UI::ExtendedTeamManager::sInstance->SetPlayerTeamByAID(aid, 0, static_cast<UI::ExtendedTeamID>(team1));
                changed = true;
            }
        }
        if (team2 < UI::TEAM_COUNT) {
            UI::ExtendedTeamID oldTeam = UI::ExtendedTeamManager::sInstance->GetPlayerTeamByAID(aid, 1);
            if (oldTeam != team2) {
                UI::ExtendedTeamManager::sInstance->SetPlayerTeamByAID(aid, 1, static_cast<UI::ExtendedTeamID>(team2));
                changed = true;
            }
        }
        if (changed) {
            UI::ExtendedTeamManager::sInstance->SendUpdateTeamsPacket();
        }
    } else if (!isHost && src.messageType == UI::ExtendedTeamManager::MSG_TYPE_ACK_START_RACE) {
        UI::ExtendedTeamManager::sInstance->SetDoneStatusForAID(aid);
    }

    memcpy(packet, &src, sizeof(RKNet::ROOMPacket)); //default
}
kmCall(0x8065add8, AfterROOMReception);

//Implements that setting
kmCall(0x806460B8, System::GetRaceCount);
kmCall(0x8064f51c, System::GetRaceCount);
}//namespace Network
}//namespace Pulsar
