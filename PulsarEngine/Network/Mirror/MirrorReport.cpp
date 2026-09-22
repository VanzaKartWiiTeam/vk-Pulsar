#include <Network/Mirror/MirrorReport.hpp>
#include <Network/Mirror/MirrorConfig.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>
#include <core/rvl/DWC/DWCGHTTP.hpp>
#include <include/c_stdio.h>
#include <include/c_string.h>

namespace Pulsar {
namespace Mirror {

extern "C" void OSReport(const char* format, ...);

static const u32 MAX_PLAYERS = 12;

struct Capture {
    PlayerRecord players[MAX_PLAYERS];
    u32 count;
    u32 regionId;
    u32 roomId;      // group id of the room, the key the server dedups on
    u32 reporterPid; // which console sent this copy
    bool isBattle;
    bool isVR;
};

static Capture sCapture;
static bool sPending = false;
static u32 sAge = 0;

// One request at a time. A second race cannot finish before the previous report is
// answered in practice, but a stalled request must not leak the buffer to a new one.
static bool sInFlight = false;
static char sBody[Config::REPORT_BUFFER_SIZE];

bool IsEnabled() {
    return MIRROR_ENABLED != 0;
}

/*
    Player index -> profile ID, by way of the aid. Same two step resolution the staff
    badge uses, so a player who shows a badge and a player who shows up in the report are
    always the same person.
*/
static u32 ResolveProfileId(u32 playerId) {
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    if (ctrl == nullptr || playerId >= MAX_PLAYERS) return 0;

    const u8 aid = ctrl->aidsBelongingToPlayerIds[playerId];
    if (aid >= MAX_PLAYERS) return 0;

    const DWC::MatchControl* match = DWC::MatchControl::sInstance;
    if (match == nullptr) return 0;

    for (u32 i = 0; i < 32; ++i) {
        if (match->nodes[i].aid == aid && match->nodes[i].pid != 0) return match->nodes[i].pid;
    }
    return 0;
}

static u32 GetOwnProfileId() {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || rksys->curLicenseId >= 4) return 0;
    return (u32)rksys->licenses[rksys->curLicenseId].dwcAccUserData.gsProfileId;
}

static u32 GetRoomId() {
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    if (ctrl == nullptr) return 0;
    return ctrl->subs[ctrl->currentSub].groupId;
}

void CaptureRace(const PlayerRecord* players, u32 count, bool isBattle, bool isVR, u32 regionId) {
    if (!IsEnabled() || players == nullptr || count == 0) return;
    if (count > MAX_PLAYERS) count = MAX_PLAYERS;

    memcpy(sCapture.players, players, count * sizeof(PlayerRecord));
    // Resolved here rather than at the call site, so the rating code stays clear of DWC.
    for (u32 i = 0; i < count; ++i) sCapture.players[i].profileId = ResolveProfileId(i);
    sCapture.count = count;
    sCapture.regionId = regionId;
    sCapture.roomId = GetRoomId();
    sCapture.reporterPid = GetOwnProfileId();
    sCapture.isBattle = isBattle;
    sCapture.isVR = isVR;

    // A capture that never got flushed is replaced rather than queued: the newer race is
    // the one worth having, and a queue here would mean holding network state across a
    // disconnect.
    sPending = true;
    sAge = 0;
}

/*
    Every console in the room reports the same race, so the server sees up to twelve
    copies of it. That is intentional - a single reporter would lose the race whenever
    that one console dropped - and it is why roomId and the player set are in the body:
    the server dedups on them and keeps whichever copy arrives first.
*/
static int BuildBody() {
    int len = snprintf(sBody, sizeof(sBody),
                       "{\"v\":%lu,\"pack\":%lu,\"ver\":%lu,\"room\":%lu,\"by\":%lu,"
                       "\"region\":%lu,\"mode\":\"%s\",\"players\":[",
                       (unsigned long)Config::REPORT_FORMAT_VERSION,
                       (unsigned long)*(volatile u32*)0x800017D0,
                       (unsigned long)*(volatile u32*)0x800017D4,
                       (unsigned long)sCapture.roomId,
                       (unsigned long)sCapture.reporterPid,
                       (unsigned long)sCapture.regionId,
                       sCapture.isBattle ? "bt" : (sCapture.isVR ? "vr" : "none"));
    if (len < 0 || (u32)len >= sizeof(sBody)) return -1;

    for (u32 i = 0; i < sCapture.count; ++i) {
        const PlayerRecord& p = sCapture.players[i];
        if (p.profileId == 0) continue;  // unresolved player, the server would drop it anyway

        const int written = snprintf(sBody + len, sizeof(sBody) - len,
                                     "%s{\"pid\":%lu,\"pos\":%u,\"b\":%u,\"a\":%u,\"sc\":%u}",
                                     sBody[len - 1] == '[' ? "" : ",",
                                     (unsigned long)p.profileId, p.position,
                                     p.ratingBefore, p.ratingAfter, p.battleScore);
        if (written < 0 || (u32)(len + written) >= sizeof(sBody)) return -1;
        len += written;
    }

    const int tail = snprintf(sBody + len, sizeof(sBody) - len, "]}");
    if (tail < 0 || (u32)(len + tail) >= sizeof(sBody)) return -1;
    return len + tail;
}

static void OnReportAnswered(const void* body, int length, DWC::GHTTPResult result, void* param) {
    sInFlight = false;
    // Nothing to do with the reply: the report is fire and forget by design, because a
    // failed mirror must never hold up or alter the player's session. It is logged so a
    // dead endpoint is visible in a crash dump rather than silent.
    if (result != DWC::DWCGHTTPSuccess) {
        OSReport("[VK MIRROR] race report failed, result=%d\n", (int)result);
    }
}

static bool SendPending() {
    if (sInFlight) return false;

    const int len = BuildBody();
    if (len <= 0) {
        OSReport("[VK MIRROR] could not serialise the race report\n");
        return false;
    }

    GHTTP::Post post;
    DWC::GHTTPNewPost(&post);
    if (!GHTTP::PostAddFileFromMemoryA(&post, "race", sBody, len, "race.json", "application/json")) {
        return false;
    }

    const int request = DWC::PostGHTTPData(Config::RACE_REPORT_URL, &post, OnReportAnswered, nullptr);
    if (request < 0) return false;

    sInFlight = true;
    return true;
}

/*
    Flush point. The first section load after a finish is the results screen, where the
    race itself is over and the peer traffic has stopped - which is the whole reason the
    send waits until here instead of going out from UpdatePoints.
*/
static void FlushOnSectionLoad() {
    if (!IsEnabled() || !sPending) return;

    if (++sAge > Config::CAPTURE_MAX_AGE_SECTIONS) {
        sPending = false;
        return;
    }

    if (SendPending()) sPending = false;
}
static SectionLoadHook mirrorFlushHook(FlushOnSectionLoad);

}  // namespace Mirror
}  // namespace Pulsar
