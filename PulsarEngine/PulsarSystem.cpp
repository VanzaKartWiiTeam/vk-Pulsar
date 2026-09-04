#include <core/RK/RKSystem.hpp>
#include <core/nw4r/ut/Misc.hpp>
#include <MarioKartWii/Scene/RootScene.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <VanzaKartChannel.hpp>
#include <Extensions/LECODE/LECODEMgr.hpp>
#include <Gamemodes/KO/KOMgr.hpp>
#include <Gamemodes/KO/KOHost.hpp>
#include <Gamemodes/OnlineTT/OnlineTT.hpp>
#include <Settings/Settings.hpp>
#include <Config.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <core/egg/DVD/DvdRipper.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <Debug/Debug.hpp>
namespace Pulsar {

System* System::sInstance = nullptr;
System::Inherit* System::inherit = nullptr;

void System::CreateSystem() {
    if(sInstance != nullptr) return;
    EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
    const EGG::Heap* prev = heap->BecomeCurrentHeap();
    System* system;
    if(inherit != nullptr) {
        system = inherit->create();
    }
    else system = new System();
    System::sInstance = system;
    UI::ExtendedTeamManager::CreateInstance(new UI::ExtendedTeamManager());
    ConfigFile& conf = ConfigFile::LoadConfig();
    system->Init(conf);
    prev->BecomeCurrentHeap();
    conf.Destroy();
}
//kmCall(0x80543bb4, System::CreateSystem);
BootHook CreateSystem(System::CreateSystem, 0);

/*
The stack used to be 0x4000. EGG::TaskThread::Create allocates the object first and its stack
immediately after, so the stack grows down straight into the object it belongs to: overrunning it
by as little as 0x50 bytes lands on the embedded OS::MessageQueue at +0x0C and corrupts the
receive queue's head. The symptom is a DSI inside OSWakeupThread the next time anything posts a
task, long after the task that actually overflowed has finished, which is why it looked like a
crash in unrelated menu code. 16K is not much for file IO with path buffers on the stack.
*/
System::System() :
    heap(RKSystem::mInstance.EGGSystem), taskThread(EGG::TaskThread::Create(8, 0, 0x10000, this->heap)),
    //Modes
    koMgr(nullptr) {
    customBmgs.bmgFile = nullptr;
    customBmgs.info = nullptr;
    customBmgs.data = nullptr;
    customBmgs.str1Block = nullptr;
    customBmgs.messageIds = nullptr;
    rawBmg = nullptr;
}

static s32 sDolphinProbe = 1;
static s32 sRiivoProbe = 1;

void System::Init(const ConfigFile& conf) {
    IOType type = IOType_ISO;
    bool isDolphin = false;
    s32 ret = IO::OpenFix("/dev/dolphin", IOS::MODE_NONE);
    sDolphinProbe = ret;
    if (ret >= 0) {
        isDolphin = true;
        IOS::Close(ret);
    }

    ret = IO::OpenFix("file", IOS::MODE_NONE);
    sRiivoProbe = ret;
    if (ret >= 0 && !IsNewChannel()) {
        type = IOType_RIIVO;
        IOS::Close(ret);
    }
    else if (IsNewChannel() && !isDolphin) {
        type = IOType_SD;
    }
    else if (isDolphin) {
        type = IOType_DOLPHIN;
    }

    strncpy(this->modFolderName, conf.header.modFolderName, IOS::ipcMaxFileName);

    //InitInstances
    CupsConfig::sInstance = new CupsConfig(conf.GetSection<CupsHolder>());
    this->info.Init(conf.GetSection<InfoHolder>().info);
    this->InitIO(type);
    this->InitSettings(&conf.GetSection<CupsHolder>().trophyCount[0]);

    if (IsNewChannel()) {
        NewChannel_Init();
    }


    //Initialize last selected cup and courses
    const PulsarCupId last = Settings::Mgr::sInstance->GetSavedSelectedCup();
    CupsConfig* cupsConfig = CupsConfig::sInstance;
    cupsConfig->SetLayout();
    if(last != -1 && cupsConfig->IsValidCup(last) && cupsConfig->GetTotalCupCount() > 8) {
        cupsConfig->lastSelectedCup = last;
        cupsConfig->SetSelected(cupsConfig->ConvertTrack_PulsarCupToTrack(last, 0));
        cupsConfig->lastSelectedCupButtonIdx = last & 1;
    }

        static char* pulMagic = reinterpret_cast<char*>(0x800017CC);
        strcpy(pulMagic, "PUL2");

    //Track blocking 
    u32 trackBlocking = this->info.GetTrackBlocking();
    this->netMgr.lastTracks = new PulsarId[trackBlocking];
    for(int i = 0; i < trackBlocking; ++i) this->netMgr.lastTracks[i] = PULSARID_NONE;
    const BMGHeader* const confBMG = &conf.GetSection<PulBMG>().header;
    this->rawBmg = EGG::Heap::alloc<BMGHeader>(confBMG->fileLength, 0x4, this->heap);
    memcpy(this->rawBmg, confBMG, confBMG->fileLength);
    this->customBmgs.Init(*this->rawBmg);
    this->AfterInit();
}

//IO
#define PULSAR_IO_SELFTEST 1

#if PULSAR_IO_SELFTEST
static const char* IOTypeName(IOType type) {
    switch(type) {
        case IOType_RIIVO: return "RIIVO";
        case IOType_ISO: return "ISO (no writable backend!)";
        case IOType_DOLPHIN: return "DOLPHIN";
        case IOType_SD: return "SD";
    }
    return "unknown";
}

static void IOSelfTest(IO* io, const char* modFolder) {
    char path[IOS::ipcMaxPath];
    snprintf(path, IOS::ipcMaxPath, "%s/IOTest.pul", modFolder);

    const u32 magic = 'PIOT';
    alignas(0x20) u32 buffer = magic;
    const char* step = nullptr;

    if(!io->CreateAndOpen(path, FILE_MODE_READ_WRITE)) step = "CreateAndOpen";
    else {
        if(io->Overwrite(sizeof(u32), &buffer) != sizeof(u32)) step = "Overwrite";
        io->Close();
    }
    if(step == nullptr) {
        buffer = 0;
        if(!io->OpenFile(path, FILE_MODE_READ)) step = "OpenFile (re-read)";
        else {
            if(io->Read(sizeof(u32), &buffer) != sizeof(u32)) step = "Read";
            else if(buffer != magic) step = "compare (the file is empty)";
            io->Close();
        }
    }
    if(step != nullptr) {
        s32 retryRiivo = IO::OpenFix("file", IOS::MODE_NONE);
        if(retryRiivo >= 0) IOS::Close(retryRiivo);

        char message[0x200];
        snprintf(message, sizeof(message),
                 "Pulsar: IO SELF-TEST FAILED\n\n"
                 "backend: %s\n"
                 "step: %s\n"
                 "path: %s\n"
                 "fallback: %s\n"
                 "\"file\" at boot: %d\n"
                 "\"file\" now: %d\n"
                 "/dev/dolphin: %d",
                 IOTypeName(io->type), step, path,
                 BootHook::executedFromFallback ? "yes" : "no",
                 sRiivoProbe, retryRiivo, sDolphinProbe);
        Debug::FatalError(message);
    }
}
#endif

#pragma suppress_warnings on
void System::InitIO(IOType type) const {

    IO* io = IO::CreateInstance(type, this->heap, this->taskThread);
    bool ret;
    if(io->type == IOType_DOLPHIN) ret = ISFS::CreateDir("/shared2/Pulsar", 0, IOS::MODE_READ_WRITE, IOS::MODE_READ_WRITE, IOS::MODE_READ_WRITE);
    const char* modFolder = this->GetModFolder();
    ret = io->CreateFolder(modFolder);
    if(!ret && io->type == IOType_DOLPHIN) {
        char path[0x100];
        snprintf(path, 0x100, "Unable to automatically create a folder for this CT distribution\nPlease create a Pulsar folder in Dolphin Emulator/Wii/shared2", modFolder);
        Debug::FatalError(path);
    }
    char ghostPath[IOS::ipcMaxPath];
    snprintf(ghostPath, IOS::ipcMaxPath, "%s%s", modFolder, "/Ghosts");
    io->CreateFolder(ghostPath);
#if PULSAR_IO_SELFTEST
    IOSelfTest(io, modFolder);
#endif
}
#pragma suppress_warnings reset

void System::InitSettings(const u16* totalTrophyCount) const {
    Settings::Mgr* settings = new (this->heap) Settings::Mgr;
    char path[IOS::ipcMaxPath];
    snprintf(path, IOS::ipcMaxPath, "%s/%s", this->GetModFolder(), "Settings.pul");
    settings->Init(totalTrophyCount, path); //params
    Settings::Mgr::sInstance = settings;
}

extern "C" void OSReport(const char* format, ...);

void System::UpdateContext() {
    OSReport("[Pulsar LOG] UpdateContext: start\n");
    const RacedataSettings& racedataSettings = Racedata::sInstance->menusScenario.settings;
    this->ottMgr.Reset();
    const Settings::Mgr& settings = Settings::Mgr::Get();
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const bool isOnlineRoomActive = controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN;
    
    bool isFroom = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST || controller->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    bool isRegionalRoom = isOnlineRoomActive && (controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL || controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL || controller->roomType == RKNet::ROOMTYPE_BT_REGIONAL);
    bool isPublicOnlineRoom = isOnlineRoomActive && !isFroom && controller->roomType != RKNet::ROOMTYPE_NONE;

    bool isCT = true;
    bool isHAW = false;
    bool isKO = false;
    bool isOTT = false;
    bool isMiiHeads = settings.GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_RADIO_MII);
    bool isThunderCloud = settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_THUNDERCLOUD) == THUNDERCLOUD_NORMAL && isFroom;
    bool isStartVKWW = false;
    bool isStartOTTWW = false;
    bool isStartItemRain = false;
    bool isStartMogi = false;
    bool isItemRainActive = false;
    bool isItemStormActive = false;
    bool isCountdown = false;
    bool isAllItemsCanLand = false;
    bool isKOFinal = settings.GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_FINAL) == KOSETTING_FINAL_ALWAYS;
    bool isExtendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
    bool isVR = false; //ranked friend room: decided by the host, obeyed by everyone

    const GameMode mode = racedataSettings.gamemode;
    Network::Mgr& netMgr = this->netMgr;
    const u32 sceneId = GameScene::GetCurrent()->id;

    OSReport("[Pulsar LOG] UpdateContext: sceneId=%d, roomType=%d, mode=%d, isExtendedTeams=%d\n", sceneId, controller->roomType, mode, isExtendedTeams);


    bool is200 = racedataSettings.engineClass == CC_100 && this->info.Has200cc();
    bool isFeather = this->info.HasFeather();
    bool isUMTs = this->info.HasUMTs();
    bool isMegaTC = this->info.HasMegaTC();
    u32 newContext = 0;
    if(sceneId != SCENE_ID_GLOBE && controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN) {
        switch(controller->roomType) {
            case(RKNet::ROOMTYPE_VS_REGIONAL):
            case(RKNet::ROOMTYPE_JOINING_REGIONAL):
                isOTT = netMgr.ownStatusData == true;
                break;
            case(RKNet::ROOMTYPE_FROOM_HOST):
            case(RKNet::ROOMTYPE_FROOM_NONHOST): {
                isCT = mode != MODE_BATTLE && mode != MODE_PUBLIC_BATTLE && mode != MODE_PRIVATE_BATTLE;
                const bool isHost = controller->roomType == RKNet::ROOMTYPE_FROOM_HOST;
                if (isHost) {
                    const u8 ottOnline = settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ONLINE);
                    const u8 koSetting = settings.GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_ENABLED);
                    const u8 koFinal = settings.GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_FINAL) == KOSETTING_FINAL_ALWAYS;
                    const bool isLocalExtendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;
                    /*
                        Every term below has to carry the same mogi guard PulROOM puts in the
                        packet. This is a rebuild from the host's own settings, so anything left
                        ungated came back the moment the scene changed: the clients kept the room
                        the host had announced while the host itself drifted back to its settings.
                    */
                    const bool isMogi = (this->context & (1 << PULSAR_STARTMOGI)) != 0;

                    newContext = (!isMogi && ottOnline != OTTSETTING_OFFLINE_DISABLED) << PULSAR_MODE_OTT
                        | (ottOnline == OTTSETTING_ONLINE_FEATHER) << PULSAR_FEATHER
                        | (settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ALLOWUMTS) ^ true) << PULSAR_UMTS
                        | (!isMogi && koSetting) << PULSAR_MODE_KO
                        | koFinal << PULSAR_KOFINAL
                        | (!isMogi && (settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_ALLOW_MIIHEADS) ^ true)) << PULSAR_MIIHEADS
                        | (!isMogi && settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_HOSTWINS)) << PULSAR_HAW
                        | (isMogi || settings.GetSettingValue(Settings::SETTINGSTYPE_HOST, SETTINGHOST_RADIO_THUNDERCLOUD) == THUNDERCLOUD_NORMAL) << PULSAR_THUNDERCLOUD
                        | (!isMogi && isLocalExtendedTeams) << PULSAR_EXTENDEDTEAMS
                        //Same term PulROOM puts in the packet, so the host's own context matches
                        //what its clients receive.
                        | (!isMogi && settings.GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_SCROLL_ITEMMODE) == RACESETTING_ITEMMODE_ITEMRAIN) << PULSAR_ITEMMODERAIN
                        | (!isMogi && settings.GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_SCROLL_ITEMMODE) == RACESETTING_ITEMMODE_COUNTDOWN) << PULSAR_MODE_COUNTDOWN;
                    /*
                        A mogi is decided once, when the room starts: PulROOM writes the bit into
                        the ROOM packet and SetContext applies it. The host rebuilds its context
                        from its own settings at every scene change and none of the terms above
                        produce that bit, so without carrying it over the host lost the mogi after
                        the first race while the clients, which read hostContext, kept it - and
                        the 150cc lock in DecideCC, which only the host runs, went with it.

                        Only this bit. The three worldwide-start ones must stay one-shot: the froom
                        to worldwide conversion clears the context on purpose once it has used them.
                    */
                    newContext |= this->context & (1 << PULSAR_STARTMOGI);

                    netMgr.hostContext = newContext;
                    OSReport("[Pulsar LOG] UpdateContext: Host compiled newContext=0x%08X (localTeams=%d)\n", newContext, isLocalExtendedTeams);
                } else {
                    newContext = netMgr.hostContext;
                    OSReport("[Pulsar LOG] UpdateContext: Client read hostContext=0x%08X\n", newContext);
                }
                isHAW = newContext & (1 << PULSAR_HAW);
                isKO = newContext & (1 << PULSAR_MODE_KO);
                isOTT = newContext & (1 << PULSAR_MODE_OTT);
                isMiiHeads = newContext & (1 << PULSAR_MIIHEADS);
                isThunderCloud = newContext & (1 << PULSAR_THUNDERCLOUD);
                isStartVKWW = newContext & (1 << PULSAR_STARTVKWW);
                isStartOTTWW = newContext & (1 << PULSAR_STARTOTTWW);
                isStartItemRain = newContext & (1 << PULSAR_STARTITEMRAIN);
                isStartMogi = newContext & (1 << PULSAR_STARTMOGI);
                isItemRainActive = newContext & (1 << PULSAR_ITEMMODERAIN);
                isItemStormActive = newContext & (1 << PULSAR_ITEMMODESTORM);
                isCountdown = newContext & (1 << PULSAR_MODE_COUNTDOWN);
                isAllItemsCanLand = newContext & (1 << PULSAR_ALLITEMSCANLAND);
                isKOFinal = newContext & (1 << PULSAR_KOFINAL);
                isExtendedTeams = newContext & (1 << PULSAR_EXTENDEDTEAMS);
                isVR = newContext & (1 << PULSAR_VR);
                if(isOTT) {
                    isUMTs &= newContext & (1 << PULSAR_UMTS);
                    isFeather &= newContext & (1 << PULSAR_FEATHER);
                }
                break;
            }
            default: isCT = true;
        }
        /*
            In a public room the mode is not in any packet: it is the region the player
            matchmade in, and after Region.cpp started publishing REGIONID in the "rk" key
            everyone in the room is guaranteed to have picked the same one. 200cc is missing
            on purpose - it rides on the engine class the host puts in the SELECT packet
            (see DecideCC), which is what PULSAR_200 is derived from just above.
        */
        if (isRegionalRoom) {
            switch (Network::REGIONID) {
                case Network::REGION_OTT:
                    isOTT = true;
                    break;
                case Network::REGION_ITEMRAIN:
                    isItemRainActive = true;
                    break;
            }
        }
    }
    else {
        const u8 ottOffline = settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_OFFLINE);
        isOTT = (mode == MODE_GRAND_PRIX || mode == MODE_VS_RACE) ? (ottOffline != OTTSETTING_OFFLINE_DISABLED) : false; //offlineOTT
        if(isOTT) {
            isFeather &= (ottOffline == OTTSETTING_OFFLINE_FEATHER);
            isUMTs &= ~settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ALLOWUMTS);
        }
        //Offline VS reads the item mode straight from the Race settings scroller. Note that
        //isItemRainActive only reaches `context` inside the isCT block below, so the mode needs
        //custom tracks enabled to take effect.
        if(mode == MODE_VS_RACE) {
            const u8 raceMode = settings.GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_SCROLL_ITEMMODE);
            isItemRainActive = raceMode == RACESETTING_ITEMMODE_ITEMRAIN;
            isCountdown = raceMode == RACESETTING_ITEMMODE_COUNTDOWN;
        }
    }
    this->netMgr.hostContext = newContext;

    // PULSAR_VR sits outside the CT-only block: a ranked battle room has isCT false and
    // still has to count towards BR.
    u32 context = (isCT << PULSAR_CT) | (isHAW << PULSAR_HAW) | (isMiiHeads << PULSAR_MIIHEADS) |
                  (isVR << PULSAR_VR);
    if(isCT) { //contexts that should only exist when CTs are on
        context |= (is200 << PULSAR_200) | (isFeather << PULSAR_FEATHER) | (isUMTs << PULSAR_UMTS) | (isMegaTC << PULSAR_MEGATC) | (isOTT << PULSAR_MODE_OTT) | (isKO << PULSAR_MODE_KO)
            | (isThunderCloud << PULSAR_THUNDERCLOUD)
            | (isStartVKWW << PULSAR_STARTVKWW)
            | (isStartOTTWW << PULSAR_STARTOTTWW)
            | (isStartItemRain << PULSAR_STARTITEMRAIN)
            | (isStartMogi << PULSAR_STARTMOGI)
            | (isItemRainActive << PULSAR_ITEMMODERAIN)
            | (isItemStormActive << PULSAR_ITEMMODESTORM)
            | (isAllItemsCanLand << PULSAR_ALLITEMSCANLAND)
            | (isKOFinal << PULSAR_KOFINAL)
            | (isExtendedTeams << PULSAR_EXTENDEDTEAMS)
            | (isCountdown << PULSAR_MODE_COUNTDOWN);
    }
    this->context = context;
    OSReport("[Pulsar LOG] UpdateContext: final context=0x%08X (extendedTeams=%d)\n", context, isExtendedTeams);

    //Create temp instances if needed:
    /*
    if(sceneId == SCENE_ID_RACE) {
        if(this->lecodeMgr == nullptr) this->lecodeMgr = new (this->heap) LECODE::Mgr;
    }
    else if(this->lecodeMgr != nullptr) {
        delete this->lecodeMgr;
        this->lecodeMgr = nullptr;
    }
    */

    if(isKO) {
        if(sceneId == SCENE_ID_MENU && SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber == -1) this->koMgr = new (this->heap) KO::Mgr; //create komgr when loading the select phase of the 1st race of a froom
    }
    if(!isKO && this->koMgr != nullptr || isKO && sceneId == SCENE_ID_GLOBE) {
        delete this->koMgr;
        this->koMgr = nullptr;
    }

    if (isStartMogi) {
        Racedata::sInstance->menusScenario.settings.modeFlags &= ~0x2;
        Racedata::sInstance->racesScenario.settings.modeFlags &= ~0x2;
    }
}

