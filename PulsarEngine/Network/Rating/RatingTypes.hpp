#ifndef _PULSAR_RATING_TYPES_HPP_
#define _PULSAR_RATING_TYPES_HPP_

#include <kamek.hpp>

namespace Pulsar {
namespace PointRating {

// A rating as stored internally: displayed VR / 100.
typedef float RatingValue;

// 0 means unranked; 1..Config::MAX_RANK are the prestige tiers.
typedef u8 RankId;

// Which rating a race puts at stake.
enum RatingKind {
    RATING_NONE,  // nothing is saved
    RATING_VR,
    RATING_BR,
};

// One player's state going into the end-of-race computation.  Snapshotted once so
// the O(n^2) comparison never touches the save file or RKSYS again.
struct PlayerSnapshot {
    RatingValue rating;
    u16 battleScore;
    u8 position;  // 1-based finish position
    bool isLocalOwner;  // local player that owns the licence, i.e. the one we persist
};

// Everything the calculator needs to know about the race itself.
struct RaceContext {
    float multiplier;
    u32 playerCount;
    bool isBattle;
    bool isRanked;  // gamemode is one where points move at all
    bool isVR;  // VR specifically is at stake (enables the anti-farm rules)
    bool allDisconnected;
};

}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_RATING_TYPES_HPP_
