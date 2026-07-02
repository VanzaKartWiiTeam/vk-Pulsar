#include <UI/PlayerCount.hpp>
#include <Settings/Settings.hpp>

// Callbacks that can occur during server browsing operations
typedef enum {
    sbc_serveradded,
    sbc_serverupdated,
    sbc_serverupdatefailed,
    sbc_serverdeleted,
    sbc_updatecomplete,
    sbc_queryerror,
    sbc_serverchallengereceived
} SBCallbackReason;

typedef void (*SBServerKeyEnumFn)(char* key, char* value, void* instance);

typedef void* SBServer;
typedef void* ServerBrowser;
typedef void (*ServerBrowserCallback)(ServerBrowser sb, SBCallbackReason reason,
                                      SBServer server, void* instance);
typedef void (*TableMapFn)(void* elem, void* clientData);

extern "C" ServerBrowser ServerBrowserNewA(const char* queryForGamename,
                                           const char* queryFromGamename,
                                           const char* queryFromKey,
                                           int queryFromVersion, int maxConcUpdates,
                                           int queryVersion, bool lanBrowse,
                                           ServerBrowserCallback callback, void* instance);
extern "C" int ServerBrowserLimitUpdateA(ServerBrowser sb, bool async,
                                         bool disconnectOnComplete,
                                         const unsigned char* basicFields,
                                         int numBasicFields,
                                         const char* serverFilter, int maxServers);
extern "C" int ServerBrowserCount(ServerBrowser sb);
extern "C" void ServerBrowserThink(ServerBrowser sb);
extern "C" void ServerBrowserClear(ServerBrowser sb);
extern "C" void ServerBrowserFree(ServerBrowser sb);
extern "C" SBServer ServerBrowserGetServer(ServerBrowser sb, int index);
extern "C" void TableMapSafe(void* table, TableMapFn fn, void* clientData);
extern "C" const char* SBServerGetStringValueA(SBServer server, const char* keyname,
                                               const char* def);
extern "C" int SBServerGetIntValueA(SBServer server, const char* key, int idefault);
extern "C" void qr2_register_keyA(int keyid, const char* key);
extern "C" void msleep(unsigned int msecs);
extern "C" void* gsimalloc(u32 size);
extern "C" int DWCi_QR2Startup(u32 profileID);
extern "C" void DWC_SetReportLevel(u32 level);
extern "C" void DWC_ProcessFriendsMatch();
extern "C" void DWCi_SBCallback(ServerBrowser sb, SBCallbackReason reason, SBServer server, void* instance);

typedef struct {
    SBServerKeyEnumFn EnumFn;
    void* instance;
} SBServerEnumData;

typedef struct _SBKeyValuePair {
    const char* key;
    const char* value;
} SBKeyValuePair;

static void KeyMapF(void* elem, void* clientData) {
    SBKeyValuePair* kv = (SBKeyValuePair*)elem;
    SBServerEnumData* ped = (SBServerEnumData*)clientData;
    ped->EnumFn((char*)kv->key, (char*)kv->value, ped->instance);
}

void SBServerEnumKeys(SBServer server, SBServerKeyEnumFn KeyFn, void* instance) {
    SBServerEnumData ed;
    ed.EnumFn = KeyFn;
    ed.instance = instance;
    TableMapSafe(*(void**)((u32)server + 0x18), KeyMapF, &ed);
}

void GetRegionParamsFromString(const char* region, char* outputRegion, u32& outputRegionID) {
    outputRegion[0] = '\0';
    outputRegionID = 0xffffffff;
    if (region == nullptr) return;

    char value[32];
    snprintf(value, sizeof(value), "%s", region);

    if (strcmp(value, "vs") == 0) {
        snprintf(outputRegion, 16, "%s", value);
        outputRegionID = 0x13371337;
        return;
    }

    char* delimiter = strchr(value, '_');
    if (delimiter) {
        *delimiter = '\0';
        snprintf(outputRegion, 16, "%s", value);

        char* end = nullptr;
        int regionInt = strtol(delimiter + 1, &end, 10);

        if (end != delimiter + 1) {
            outputRegionID = static_cast<u32>(regionInt);
        }
        return;
    }

    snprintf(outputRegion, 16, "%s", value);
}

