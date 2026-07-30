#ifndef _PULSAR_RATING_MANAGER_HPP_
#define _PULSAR_RATING_MANAGER_HPP_

#include <kamek.hpp>
#include <Network/Rating/RatingTypes.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

/*
    Orchestration layer: decides whether a race counts, snapshots the room, hands the
    numbers to the calculator and commits the result to storage and to the scenario.
    It owns no maths of its own -- that lives in RatingCalculator -- and no file
    format -- that lives in RatingStorage.
*/

namespace Pulsar {
namespace PointRating {
namespace Manager {

// Which rating, if any, this race puts at stake for the local player.
RatingKind GetStake(const RacedataSettings& settings);

// True when the room is a friend room explicitly hosted as ranked.
bool IsRankedFroom();

// Replaces RacedataScenario::UpdatePoints. Recomputes every player's rating, awards
// the vanilla race points, applies promotions and persists the local player.
void UpdatePoints(RacedataScenario* scenario);

}  // namespace Manager
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_RATING_MANAGER_HPP_
