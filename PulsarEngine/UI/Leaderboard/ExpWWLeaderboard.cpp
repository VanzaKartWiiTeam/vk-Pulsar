#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Ctrl/CtrlRace/CtrlRaceResult.hpp>
#include <MarioKartWii/UI/Page/Leaderboard/WWLeaderboardUpdate.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/UI/Section/Section.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RatingManager.hpp>
#include <Network/PacketExpansion.hpp>
#include <UI/UI.hpp>
#include <include/c_stdlib.h>
#include <include/c_wchar.h>

extern "C" void OSReport(const char* format, ...);

/*
    Replaces the end of race leaderboard so it can show a rating with decimals.

    Without this the vanilla routine reads the u16 rating.points, which holds our
    internal value (displayed VR / 100), and diffs it against the previous value on the
    vanilla scale. A rating of 50.00 then renders as "50" and the change comes out as
    something like -4950 instead of the couple of points actually won or lost.

    Ported from Retro Rewind. Adapted: the controller is used directly instead of RR's
    CustomRKNetController, and the clamps follow VanzaKart's own MIN/MAX_RATING rather
    than RR's hardcoded 10000.
*/

namespace Pulsar {
namespace UI {

typedef Pages::GPVSLeaderboardUpdate::Player PlayerEntry;

typedef void (*FillDeltaFn)(CtrlRaceResult*, s32, u8);
typedef void (*ApplyExtraPageFn)(Page*);
typedef void (*MarkLicensesDirtyFn)(void*);

kmRuntimeUse(0x807f579c);
kmRuntimeUse(0x8064f65c);
kmRuntimeUse(0x80621410);
static const FillDeltaFn sFillDelta = reinterpret_cast<FillDeltaFn>(kmRuntimeAddr(0x807f579c));
static const ApplyExtraPageFn sApplyExtraPage = reinterpret_cast<ApplyExtraPageFn>(kmRuntimeAddr(0x8064f65c));
static const MarkLicensesDirtyFn sMarkLicensesDirty = reinterpret_cast<MarkLicensesDirtyFn>(kmRuntimeAddr(0x80621410));

static const float RATING_DISPLAY_MAX = (float)PointRating::MAX_RATING;
static const float RATING_DISPLAY_MIN = (float)PointRating::MIN_RATING;

/*
    Both numbers on a row go through the vanilla FillScore / FillDelta.

    The custom path this file used to take -- a float counter behind a vtable hook, plus
    SetTextBoxMessage with BMG_TEXT -- could never draw anything here. BMG_TEXT (0x2849)
    is a Pulsar message that only exists in the menu BMGs, so in the Race scene the id
    does not resolve and the pane keeps the layout placeholder; writing TextBox::stringBuf
    by hand does not help either, because the glyphs are built by Text::PaneHandler from
    its own buffer, not from that string.

    None of it was needed: a rating is shown as whole*100 + hundredths run together, which
    is just an integer. Scaling the internal value by 100 and handing it to the routines
    the game already uses for the vanilla VR renders it for free, animation included.
*/
static u32 ToDisplayedRating(float internalRating) {
    if (internalRating < 0.0f) internalRating = 0.0f;
    return (u32)(internalRating * 100.0f + 0.5f);
}

static s32 ToDisplayedDelta(float internalDelta) {
    const float rounding = (internalDelta >= 0.0f) ? 0.5f : -0.5f;
    return (s32)(internalDelta * 100.0f + rounding);
}

struct RatingDisplay {
    float total;
    float startTotal;
    float delta;
};

// The vanilla delta is a u16 difference, so a drop wraps around; bring it back.
static s32 NormalizeRatingDelta(s32 rawDelta) {
    const s32 HALF_RANGE = 0x8000;
    const s32 FULL_RANGE = 0x10000;
    if (rawDelta <= -HALF_RANGE) return rawDelta + FULL_RANGE;
    if (rawDelta >= HALF_RANGE) return rawDelta - FULL_RANGE;
    return rawDelta;
}

inline bool IsValidPlayerId(u8 playerId) {
    return playerId < 12;
}

bool IsBattleMode(const RacedataScenario& scenario) {
    const int diff = static_cast<int>(scenario.settings.gamemode) - static_cast<int>(MODE_BATTLE);
    if (diff < 0 || diff >= 8) return false;
    return ((1u << diff) & 0xC1u) != 0;
}

u8 DetermineAnimationVariant(u32 rowIndex) {
    return rowIndex == 0 ? 1 : 0;
}

void* GetSaveGhostManagerPointer() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr) return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<u32>(mgr) + 0x90);
}

