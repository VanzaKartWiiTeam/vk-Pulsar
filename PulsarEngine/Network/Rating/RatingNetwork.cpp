#include <Network/Rating/RatingNetwork.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/RankManager.hpp>
#include <Network/Rating/RatingStorage.hpp>
#include <Network/Rating/RatingSync.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/PacketExpansion.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>

namespace Pulsar {
namespace PointRating {
namespace Net {

// Hundredths of a VR point, as carried in the packet's decimal byte.
static u8 ExtractDecimals(RatingValue rating) {
    const float fraction = rating - (float)((u16)rating);
    return (u8)(fraction * 100.0f + 0.5f);
}

void FillLocalPayload(Network::PulSELECT* packet) {
    if (packet == nullptr) return;

    for (u8 i = 0; i < 2; ++i) {
        packet->prestigeRank[i] = 0;
        packet->decimalVR[i] = 0;
    }

    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || rksys->curLicenseId >= Config::MAX_LICENSES) return;

    const u32 licenseId = rksys->curLicenseId;
    RankId rank = Rank::GetLocal(licenseId);
    if (rank > Config::MAX_RANK) rank = Config::MAX_RANK;
    const u8 decimals = ExtractDecimals(Storage::GetVR(licenseId));

    // Both local slots share the licence, so both advertise the same rank and
    // decimals; leaving slot 1 at zero is what used to hide the second player.
    for (u8 i = 0; i < 2; ++i) {
        packet->prestigeRank[i] = rank;
        packet->decimalVR[i] = decimals;
    }

#if RATING_RANK_REPLACES_STARS
    // The prestige rank rides the vanilla per-slot star rank field, which the game
    // already transmits and already routes to the icon above the player name.
    for (u8 i = 0; i < 2; ++i) packet->playersData[i].starRank = rank;
#endif
}

void ClearPayload(Network::PulSELECT* packet) {
    if (packet == nullptr) return;
    for (u8 i = 0; i < 2; ++i) {
        packet->prestigeRank[i] = 0;
        packet->decimalVR[i] = 0;
    }
}

void CacheRemotePayload(u8 aid, const Network::PulSELECT* packet) {
    if (packet == nullptr || aid >= 12) return;
    Rank::CacheRemote(aid, packet->prestigeRank);
    remoteDecimalVR[aid][0] = packet->decimalVR[0];
    remoteDecimalVR[aid][1] = packet->decimalVR[1];
}

int ClampForQr2(RatingValue rating) {
    const int scaled = (int)(rating * 100.0f + 0.5f);
    if (scaled < Config::QR2_MIN) return Config::QR2_MIN;
    if (scaled > Config::QR2_MAX) return Config::QR2_MAX;
    return scaled;
}

void BindActiveLicenseProfile() {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || rksys->curLicenseId >= Config::MAX_LICENSES) return;

    const u32 licenseId = rksys->curLicenseId;
    const s32 profileId = rksys->licenses[licenseId].dwcAccUserData.gsProfileId;
    // Binds the licence to its profile and, once the server exists, pulls its copy.
    StartLoginRatingDownload(profileId, licenseId);
}

}  // namespace Net
}  // namespace PointRating
}  // namespace Pulsar
