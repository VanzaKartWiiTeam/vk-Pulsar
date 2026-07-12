#include <UI/Leaderboard/ExpGPVSLeaderboardUpdate.hpp>
#include <UI/Leaderboard/LeaderboardDisplay.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

void ExpGPVSLeaderboardUpdate::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

void ExpGPVSLeaderboardUpdate::BeforeEntranceAnimations() {
    setLeaderboardDisplayType(LEADERBOARD_DISPLAY_NAMES);

    if (System::sInstance->IsContext(PULSAR_MODE_OTT)) {
        setLeaderboardDisplayType(LEADERBOARD_DISPLAY_TIMES);
    }

    fillLeaderboardResults(GetRowCount(), this->results);
}

static const int ALLOWED_PLAYER_PER_TEAM_COUNT[TEAM_COUNT + 1] = {0, 0, 6, 4, 3, 2, 2};

PageId ExpGPVSLeaderboardUpdate::GetNextPage() const {
    PageId nextPageId = Pages::GPVSLeaderboardUpdate::GetNextPage();
    if (ExtendedTeamManager::IsActivated() && nextPageId == PAGE_GPVS_TOTAL_LEADERBOARDS) {
        RacedataScenario &scenario = Racedata::sInstance->racesScenario;
        int teamCount = 0;
        int numPlayerPerTeam[TEAM_COUNT] = {0};

        for (int i = 0; i < scenario.playerCount; i++) {
            ExtendedTeamID team = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
            numPlayerPerTeam[team]++;
        }

        for (int i = 0; i < TEAM_COUNT; i++) {
            if (numPlayerPerTeam[i] > 0) {
                teamCount++;
            }
        }

        for (int i = 0; i < TEAM_COUNT; i++) {
            if (numPlayerPerTeam[i] > ALLOWED_PLAYER_PER_TEAM_COUNT[teamCount]) {
                return static_cast<PageId>(PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL_IRREGULAR);
            }
        }

        return static_cast<PageId>(PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL);
    }

    return nextPageId;
}

}  // namespace UI
}  // namespace Pulsar
