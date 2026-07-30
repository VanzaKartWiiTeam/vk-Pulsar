#ifndef _PULSAR_RATING_SYNC_HPP_
#define _PULSAR_RATING_SYNC_HPP_

#include <kamek.hpp>

/*
    Server-side rating sync.

    Upload   : every rating write is reported on the GameSpy channel, so the server can
               keep leaderboards and spot impossible values.
    Download : at login the server's copy, when it has one, overwrites the local file.
               That is what makes the server authoritative instead of the SD card, and
               it is what lets a player change console without carrying VKRating.pul.

    The whole thing is compiled but inert while Config::RATING_SYNC_ENABLED is 0, because
    VanzaKart does not serve the endpoint yet. Until then VKRating.pul stays the source
    of truth and none of these calls touch the network.

    Two safeguards are worth keeping whenever it is switched on: a generation counter,
    so a slow reply cannot overwrite a rating that changed while it was in flight, and a
    suppression flag, so applying a downloaded rating does not bounce straight back out
    as an upload.
*/

namespace Pulsar {
namespace PointRating {

void SetSyncReportingSuppressed(bool suppress);
void ReportCurrentRatings(u32 licenseId);
void StartLoginRatingDownload(s32 profileId, u32 licenseId);

// True when the sync layer is actually allowed to talk to the network.
bool IsSyncEnabled();

}  // namespace PointRating
}  // namespace Pulsar

#endif
