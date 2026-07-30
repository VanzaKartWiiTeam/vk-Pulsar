#ifndef _PULSAR_MULTIPLIER_MANAGER_HPP_
#define _PULSAR_MULTIPLIER_MANAGER_HPP_

#include <kamek.hpp>

/*
    The rating multiplier, composed from independent layers:

        base  x  event  x  weekend  x  remote

    Only the remote layer is implemented today; the others return 1.0 and exist so a
    seasonal event or a weekend bonus can be added later without touching a single
    caller.  Adding one means filling in its function here, nothing else.
*/

namespace Pulsar {
namespace PointRating {
namespace Multiplier {

// Kicks off the one-shot download. Safe to call every frame; it only fires once.
void TryDownload();

// The composed multiplier applied to every rating delta of a race.
float Get();

// Individual layers, exposed so the UI can highlight an active bonus.
float GetRemoteLayer();
float GetEventLayer();
float GetWeekendLayer(u8 region);
bool IsWeekendActive();
bool IsWeekendActiveForRegion(u8 region);
bool IsItemRainEventActive();

}  // namespace Multiplier
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_MULTIPLIER_MANAGER_HPP_
