#include <Network/Rating/RankManager.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/RatingStorage.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <Network/Rating/StaffBadge.hpp>

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

u32 FormatLabel(RankId rank, wchar_t* dst, u32 dstLen) {
    if (dst == nullptr || dstLen < 2) return 0;

#if RATING_BADGE_USES_GLYPH
    const wchar_t glyph = GetBadgeGlyph(rank);
    if (glyph != 0) {
        dst[0] = glyph;
        dst[1] = L'\0';
        return 1;
    }
#endif
    dst[0] = (wchar_t)(L'0' + ((rank <= 9) ? rank : 9));
    dst[1] = L'\0';
    return 1;
}


bool PrefixWithBadge(RankId rank, const wchar_t* name, wchar_t* dst, u32 dstLen, wchar_t staffGlyph) {
    if (name == nullptr || dst == nullptr) return false;
    if (rank > Config::MAX_RANK) rank = 0;

#if RATING_BADGE_USES_GLYPH
    const wchar_t glyph = GetBadgeGlyph(rank);
    if (glyph == 0 && staffGlyph == 0) return false;
    wchar_t prefix[3];
    u32 prefixLen = 0;
    if (staffGlyph != 0) prefix[prefixLen++] = staffGlyph;
    if (glyph != 0) prefix[prefixLen++] = glyph;
    prefix[prefixLen++] = L' ';
    if (dstLen < prefixLen + 2) return false;
    for (u32 i = 0; i < prefixLen; ++i) dst[i] = prefix[i];
#else
    (void)staffGlyph;
    if (rank == 0) return false;
    // Ranks past 9 would need a second digit; MAX_RANK is 8, so one is enough.
    const u32 prefixLen = 4;
    if (dstLen < prefixLen + 2) return false;
    dst[0] = L'[';
    dst[1] = (wchar_t)(L'0' + rank);
    dst[2] = L']';
    dst[3] = L' ';
#endif

    u32 i = 0;
    const u32 maxName = dstLen - prefixLen - 1;
    while (i < maxName && name[i] != L'\0') {
        dst[i + prefixLen] = name[i];
        ++i;
    }
    dst[i + prefixLen] = L'\0';
    return true;
}

/*
    The name comes from the Mii, not from the pane: the pane holds the message FillName or
    UpdateInfo set, which is an escape sequence asking for "the Mii name", not the name.
    Copied into a BMG_TEXT string, that escape resolves without its Mii and comes out as
    some default Mii's name.
*/
bool ComposeRaceName(u8 playerId, wchar_t* dst, u32 dstLen) {
    if (playerId >= 12) return false;
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr == nullptr || sectionMgr->sectionParams == nullptr) return false;
    const Mii* mii = sectionMgr->sectionParams->playerMiis.GetMii(playerId);
    if (mii == nullptr) return false;

    const wchar_t staff = Staff::GetBadgeGlyph(Staff::GetForPlayer(playerId));
    return PrefixWithBadge(GetForPlayer(playerId), mii->info.name, dst, dstLen, staff);
}

}  // namespace Rank
}  // namespace PointRating
}  // namespace Pulsar