void UpdateBattleScores(const RacedataScenario& scenario, Raceinfo* raceInfo) {
    if (raceInfo == nullptr) return;
    SectionMgr* mgr = SectionMgr::sInstance;
    if (mgr == nullptr || mgr->sectionParams == nullptr) return;

    const Team winningTeam = mgr->sectionParams->lastBattleWinningTeam;
    const s16 bonus = (scenario.settings.battleType == BATTLE_BALLOON) ? 3 : 5;

    for (u8 i = 0; i < scenario.playerCount; ++i) {
        if (scenario.players[i].team != winningTeam) continue;
        RaceinfoPlayer* infoPlayer = raceInfo->players[i];
        if (infoPlayer == nullptr) continue;
        infoPlayer->battleScore = static_cast<s16>(infoPlayer->battleScore + bonus);
    }
}

// Clamps a rating pair to the ladder's limits, zeroing a change that would push past
// an edge so the row never animates towards a value the rating cannot reach.
static void ClampRatingPair(float& current, float& oldRating, float& delta) {
    if (current > RATING_DISPLAY_MAX)
        current = RATING_DISPLAY_MAX;
    else if (current < RATING_DISPLAY_MIN)
        current = RATING_DISPLAY_MIN;

    if (oldRating >= RATING_DISPLAY_MAX && delta > 0.0f) {
        delta = 0.0f;
        current = RATING_DISPLAY_MAX;
    } else if (oldRating <= RATING_DISPLAY_MIN && delta < 0.0f) {
        delta = 0.0f;
        current = RATING_DISPLAY_MIN;
    } else {
        delta = current - oldRating;
    }
}

RatingDisplay BuildRatingDisplay(u8 playerId, bool isBattle, const RacedataScenario& raceScenario,
                                 const RacedataScenario& menuScenario) {
    RatingDisplay display = {};

    const RacedataPlayer& racePlayer = raceScenario.players[playerId];
    const RacedataPlayer& menuPlayer = menuScenario.players[playerId];

    if (racePlayer.playerType == PLAYER_REAL_LOCAL) {
        RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
        if (rksys != nullptr && rksys->curLicenseId < 4) {
            float current = isBattle ? PointRating::GetUserBR(rksys->curLicenseId)
                                     : PointRating::GetUserVR(rksys->curLicenseId);
            float delta = PointRating::lastRaceDeltas[playerId];
            float oldRating = current - delta;

            const float startRating = (float)racePlayer.rating.points;
            if (startRating >= RATING_DISPLAY_MAX)
                oldRating = RATING_DISPLAY_MAX;
            else if (startRating <= RATING_DISPLAY_MIN)
                oldRating = RATING_DISPLAY_MIN;

            ClampRatingPair(current, oldRating, delta);

            display.total = current;
            display.startTotal = oldRating;
            display.delta = delta;
            return display;
        }
    } else if (racePlayer.playerType == PLAYER_REAL_ONLINE) {
        const RKNet::Controller* controller = RKNet::Controller::sInstance;
        if (controller != nullptr) {
            const u8 aid = controller->aidsBelongingToPlayerIds[playerId];

            u8 playerIndexOnConsole = 0;
            for (u8 i = 0; i < playerId; ++i) {
                if (controller->aidsBelongingToPlayerIds[i] == aid) ++playerIndexOnConsole;
            }

            if (aid < 12 && playerIndexOnConsole < 2) {
                // The integer part rides the vanilla field, the hundredths come over
                // the wire in PulSELECT.
                float oldRating = (float)racePlayer.rating.points +
                                  (float)PointRating::remoteDecimalVR[aid][playerIndexOnConsole] / 100.0f;
                float delta = PointRating::lastRaceDeltas[playerId];
                float current = oldRating + delta;

                ClampRatingPair(current, oldRating, delta);

                display.total = current;
                display.startTotal = oldRating;
                display.delta = delta;
                return display;
            }
        }
    }

    /*
        Anything else falls back to the scenario's own copy of the rating, which is stored
        on the same internal scale, so the caller's x100 still yields the right digits.
    */
    display.startTotal = (float)racePlayer.rating.points;
    display.delta = (float)NormalizeRatingDelta(static_cast<s32>(menuPlayer.rating.points) -
                                                static_cast<s32>(racePlayer.rating.points));
    display.total = display.startTotal + display.delta;
    return display;
}