static ServerBrowser playerCntSB = nullptr;
static bool isHookedRequest = false;
static bool isInternalPlayerCountRequest = false;
static float hookLocalTimer = 0.0f;
static bool hasRKNetRequestFinished = true;

static bool IsCompetitiveMatchmakingEnabled() {
    return false;
}

static const char* GetCompetitiveServerFilter(const char* serverFilter, char* expandedFilter, u32 filterSize) {
    return serverFilter;
}

static int VK_numPlayersVS = 0;
static int VK_numPlayers200cc = 0;
static int VK_numPlayersOTT = 0;
static int VK_numPlayersIR = 0;
static int VK_numPlayersBattle = 0;
static int VK_numPlayersOthers = 0;
static int VK_numPlayersTotal = 0;

void PlayerCount::GetNumbers(int& nVS, int& n200cc, int& nOTT, int& nIR, int& nBattle) {
    nVS = VK_numPlayersVS;
    n200cc = VK_numPlayers200cc;
    nOTT = VK_numPlayersOTT;
    nIR = VK_numPlayersIR;
    nBattle = VK_numPlayersBattle;
}

void PlayerCount::GetNumbersTotal(int& nTotal) {
    nTotal = VK_numPlayersTotal;
}

void sbCallback(ServerBrowser sb, SBCallbackReason reason, SBServer server, void* instance) {
    if (reason == sbc_updatecomplete) {
        int totalPlayers = 0;
        int numElse = 0;
        int VK_localVS = 0, VK_local200cc = 0, VK_localOTT = 0, VK_localIR = 0;
        int VK_localBattle = 0;
        
        u32 wwRegion = Pulsar::System::sInstance->GetInfo().GetWiimmfiRegion();

        for (int i = 0; i < ServerBrowserCount(sb); i++) {
            SBServer server = ServerBrowserGetServer(sb, i);

            char region[16];
            u32 regionID = 0xffffffff;

            const char* rk = SBServerGetStringValueA(server, "rk", "");
            GetRegionParamsFromString(rk, region, regionID);

            int numplayers = SBServerGetIntValueA(server, "numplayers", -1) + 1;
            if (strstr(region, "vs")) {
                if (regionID == wwRegion) {
                    VK_localVS += numplayers;
                } else if (regionID == 0x70) {
                    VK_local200cc += numplayers;
                } else if (regionID == 0xCD) {
                    VK_localOTT += numplayers;
                } else if (regionID == 0x71) {
                    VK_localIR += numplayers;
                } else {
                    numElse += numplayers;
                }
            } else if (strstr(region, "bt")) {
                VK_localBattle += numplayers;
            }
            totalPlayers += numplayers;
        }

        VK_numPlayersVS = VK_localVS;
        VK_numPlayers200cc = VK_local200cc;
        VK_numPlayersOTT = VK_localOTT;
        VK_numPlayersIR = VK_localIR;
        VK_numPlayersBattle = VK_localBattle;
        VK_numPlayersOthers = numElse;
        VK_numPlayersTotal = totalPlayers;

        isHookedRequest = false;
    }
}

static const u8 basicFields[] = {0x08, 0x0a, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x64, 0x65, 0x66, 0x67};

bool hasQR2Initialized = false;
int hook_QR2Startup(u32 id) {
    int res = DWCi_QR2Startup(id);

    qr2_register_keyA(0x32, "dwc_pid");
    qr2_register_keyA(0x33, "dwc_mtype");
    qr2_register_keyA(0x34, "dwc_mver");
    qr2_register_keyA(0x35, "dwc_eval");
    qr2_register_keyA(0x36, "dwc_groupid");
    qr2_register_keyA(0x37, "dwc_hoststate");
    qr2_register_keyA(0x38, "dwc_suspend");
    qr2_register_keyA(0x64, "rk");
    qr2_register_keyA(0x65, "ev");
    qr2_register_keyA(0x66, "eb");
    qr2_register_keyA(0x67, "p");

    hasQR2Initialized = true;

    return res;
}

