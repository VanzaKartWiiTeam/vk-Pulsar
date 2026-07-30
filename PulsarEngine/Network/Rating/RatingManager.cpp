#include <Network/Rating/RatingManager.hpp>
#include <Network/Rating/RatingCalculator.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/RankManager.hpp>
#include <Network/Rating/RatingStorage.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

namespace Pulsar {
namespace PointRating {

// Fractional VR of remote players, indexed by [aid][slot on that console]. Filled by
// PulSELECT reception; the integer part rides the vanilla rating field.
u8 remoteDecimalVR[12][2];

// Signed variation of the last race, consumed by the end-of-race leaderboard.
float lastRaceDeltas[12];

namespace Manager {

static bool IsBattleMode(GameMode mode) {
    return mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
}

static bool IsRegionalVS() {
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl != nullptr && (ctrl->roomType == RKNet::ROOMTYPE_VS_REGIONAL ||
                               ctrl->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL);
}

static bool IsRegionalBT() {
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl != nullptr && ctrl->roomType == RKNet::ROOMTYPE_BT_REGIONAL;
}

bool IsRankedFroom() {
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    if (ctrl == nullptr || System::sInstance == nullptr) return false;
    const bool isFroom = ctrl->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                         ctrl->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    return isFroom && System::sInstance->IsContext(PULSAR_VR);
}

RatingKind GetStake(const RacedataSettings& settings) {
    if (IsBattleMode(settings.gamemode)) {
        return (IsRegionalBT() || IsRankedFroom()) ? RATING_BR : RATING_NONE;
    }
    const bool publicVS = IsRegionalVS() && settings.gamemode == MODE_PUBLIC_VS;
    return (publicVS || IsRankedFroom()) ? RATING_VR : RATING_NONE;
}

// Gamemodes in which points move at all, ranked or not.
static bool IsRankedMode(const RacedataSettings& settings) {
    return settings.gamemode > MODE_6 && settings.gamemode < MODE_AWARD;
}

// The local player that owns the licence is the first local one; a guest sharing the
// console has no profile of their own and therefore nothing to persist.
static bool IsLicenseOwner(const RacedataScenario& scenario, u32 idx) {
    if (scenario.players[idx].playerType != PLAYER_REAL_LOCAL) return false;
    for (u32 i = 0; i < idx; ++i) {
        if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) return false;
    }
    return true;
}

static RatingValue ReadRating(const RacedataScenario& scenario, u32 idx, RatingKind stake, bool isBattle,
                              bool isOwner) {
    const RacedataPlayer& player = scenario.players[idx];

    if (isOwner) {
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (rksys != nullptr && rksys->curLicenseId < Config::MAX_LICENSES) {
            // A battle always reads the custom BR, even when the room turns out not
            // to save it; only VR is gated on the race actually being ranked.
            if (isBattle) return Storage::GetBR(rksys->curLicenseId);
            if (stake == RATING_VR) return Storage::GetVR(rksys->curLicenseId);
        }
    } else if (player.playerType == PLAYER_REAL_ONLINE) {
        const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
        if (ctrl != nullptr) {
            const u8 aid = ctrl->aidsBelongingToPlayerIds[idx];
            u8 slot = 0;
            for (u32 i = 0; i < idx; ++i) {
                if (ctrl->aidsBelongingToPlayerIds[i] == aid) ++slot;
            }
            if (aid < 12 && slot < 2) {
                return (float)player.rating.points + (float)remoteDecimalVR[aid][slot] / 100.0f;
            }
        }
    }
    return (float)player.rating.points;
}

// Vanilla race points, untouched by the rating system.
static void AwardRacePoints(RacedataScenario& scenario, u32 idx, u8 position, u16 battleScore, bool isBattle) {
    if (position == 0 || scenario.playerCount == 0) return;

    scenario.players[idx].finishPos = position;

    u16 points = 0;
    if (!isBattle && (scenario.settings.gamemode < 9 || scenario.settings.gamemode > 10)) {
        if (scenario.playerCount <= 12 && position <= 12) {
            points = Racedata::pointsRoom[scenario.playerCount - 1][position - 1];
        }
    } else {
        points = battleScore;
    }
    scenario.players[idx].score = scenario.players[idx].previousScore + points;
}

void UpdatePoints(RacedataScenario* scenario) {
    if (scenario == nullptr || scenario->settings.gametype != GAMETYPE_DEFAULT) return;

    Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceInfo == nullptr) return;

