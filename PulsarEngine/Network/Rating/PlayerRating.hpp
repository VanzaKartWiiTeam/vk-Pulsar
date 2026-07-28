#ifndef _PULSAR_PLAYER_RATING_HPP_
#define _PULSAR_PLAYER_RATING_HPP_

#include <kamek.hpp>

namespace Pulsar {
namespace PointRating {

static const u16 MIN_RATING = 1;
static const u16 MAX_RATING = 5000; // 500000 displayed VR
static const float DEFAULT_RATING = 50.0f;

float GetUserVR(u32 licenseId);
float GetUserBR(u32 licenseId);
void SetUserVR(u32 licenseId, float vr);
void SetUserBR(u32 licenseId, float br);
u32 GetUserRank(u32 licenseId);
void SetUserRank(u32 licenseId, u32 rank);
void BindLicenseProfileId(u32 licenseId, s32 profileId);
void SaveProfileVR(s32 profileId, float vr);
void SaveProfileBR(s32 profileId, float br);
void FormatRatingDigits(float rating, wchar_t* buffer, u32 bufferSize);
u8 GetRemotePrestigeRank(u8 aid, u8 playerIndexOnConsole);
void CacheRemotePrestigeRanks(u8 aid, const u8 ranks[2]);
void ResetRemotePrestigeRanks();
u32 GetPlayerPrestigeRank(u8 playerId);

extern u8 remoteDecimalVR[12][2];
extern float lastRaceDeltas[12];

void TryDownloadMultiplier();
float GetMultiplier();
bool IsWeekendMultiplierActive();
bool IsWeekendMultiplierActiveForRegion(u8 region);
bool IsItemRainEventActive();

}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_PLAYER_RATING_HPP_
