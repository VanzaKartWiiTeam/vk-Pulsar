#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/PacketExpansion.hpp>
#include <include/c_wchar.h>

namespace Pulsar {
namespace PointRating {

u8 remoteDecimalVR[12][2];
static u8 remotePrestigeRanks[12][2];
float lastRaceDeltas[12];

u8 GetRemotePrestigeRank(u8 aid, u8 playerIndexOnConsole) {
    if (aid >= 12 || playerIndexOnConsole >= 2) return 0;
    const u8 rank = remotePrestigeRanks[aid][playerIndexOnConsole];
    return rank <= 8 ? rank : 0;
}

void CacheRemotePrestigeRanks(u8 aid, const u8 ranks[2]) {
    if (aid >= 12 || ranks == nullptr) return;
    remotePrestigeRanks[aid][0] = ranks[0] <= 8 ? ranks[0] : 0;
    remotePrestigeRanks[aid][1] = ranks[1] <= 8 ? ranks[1] : 0;
}

void ResetRemotePrestigeRanks() {
    memset(remotePrestigeRanks, 0, sizeof(remotePrestigeRanks));
}

static const s16 SPLINE_CONTROL_POINTS[5] = {0, 1, 8, 50, 125};
static const int SPLINE_BIAS = 7499;
static const float SPLINE_SCALE = 0.00020004f;  // 1/(2*SPLINE_BIAS)

static inline float Clamp(float val, float min, float max) {
    return (val < min) ? min : (val > max) ? max
                                           : val;
}

static float EvaluateSpline(float x) {
    float result = 0.0f;
    for (int i = -2; i <= 6; ++i) {
        int idx = (i < 0) ? 0 : (i > 4) ? 4
                                        : i;
        float d = x - (float)i;
        if (d < 0.0f) d = -d;

        float w = 0.0f;
        if (d <= 1.0f) {
            w = (4.0f - 6.0f * d * d + 3.0f * d * d * d) / 6.0f;
        } else if (d < 2.0f) {
            float t = 2.0f - d;
            w = (t * t * t) / 6.0f;
        }
        result += w * (float)SPLINE_CONTROL_POINTS[idx];
    }
    return result / 30.0f;
}

static float CalcPosPoints(float self, float opponent) {
    float sample = (float)SPLINE_BIAS + (opponent - self) * 4.0f;
    sample = Clamp(sample, 0.0f, (float)(SPLINE_BIAS * 2));
    return Clamp(EvaluateSpline(SPLINE_SCALE * sample), 0.02f, 0.24f);
}

static float CalcNegPoints(float self, float opponent) {
    float sample = (float)SPLINE_BIAS - (opponent - self) * 16.0f;
    sample = Clamp(sample, 0.0f, (float)(SPLINE_BIAS * 2));
    return Clamp(-EvaluateSpline(SPLINE_SCALE * sample), -0.19f, 0.0f);
}

static float GetGainCap(float rating) {
    if (rating < 1500.0f) return 1e6f;
    if (rating >= 9000.0f) return 0.10f;
    float t = (rating - 1500.0f) / 7500.0f;
    return 0.10f + 999.9f * (1.0f - t);
}

static float GetLossCap(float rating) {
    if (rating >= 500.0f) return -2.09f;
    float t = (rating - 150.0f) / 350.0f;
    return -0.5f + (-2.09f + 0.5f) * t;
}

static float GetLowVrLossDivider(float rating) {
    if (rating >= 150.0f) return 1.0f;
    if (rating <= 0.0f) return 7.5f;
    float t = rating / 150.0f;
    return 7.5f - 6.5f * t;
}

static bool IsBattle(GameMode mode) {
    return mode == MODE_BATTLE || mode == MODE_PUBLIC_BATTLE || mode == MODE_PRIVATE_BATTLE;
}

static bool IsRegionalVS() {
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl && (ctrl->roomType == RKNet::ROOMTYPE_VS_REGIONAL || ctrl->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL);
}

static bool IsRankedFroom() {
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl && (ctrl->roomType == RKNet::ROOMTYPE_FROOM_HOST || ctrl->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) &&
           System::sInstance->IsContext(PULSAR_VR);
}

static bool IsRegionalBT() {
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    return ctrl && (ctrl->roomType == RKNet::ROOMTYPE_BT_REGIONAL);
}

static bool IsRankedMode(const RacedataSettings& settings) {
    return settings.gamemode > MODE_6 && settings.gamemode < MODE_AWARD;
}

static int CountLocalPlayersBefore(const RacedataScenario& scenario, int idx) {
    int count = 0;
    for (int i = 0; i < idx; ++i) {
        if (scenario.players[i].playerType == PLAYER_REAL_LOCAL) count++;
    }
    return count;
}

static float GetPlayerRating(const RacedataScenario& scenario, int idx) {
    const RacedataPlayer& player = scenario.players[idx];

    if (player.playerType == PLAYER_REAL_LOCAL && CountLocalPlayersBefore(scenario, idx) == 0) {
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (rksys) {
            if (IsBattle(scenario.settings.gamemode)) {
                return GetUserBR(rksys->curLicenseId);
            }
            if ((IsRegionalVS() && scenario.settings.gamemode == MODE_PUBLIC_VS) || IsRankedFroom()) {
                return GetUserVR(rksys->curLicenseId);
            }
        }
    } else if (player.playerType == PLAYER_REAL_ONLINE) {
        const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
        u8 aid = ctrl->aidsBelongingToPlayerIds[idx];

        int slot = 0;
        for (int i = 0; i < idx; ++i) {
            if (ctrl->aidsBelongingToPlayerIds[i] == aid) slot++;
        }
        if (slot < 2) {
            float base = (float)player.rating.points;
            float decimal = (float)remoteDecimalVR[aid][slot] / 100.0f;
            return base + decimal;
        }
    }
    return (float)player.rating.points;
}

static float TruncateToCentis(float val) {
    return (float)((int)(val * 100.0f)) / 100.0f;
}

void FormatRatingDigits(float rating, wchar_t* buffer, u32 bufferSize) {
    int whole = (int)rating;
    int centis = (int)((rating - (float)whole) * 100.0f + 0.5f);
    if (centis >= 100) {
        whole++;
        centis -= 100;
    }
    if (centis < 0) centis = -centis;

    if (whole == 0)
        swprintf(buffer, bufferSize, L"%d", centis);
    else
        swprintf(buffer, bufferSize, L"%d%02d", whole, centis);
}

static void SaveLocalRating(const RacedataScenario& scenario, int idx, float rating) {
    const RacedataPlayer& player = scenario.players[idx];
    if (player.playerType != PLAYER_REAL_LOCAL || CountLocalPlayersBefore(scenario, idx) != 0) return;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (!rksys) return;

    if (IsBattle(scenario.settings.gamemode)) {
        if (IsRegionalBT() || IsRankedFroom()) SetUserBR(rksys->curLicenseId, rating);
    } else if ((IsRegionalVS() && scenario.settings.gamemode == MODE_PUBLIC_VS) || IsRankedFroom()) {
        SetUserVR(rksys->curLicenseId, rating);
    }
}

static void UpdatePlayerRating(RacedataScenario& scenario, int idx, float delta, bool allowRankUpdate) {
    float next = Clamp(GetPlayerRating(scenario, idx) + delta, (float)MIN_RATING, (float)MAX_RATING);
    next = TruncateToCentis(next);

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (allowRankUpdate && rksys != nullptr &&
        scenario.players[idx].playerType == PLAYER_REAL_LOCAL &&
        CountLocalPlayersBefore(scenario, idx) == 0 && rksys->curLicenseId < 4) {
        const u32 licenseId = rksys->curLicenseId;
        const u32 oldRank = GetUserRank(licenseId);
        u32 rank = oldRank;

        // Rating points are stored divided by 100: 250.0 == 25000 displayed VR.
        // Promotions happen every 25000 VR and never reset the player's rating.
        while (rank < 8 && next >= (float)(rank + 1) * 250.0f) ++rank;

        // A rank is lost only after falling 500 displayed VR below its threshold.
        while (rank > 0 && next < (float)rank * 250.0f - 5.0f) --rank;

        if (rank != oldRank) SetUserRank(licenseId, rank);
    }

    scenario.players[idx].rating.points = (u16)next;
    SaveLocalRating(scenario, idx, next);
}

void RR_UpdatePoints(RacedataScenario* scenario) {
    if (scenario->settings.gametype != GAMETYPE_DEFAULT) return;

    const u32 playerCount = scenario->playerCount;
    Raceinfo* raceInfo = Raceinfo::sInstance;
    bool isBattle = IsBattle(scenario->settings.gamemode);
    bool isRanked = IsRankedMode(scenario->settings);
    bool isVR = !isBattle && ((IsRegionalVS() && scenario->settings.gamemode == MODE_PUBLIC_VS) || IsRankedFroom());

    float deltas[12] = {};
    bool allDisconnected = false;
    if (isVR) {
        const RKNet::Controller* rkCtrl = RKNet::Controller::sInstance;
        if (rkCtrl->subs[rkCtrl->currentSub].connectionCount <= 1) {
            allDisconnected = true;
        }
    }

    for (u32 i = 0; i < playerCount; ++i) {
        u8 myPos = raceInfo->players[i]->position;
        u16 myScore = isBattle ? raceInfo->players[i]->battleScore : 0;

        if (isRanked) {
            float myRating = GetPlayerRating(*scenario, i);
            for (u32 j = 0; j < playerCount; ++j) {
                if (i == j) continue;
                float oppRating = GetPlayerRating(*scenario, j);

                if (isBattle) {
                    u16 oppScore = raceInfo->players[j]->battleScore;
                    if (oppScore < myScore)
                        deltas[i] += CalcPosPoints(myRating, oppRating);
                    else if (myScore < oppScore)
                        deltas[i] += CalcNegPoints(myRating, oppRating);
                } else {
                    u8 oppPos = raceInfo->players[j]->position;
                    if (myPos < oppPos)
                        deltas[i] += CalcPosPoints(myRating, oppRating);
                    else if (oppPos < myPos)
                        deltas[i] += CalcNegPoints(myRating, oppRating);
                }
            }
        }

        if (myPos != 0 && playerCount != 0) {
            scenario->players[i].finishPos = myPos;
            u16 pts = 0;
            if (!isBattle && (scenario->settings.gamemode < 9 || scenario->settings.gamemode > 10)) {
                if (playerCount <= 12 && myPos <= 12 && myPos > 0)
                    pts = Racedata::pointsRoom[playerCount - 1][myPos - 1];
            } else {
                pts = raceInfo->players[i]->battleScore;
            }
            scenario->players[i].score = scenario->players[i].previousScore + pts;
        }
    }

    float multiplier = GetMultiplier();
    for (u32 i = 0; i < playerCount; ++i) {
        float oldRating = GetPlayerRating(*scenario, i);
        if (isVR && oldRating < 150.0f && deltas[i] < 0.0f) {
            float divider = GetLowVrLossDivider(oldRating);
            if (divider > 1.0f) deltas[i] /= divider;
        }
        deltas[i] *= multiplier;
        deltas[i] = Clamp(deltas[i], GetLossCap(oldRating), GetGainCap(oldRating));

        if (isVR) {
            if (allDisconnected) {
                deltas[i] = (playerCount >= 4) ? -0.01f : 0.0f;
            } else if (deltas[i] >= -0.0101f && deltas[i] < 0.0f) {
                deltas[i] = 0.0f;
            }
        }

        float next = Clamp(oldRating + deltas[i], (float)MIN_RATING, (float)MAX_RATING);
        next = TruncateToCentis(next);
        UpdatePlayerRating(*scenario, i, deltas[i], isVR);
        lastRaceDeltas[i] = next - oldRating;
    }
}
u32 GetPlayerPrestigeRank(u8 playerId) {
    Racedata* racedata = Racedata::sInstance;
    if (racedata == nullptr || playerId >= 12 || playerId >= racedata->racesScenario.playerCount) return 0;

    const RacedataPlayer& player = racedata->racesScenario.players[playerId];
    if (player.playerType == PLAYER_REAL_LOCAL) {
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (rksys != nullptr && rksys->curLicenseId < 4 && player.hudSlotId == 0) {
            return GetUserRank(rksys->curLicenseId);
        }
    } else if (player.playerType == PLAYER_REAL_ONLINE) {
        const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
        if (ctrl == nullptr) return 0;
        const u8 aid = ctrl->aidsBelongingToPlayerIds[playerId];
        u8 playerIndexOnConsole = 0;
        for (u8 i = 0; i < playerId; ++i) {
            if (ctrl->aidsBelongingToPlayerIds[i] == aid) ++playerIndexOnConsole;
        }
        return GetRemotePrestigeRank(aid, playerIndexOnConsole);
    }
    return 0;
}
kmRuntimeUse(0x8052e950);
static void ApplyRatingPatch() {
    kmRuntimeBranchA(0x8052e950, RR_UpdatePoints);
    RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    if (((ctrl->roomType == RKNet::ROOMTYPE_FROOM_HOST || ctrl->roomType == RKNet::ROOMTYPE_FROOM_NONHOST) && !System::sInstance->IsContext(PULSAR_VR)) || ctrl->roomType == RKNet::ROOMTYPE_NONE) {
        kmRuntimeWrite32A(0x8052e950, 0x9421ff70);
    }
}
static SectionLoadHook ratingHook(ApplyRatingPatch);


}  // namespace PointRating
}  // namespace Pulsar
