#ifndef _PULSAR_MIRROR_REPORT_HPP_
#define _PULSAR_MIRROR_REPORT_HPP_

#include <kamek.hpp>

namespace Pulsar {
namespace Mirror {

/*
    One player's line in a race report. Ratings are in the same units the rest of the
    rating code uses (points, not centis), so what lands on the server matches what the
    player saw on the results screen.
*/
struct PlayerRecord {
    u32 profileId;  // filled in by CaptureRace from the player index; callers leave it alone
    u16 ratingBefore;
    u16 ratingAfter;
    u16 battleScore;
    u8 position;
    u8 padding[1];
};

/*
    Called once per race, at the point the rating has just been committed. Only snapshots
    into memory - see the note in MirrorConfig.hpp about why nothing is sent here.
*/
void CaptureRace(const PlayerRecord* players, u32 count, bool isBattle, bool isVR, u32 regionId);

// True when the dual send is allowed to talk to the network at all.
bool IsEnabled();

}  // namespace Mirror
}  // namespace Pulsar

#endif