void StartRequestTask(void* arg) {
    if (isHookedRequest) {
        playerCntSB = ServerBrowserNewA(
            DWC::MatchControl::sInstance->gameName,
            DWC::MatchControl::sInstance->gameName,
            DWC::MatchControl::sInstance->secretKey,
            0,
            20,
            1,
            false,
            sbCallback,
            nullptr);
        if (playerCntSB != nullptr) {
            isInternalPlayerCountRequest = true;
            ServerBrowserLimitUpdateA(playerCntSB, false, false, basicFields, sizeof(basicFields), "gamename = \"mariokartwii\"", 256);
            isInternalPlayerCountRequest = false;
            ServerBrowserFree(playerCntSB);
            playerCntSB = nullptr;
        }
        isInternalPlayerCountRequest = false;
        isHookedRequest = false;
        hasRKNetRequestFinished = true;
    }
}

int hook_ServerBrowserLimitUpdateA(ServerBrowser sb, bool async,
                                   bool disconnectOnComplete,
                                   const unsigned char* basicFields,
                                   int numBasicFields,
                                   const char* serverFilter, int maxServers) {
    if (!isInternalPlayerCountRequest) {
        while (isHookedRequest)
            msleep(10);

        hasRKNetRequestFinished = false;
    }

    unsigned char newFields[64];
    int newNumFields = 0;

    if (basicFields && numBasicFields > 0) {
        newNumFields = numBasicFields;
        if (newNumFields > 60) newNumFields = 60;
        for (int i = 0; i < newNumFields; i++) {
            newFields[i] = basicFields[i];
        }
    }

    bool hasEV = false;
    bool hasEB = false;
    for (int i = 0; i < newNumFields; i++) {
        if (newFields[i] == 0x65) hasEV = true;
        if (newFields[i] == 0x66) hasEB = true;
    }

    if (!hasEV) newFields[newNumFields++] = 0x65;
    if (!hasEB) newFields[newNumFields++] = 0x66;

    char expandedFilter[0x100];
    const char* updatedServerFilter = GetCompetitiveServerFilter(serverFilter, expandedFilter, sizeof(expandedFilter));

    int res = ServerBrowserLimitUpdateA(
        sb,
        async,
        disconnectOnComplete,
        newFields,
        newNumFields,
        updatedServerFilter,
        maxServers);

    return res;
}

void hook_ServerBrowserFree(ServerBrowser sb) {
    hasRKNetRequestFinished = true;
    ServerBrowserFree(sb);
}

void hook_DWC_SetReportLevel(u32 level) {
#ifndef PROD
    DWC_SetReportLevel(0xffffffff);
#else
    DWC_SetReportLevel(level);
#endif
}

void hook_Section_calc(Section* _this) {
    _this->UpdateLayers();

    hookLocalTimer += 1.0f / 60.0f;

    bool isConnectionIdle = RKNet::Controller::sInstance &&
                            RKNet::Controller::sInstance->GetConnectionState() == RKNet::CONNECTIONSTATE_IDLE;

    if (hasQR2Initialized && !isHookedRequest && hasRKNetRequestFinished && hookLocalTimer >= 5.0f &&
        SectionMgr::sInstance->curSection->pages[Pages::Globe::id] && DWC::MatchControl::sInstance && isConnectionIdle) {
        isHookedRequest = true;
        hookLocalTimer = 0.0f;
        Pulsar::System::sInstance->taskThread->Request(StartRequestTask, nullptr, 0);
    }

    if (!SectionMgr::sInstance->curSection->pages[Pages::Globe::id]) {
        isHookedRequest = false;
        hasRKNetRequestFinished = true;
    }
}

kmCall(0x800d0584, hook_QR2Startup);
kmCall(0x800d413c, hook_QR2Startup);
kmCall(0x800d5484, hook_QR2Startup);
kmCall(0x800d56bc, hook_QR2Startup);
kmCall(0x800d605c, hook_QR2Startup);
kmCall(0x800d62c0, hook_QR2Startup);

kmCall(0x800db908, hook_ServerBrowserLimitUpdateA);
kmCall(0x800d1058, hook_ServerBrowserFree);
kmCall(0x800d839c, hook_ServerBrowserFree);
kmCall(0x800db46c, hook_ServerBrowserFree);
kmCall(0x80658be8, hook_DWC_SetReportLevel);
kmCall(0x80622514, hook_Section_calc);
