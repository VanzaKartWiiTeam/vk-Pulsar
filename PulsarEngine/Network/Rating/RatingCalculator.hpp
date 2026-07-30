#ifndef _PULSAR_RATING_CALCULATOR_HPP_
#define _PULSAR_RATING_CALCULATOR_HPP_

#include <kamek.hpp>
#include <Network/Rating/RatingTypes.hpp>

/*
    Pure end-of-race maths.  No hooks, no file access, no globals: give it a snapshot
    of the room and it returns the deltas.  Keeping it side-effect free is what makes
    the rating curve reviewable and reproducible without booting the game.
*/

namespace Pulsar {
namespace PointRating {
namespace Calculator {

// Fills deltas[0 .. ctx.playerCount-1] with the final, already capped variation to
// apply to each player.  players and deltas must both hold at least playerCount
// entries.  Callers commit the result; this function changes nothing.
void ComputeDeltas(const PlayerSnapshot* players, const RaceContext& ctx, float* deltas);

// Shared helpers, exposed because the manager and the storage layer clamp too.
float Clamp(float value, float min, float max);
float ClampToLimits(float rating);  // into [MIN_RATING, MAX_RATING]
float TruncateToCentis(float value);  // drop everything below one displayed VR

}  // namespace Calculator
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_RATING_CALCULATOR_HPP_