    const u32 playerCount = (scenario->playerCount < 12) ? scenario->playerCount : 12;
    const RatingKind stake = GetStake(scenario->settings);
    const bool isBattle = IsBattleMode(scenario->settings.gamemode);

    RaceContext ctx;
    ctx.playerCount = playerCount;
    ctx.isBattle = isBattle;
    ctx.isRanked = IsRankedMode(scenario->settings);
    ctx.isVR = stake == RATING_VR;
    ctx.multiplier = GetMultiplier();
    ctx.allDisconnected = false;

    if (ctx.isVR) {
        const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
        if (ctrl != nullptr && ctrl->subs[ctrl->currentSub].connectionCount <= 1) {
            ctx.allDisconnected = true;
        }
    }

    // One snapshot pass: after this the maths never touches the save file or RKSYS,
    // which previously happened on the order of a hundred times per race.
    PlayerSnapshot snapshots[12];
    for (u32 i = 0; i < playerCount; ++i) {
        const bool isOwner = IsLicenseOwner(*scenario, i);
        snapshots[i].isLocalOwner = isOwner;
        snapshots[i].rating = ReadRating(*scenario, i, stake, isBattle, isOwner);
        snapshots[i].position = raceInfo->players[i]->position;
        snapshots[i].battleScore = isBattle ? raceInfo->players[i]->battleScore : 0;

        AwardRacePoints(*scenario, i, snapshots[i].position, raceInfo->players[i]->battleScore, isBattle);
    }

    float deltas[12];
    Calculator::ComputeDeltas(snapshots, ctx, deltas);

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const bool canPersist = rksys != nullptr && rksys->curLicenseId < Config::MAX_LICENSES;

    // One flush for the whole commit instead of one per field written.
    Storage::BeginBatch();
    for (u32 i = 0; i < playerCount; ++i) {
        const RatingValue oldRating = snapshots[i].rating;
        const RatingValue next = Calculator::TruncateToCentis(
            Calculator::ClampToLimits(oldRating + deltas[i]));

        scenario->players[i].rating.points = (u16)next;
        lastRaceDeltas[i] = next - oldRating;

        if (!snapshots[i].isLocalOwner || !canPersist || stake == RATING_NONE) continue;

        const u32 licenseId = (u32)rksys->curLicenseId;
        if (stake == RATING_BR) {
            Storage::SetBR(licenseId, next);
        } else {
            Storage::SetVR(licenseId, next);
            const RankId oldRank = Storage::GetRank(licenseId);
            const RankId newRank = Rank::Resolve(next, oldRank);
            if (newRank != oldRank) Storage::SetRank(licenseId, newRank);
        }
    }
    Storage::EndBatch();
}

}  // namespace Manager

// ------------------------------------------------------------- public facade
// Kept in the PointRating namespace so every existing call site still compiles.

float GetUserVR(u32 licenseId) {
    return Storage::GetVR(licenseId);
}
float GetUserBR(u32 licenseId) {
    return Storage::GetBR(licenseId);
}
void SetUserVR(u32 licenseId, float vr) {
    Storage::SetVR(licenseId, vr);
}
void SetUserBR(u32 licenseId, float br) {
    Storage::SetBR(licenseId, br);
}
u32 GetUserRank(u32 licenseId) {
    return Storage::GetRank(licenseId);
}
void SetUserRank(u32 licenseId, u32 rank) {
    Storage::SetRank(licenseId, (RankId)rank);
}
void BindLicenseProfileId(u32 licenseId, s32 profileId) {
    Storage::BindLicenseProfileId(licenseId, profileId);
}
void SaveProfileVR(s32 profileId, float vr) {
    Storage::SaveProfileVR(profileId, vr);
}
void SaveProfileBR(s32 profileId, float br) {
    Storage::SaveProfileBR(profileId, br);
}

u8 GetRemotePrestigeRank(u8 aid, u8 playerIndexOnConsole) {
    return Rank::GetRemote(aid, playerIndexOnConsole);
}
void CacheRemotePrestigeRanks(u8 aid, const u8 ranks[2]) {
    Rank::CacheRemote(aid, ranks);
}
void ResetRemotePrestigeRanks() {
    Rank::ResetRemote();
}
u32 GetPlayerPrestigeRank(u8 playerId) {
    return Rank::GetForPlayer(playerId);
}

}  // namespace PointRating
}  // namespace Pulsar