s32 System::OnSceneEnter(Random& random) {
    System* self = System::sInstance;
    if (self != nullptr) {
        self->UpdateContext();
        if(self->IsContext(PULSAR_MODE_OTT)) OTT::AddGhostToVS();
        if(self->IsContext(PULSAR_HAW) && self->IsContext(PULSAR_MODE_KO) && GameScene::GetCurrent()->id == SCENE_ID_RACE && SectionMgr::sInstance->sectionParams->onlineParams.currentRaceNumber > 0) {
            KO::HAWChangeData();
        }
    }
    return random.NextLimited(8);
}
kmCall(0x8051ac40, System::OnSceneEnter);

asmFunc System::GetRaceCount() {
    ASM(
        nofralloc;
    lis r5, sInstance@ha;
    lwz r5, sInstance@l(r5);
    lbz r0, System.netMgr.racesPerGP(r5);
    blr;
        )
}

asmFunc System::GetNonTTGhostPlayersCount() {
    ASM(
        nofralloc;
    lis r12, sInstance@ha;
    lwz r12, sInstance@l(r12);
    lbz r29, System.nonTTGhostPlayersCount(r12);
    blr;
        )
}

//Unlock Everything Without Save (_tZ)
kmWrite32(0x80549974, 0x38600001);

//Skip ESRB page
kmRegionWrite32(0x80604094, 0x4800001c, 'E');

// VanzaKart WWFC pack identification
#ifdef BETA
kmWrite32(0x800017D0, 204);   // pack_id
#else
kmWrite32(0x800017D0, 104);   // pack_id
#endif
kmWrite32(0x800017D4, 131);      // pack_version

const char System::pulsarString[] = "/Pulsar";
const char System::CommonAssets[] = "/CommonAssets.szs";
const char System::breff[] = "/Effect/Pulsar.breff";
const char System::breft[] = "/Effect/Pulsar.breft";
const char* System::ttModeFolders[] ={ "150", "200", "150F", "200F" };

}//namespace Pulsar