u8 ResolvePlayerIdForRow(bool isBattle, const PlayerEntry* sortedEntries, u32 rowIndex, const Raceinfo* raceInfo) {
    if (isBattle) return sortedEntries[rowIndex].playerId;
    if (raceInfo != nullptr && raceInfo->playerIdInEachPosition != nullptr) {
        return raceInfo->playerIdInEachPosition[rowIndex];
    }
    return static_cast<u8>(rowIndex);
}

// A guest sharing a console has no rating of their own, so their row shows no score.
bool ShouldSkipScoreDisplay(bool isBattle, u8 playerId, const RacedataScenario& raceScenario,
                            const RKNet::Controller* controller) {
    if (isBattle || controller == nullptr) return false;
    if (!IsValidPlayerId(playerId)) return false;
    if (raceScenario.players[playerId].playerType == PLAYER_REAL_LOCAL) return false;

    const u8 aidCurrent = controller->aidsBelongingToPlayerIds[playerId];
    const u8 aidPrevious = (playerId > 0) ? controller->aidsBelongingToPlayerIds[playerId - 1] : 0xFF;
    if (aidCurrent == 0xFF) return false;
    return aidCurrent == aidPrevious;
}

kmRuntimeUse(0x8085c16c);
kmRuntimeUse(0x8085cc84);
void WWLeaderboardFillRows(Pages::WWLeaderboardUpdate* page) {
    Raceinfo* raceInfo = Raceinfo::sInstance;
    Racedata* raceData = Racedata::sInstance;
    if (raceInfo == nullptr || raceData == nullptr) return;

    void (*prepareLicenses)() = reinterpret_cast<void (*)()>(kmRuntimeAddr(0x8085c16c));
    prepareLicenses();

    const RacedataScenario& raceScenario = raceData->racesScenario;
    const RacedataScenario& menuScenario = raceData->menusScenario;

    const bool isBattle = IsBattleMode(raceScenario);
    if (isBattle) UpdateBattleScores(raceScenario, raceInfo);

    // Same call the vanilla routine made here: this is what commits the new ratings.
    raceData->menusScenario.UpdatePoints();

    u32 rowCount = static_cast<u32>(page->GetRowCount());
    const u32 playerCount = raceScenario.playerCount;
    if (rowCount > playerCount) rowCount = playerCount;

    PlayerEntry* sortedEntries = page->sortedArray;

    const PointRating::RatingKind stake = PointRating::Manager::GetStake(raceScenario.settings);

    // DIAGNOSTIC, remove with the [VK LB] lines in BuildRatingDisplay.
    {
        const RKNet::Controller* diagCtrl = RKNet::Controller::sInstance;
        OSReport("[VK LB] gara rowCount=%u playerCount=%u isBattle=%d stake=%d roomType=%d rankedFroom=%d\n",
                 rowCount, playerCount, (int)isBattle, (int)stake,
                 diagCtrl != nullptr ? (int)diagCtrl->roomType : -1,
                 (int)PointRating::Manager::IsRankedFroom());
    }

    if (isBattle) {
        if (sortedEntries == nullptr) return;
        for (u32 i = 0; i < rowCount; ++i) {
            sortedEntries[i].playerId = static_cast<u8>(i);
            RaceinfoPlayer* infoPlayer = raceInfo->players[i];
            sortedEntries[i].totalScore = (infoPlayer != nullptr) ? static_cast<u32>(infoPlayer->battleScore) : 0;
            sortedEntries[i].lastRaceScore = 0;
        }
        typedef int (*Comparator)(const void*, const void*);
        static const Comparator sortPlayers = reinterpret_cast<Comparator>(kmRuntimeAddr(0x8085cc84));
        qsort(sortedEntries, rowCount, sizeof(PlayerEntry), sortPlayers);
    }

    const RKNet::Controller* controller = RKNet::Controller::sInstance;

    for (u32 row = 0; row < rowCount; ++row) {
        const u8 rank = static_cast<u8>(row + 1);
        const u8 playerId = ResolvePlayerIdForRow(isBattle, sortedEntries, row, raceInfo);
        if (!IsValidPlayerId(playerId)) continue;

        if (page->results == nullptr) continue;
        CtrlRaceResult* result = page->results[row];
        if (result == nullptr) continue;

        result->Fill(rank, playerId);

        /*
            DIAGNOSTIC, remove once the BMG base is known. Fill() has just written the
            star rank message id into OnlineParams::rankBMG, so dumping it here next to
            the CalcRank log tells us which BMG id each index maps to -- the one thing
            needed to put the prestige rank where the stars are.
        */
        {
            SectionMgr* diagMgr = SectionMgr::sInstance;
            if (diagMgr != nullptr && diagMgr->sectionParams != nullptr) {
                OSReport("[VK RANKICON] row=%u playerId=%u rankBMG=0x%04X\n", row, playerId,
                         diagMgr->sectionParams->onlineParams.rankBMG[playerId]);
            }
        }

        if (isBattle) {
            const PlayerEntry& topEntry = sortedEntries[0];
            const PlayerEntry& currentEntry = sortedEntries[row];
            if (topEntry.totalScore != 0 && currentEntry.totalScore == topEntry.totalScore) {
                result->SetTextBoxMessage("position", 0x541);
            } else {
                result->ResetTextBoxMessage("position");
            }
        }

        if (ShouldSkipScoreDisplay(isBattle, playerId, raceScenario, controller)) {
            result->SetTextBoxMessage("total_score", 0x25e7);
            result->ResetTextBoxMessage("total_point");
            result->ResetTextBoxMessage("get_point");
        } else {
            const RatingDisplay display = BuildRatingDisplay(playerId, isBattle, raceScenario, menuScenario);
            const u32 messageId = isBattle ? 0x540 : 0x53f;

            const u32 displayedStart = ToDisplayedRating(display.startTotal);
            const u32 displayedTotal = ToDisplayedRating(display.total);
            const s32 displayedDelta = ToDisplayedDelta(display.delta);

            OSReport("[VK LB] row=%u playerId=%u start=%u -> total=%u delta=%d\n",  //DIAGNOSTIC
                     row, playerId, displayedStart, displayedTotal, displayedDelta);

            result->FillScore(displayedStart, messageId);
            sFillDelta(result, displayedDelta, DetermineAnimationVariant(row));
        }

        result->FillName(playerId);
    }

    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if (sectionMgr != nullptr && sectionMgr->curSection != nullptr) {
        const SectionId sectionId = sectionMgr->curSection->sectionId;
        if ((sectionId >= 0x68 && sectionId <= 0x69) || (sectionId >= 0x6c && sectionId <= 0x6d)) {
            sApplyExtraPage(sectionMgr->curSection->pages[0x48]);
        } else {
            sApplyExtraPage(nullptr);
        }
    }

    page->func_0x6c();

    void* saveGhostManager = GetSaveGhostManagerPointer();
    if (saveGhostManager != nullptr) sMarkLicensesDirty(saveGhostManager);
}
kmBranch(0x8085ce8c, WWLeaderboardFillRows);

}  // namespace UI
}  // namespace Pulsar
