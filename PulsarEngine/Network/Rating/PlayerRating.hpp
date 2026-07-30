#ifndef _PULSAR_PLAYER_RATING_HPP_
#define _PULSAR_PLAYER_RATING_HPP_

#include <kamek.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/RatingTypes.hpp>

/*
    Public surface of the rating system.

    This header is deliberately thin: it is what the rest of the mod includes, while
    the implementation is split across RatingManager, RatingCalculator, RankManager,
    RatingStorage, MultiplierManager, RatingNetwork, RatingSync and RatingUI.  Tuning
    values are not here -- they live in RatingConfig.hpp.
*/

namespace Pulsar {
namespace PointRating {

static const u16 MIN_RATING = Config::MIN_RATING;
static const u16 MAX_RATING = Config::MAX_RATING;  // 500000 displayed VR
static const float DEFAULT_RATING = Config::DEFAULT_RATING;

// ------------------------------------------------------------- local ratings
float GetUserVR(u32 licenseId);
float GetUserBR(u32 licenseId);
void SetUserVR(u32 licenseId, float vr);
void SetUserBR(u32 licenseId, float br);
u32 GetUserRank(u32 licenseId);
void SetUserRank(u32 licenseId, u32 rank);
void BindLicenseProfileId(u32 licenseId, s32 profileId);
void SaveProfileVR(s32 profileId, float vr);
void SaveProfileBR(s32 profileId, float br);

// ------------------------------------------------------------ remote players
// Prestige rank of a remote console's player slot, cached from PulSELECT.
u8 GetRemotePrestigeRank(u8 aid, u8 playerIndexOnConsole);
void CacheRemotePrestigeRanks(u8 aid, const u8 ranks[2]);
void ResetRemotePrestigeRanks();

// Rank of any player of the current race, local or remote.
u32 GetPlayerPrestigeRank(u8 playerId);

// Fractional VR of remote players, [aid][slot]; the integer part rides the vanilla
// rating field. Signed variation of the last race, per player id.
extern u8 remoteDecimalVR[12][2];
extern float lastRaceDeltas[12];

// ------------------------------------------------------------------ display
// Writes the rating as concatenated units and hundredths: 62.73 becomes "6273".
void FormatRatingDigits(float rating, wchar_t* buffer, u32 bufferSize);

// --------------------------------------------------------------- multiplier
void TryDownloadMultiplier();
float GetMultiplier();
bool IsWeekendMultiplierActive();
bool IsWeekendMultiplierActiveForRegion(u8 region);
bool IsItemRainEventActive();

}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_PLAYER_RATING_HPP_
