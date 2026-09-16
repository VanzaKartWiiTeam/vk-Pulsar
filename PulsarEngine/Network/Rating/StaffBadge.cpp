#include <Network/Rating/StaffBadge.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>

namespace Pulsar {
namespace PointRating {
namespace Staff {

struct Member {
    u64 friendCode;  // as shown in game without the dashes: 1234-5678-9012 -> 123456789012ULL.
                     // No leading zeros: 0568-3457-5116 -> 56834575116ULL, since a literal
                     // starting with 0 is octal and would silently match nobody.
    Role role;
};

/*
    The contributors. The profile ID is the low 32 bits of the friend code, so the code
    can be copied straight from the game; one person, one role.
*/
static const Member sMembers[] = {
    { 426201762344ULL, ROLE_LEADER },  // 4262-0176-2344, creator
    { 164208757382ULL, ROLE_LEADER },
    { 20202ULL, ROLE_CREATIVE_DIRECTOR },
    { 1000000065ULL, ROLE_CREATIVE_DIRECTOR },
    { 542165879414ULL, ROLE_MODERATOR },
    { 417611827933ULL, ROLE_MODERATOR },
    { 31064771154ULL, ROLE_MODERATOR },
    { 10010100ULL, ROLE_DEVELOPER },
    { 443381631713ULL, ROLE_DEVELOPER },
    { 271582939761ULL, ROLE_STAFF_GHOST },
    { 305544760230ULL, ROLE_STAFF_GHOST },
    { 0ULL, ROLE_NONE }  // terminator, keeps the array non-empty
};

Role GetForProfileId(u32 profileId) {
    if (profileId == 0) return ROLE_NONE;
    for (u32 i = 0; i < sizeof(sMembers) / sizeof(sMembers[0]); ++i) {
        if (sMembers[i].role == ROLE_NONE) continue;
        if ((u32)(sMembers[i].friendCode & 0xFFFFFFFFULL) == profileId) return sMembers[i].role;
    }
    return ROLE_NONE;
}

Role GetLocal(u8 playerIndexOnConsole) {
    if (playerIndexOnConsole != 0) return ROLE_NONE;
    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || rksys->curLicenseId < 0 || rksys->curLicenseId >= (int)Config::MAX_LICENSES) {
        return ROLE_NONE;
    }
    return GetForProfileId((u32)rksys->licenses[rksys->curLicenseId].dwcAccUserData.gsProfileId);
}

Role GetRemote(u8 aid, u8 playerIndexOnConsole) {
    if (aid >= 12 || playerIndexOnConsole != 0) return ROLE_NONE;
    const DWC::MatchControl* match = DWC::MatchControl::sInstance;
    if (match == nullptr) return ROLE_NONE;
    for (u32 i = 0; i < 32; ++i) {
        if (match->nodes[i].aid == aid && match->nodes[i].pid != 0) {
            return GetForProfileId(match->nodes[i].pid);
        }
    }
    return ROLE_NONE;
}

// Same resolution as Rank::GetForPlayer, so the two badges always land on the same player.
Role GetForPlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr || playerId >= 12) return ROLE_NONE;

    const RacedataScenario& scenario = racedata->racesScenario;
    if (playerId >= scenario.playerCount) return ROLE_NONE;

    const RacedataPlayer& player = scenario.players[playerId];

    if (player.playerType == PLAYER_REAL_LOCAL) {
        u8 slot = 0;
        for (u8 i = 0; i < playerId; ++i) {
            if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) ++slot;
        }
        return GetLocal(slot);
    }

    if (player.playerType == PLAYER_REAL_ONLINE) {
        const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
        if (ctrl == nullptr) return ROLE_NONE;

        const u8 aid = ctrl->aidsBelongingToPlayerIds[playerId];
        u8 slot = 0;
        for (u8 i = 0; i < playerId; ++i) {
            if (ctrl->aidsBelongingToPlayerIds[i] == aid) ++slot;
        }
        return GetRemote(aid, slot);
    }

    return ROLE_NONE;
}

wchar_t GetBadgeGlyph(Role role) {
    if (role == ROLE_NONE || role >= ROLE_COUNT) return 0;
    return (wchar_t)(Config::STAFF_GLYPH_BASE + role);
}

}  // namespace Staff
}  // namespace PointRating
}  // namespace Pulsar
