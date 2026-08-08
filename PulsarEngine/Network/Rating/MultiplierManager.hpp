#ifndef _PULSAR_MULTIPLIER_MANAGER_HPP_
#define _PULSAR_MULTIPLIER_MANAGER_HPP_

#include <kamek.hpp>

/*
    The rating multiplier, composed from independent layers:

        event  x  weekend  x  remote  [ x beta ]

    event   seasonal dates (Christmas, Halloween, MKWii's birthday...), see Config
    weekend the rotating regional bonus, plus April Fools
    remote  the value served at Config::MULTIPLIER_URL, 1.0 until it downloads
    beta    a flat bonus on -DBETA builds

    All the date-driven layers read the console RTC through SystemManager, since VanzaKart
    has no server clock. Each layer is independent: switching one off means making it
    return 1.0, and no caller changes.
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
