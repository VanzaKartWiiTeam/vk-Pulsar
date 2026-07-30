#include <Network/Rating/RankManager.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/RatingStorage.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace PointRating {
namespace Rank {

static RankId sRemoteRanks[12][2];

RankId Resolve(RatingValue rating, RankId currentRank) {
    RankId rank = (currentRank <= Config::MAX_RANK) ? currentRank : Config::MAX_RANK;

    // Promote while the rating clears the next threshold.  Promotions never reset it.
    while (rank < Config::MAX_RANK && rating >= (float)(rank + 1) * Config::RANK_STEP) ++rank;

    // Demote only once the rating drops a full margin below the current threshold.
    while (rank > 0 && rating < (float)rank * Config::RANK_STEP - Config::DERANK_MARGIN) --rank;

    return rank;
}

RankId GetLocal(u32 licenseId) {
    return Storage::GetRank(licenseId);
}

RankId GetRemote(u8 aid, u8 playerIndexOnConsole) {
    if (aid >= 12 || playerIndexOnConsole >= 2) return 0;
    const RankId rank = sRemoteRanks[aid][playerIndexOnConsole];
    return (rank <= Config::MAX_RANK) ? rank : 0;
}

void CacheRemote(u8 aid, const u8 ranks[2]) {
    if (aid >= 12 || ranks == nullptr) return;
    for (u8 i = 0; i < 2; ++i) {
        sRemoteRanks[aid][i] = (ranks[i] <= Config::MAX_RANK) ? ranks[i] : 0;
    }
}

void ResetRemote() {
    memset(sRemoteRanks, 0, sizeof(sRemoteRanks));
}

RankId GetForPlayer(u8 playerId) {
    const Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr || playerId >= 12) return 0;

    const RacedataScenario& scenario = racedata->racesScenario;
    if (playerId >= scenario.playerCount) return 0;

    const RacedataPlayer& player = scenario.players[playerId];

    if (player.playerType == PLAYER_REAL_LOCAL) {
        // Both local players share the licence and therefore the same rank, so the
        // second one is shown too instead of being left blank.
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (rksys != nullptr && rksys->curLicenseId < (int)Config::MAX_LICENSES) {
            return GetLocal(rksys->curLicenseId);
        }
        return 0;
    }

    if (player.playerType == PLAYER_REAL_ONLINE) {
        const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
        if (ctrl == nullptr) return 0;

        const u8 aid = ctrl->aidsBelongingToPlayerIds[playerId];
        u8 slot = 0;
        for (u8 i = 0; i < playerId; ++i) {
            if (ctrl->aidsBelongingToPlayerIds[i] == aid) ++slot;
        }
        return GetRemote(aid, slot);
    }

    return 0;
}

wchar_t GetBadgeGlyph(RankId rank) {
    if (rank == 0 || rank > Config::MAX_RANK) return 0;
    return (wchar_t)(Config::BADGE_GLYPH_BASE + rank);
}

bool PrefixWithBadge(RankId rank, const wchar_t* name, wchar_t* dst, u32 dstLen) {
    const wchar_t glyph = GetBadgeGlyph(rank);
    if (glyph == 0 || name == nullptr || dst == nullptr || dstLen < 4) return false;

    dst[0] = glyph;
    dst[1] = L' ';

    u32 i = 0;
    const u32 maxName = dstLen - 3;  // glyph, space, terminator
    while (i < maxName && name[i] != L'\0') {
        dst[i + 2] = name[i];
        ++i;
    }
    dst[i + 2] = L'\0';
    return true;
}

}  // namespace Rank
}  // namespace PointRating
}  // namespace Pulsar
