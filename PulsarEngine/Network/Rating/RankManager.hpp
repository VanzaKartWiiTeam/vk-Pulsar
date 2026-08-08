#ifndef _PULSAR_RANK_MANAGER_HPP_
#define _PULSAR_RANK_MANAGER_HPP_

#include <kamek.hpp>
#include <Network/Rating/RatingTypes.hpp>

/*
    The single source of truth for prestige ranks.

    Local player  -> VKRating.pul, through Rank::GetLocal()
    Remote player -> the cache fed by PulSELECT, through Rank::GetRemote()

    Nothing else is a valid source.  In particular the vanilla star rank, the wheel
    type and the licence's vanilla VR must never be used to derive a prestige rank.
*/

namespace Pulsar {
namespace PointRating {
namespace Rank {

// Rank a rating maps to, given the rank currently held.  Promotions never reset the
// rating, and a rank is only lost once the rating falls a full margin below its own
// threshold, so a player hovering on the boundary does not flicker between tiers.
RankId Resolve(RatingValue rating, RankId currentRank);

// Prestige rank of a local licence, straight from storage.
RankId GetLocal(u32 licenseId);

// Prestige rank of a remote console's player slot, from the PulSELECT cache.
RankId GetRemote(u8 aid, u8 playerIndexOnConsole);

// Called on reception; values outside the valid range are stored as unranked.
void CacheRemote(u8 aid, const u8 ranks[2]);
void ResetRemote();

// Resolves any player of the current race to a rank, local or remote.
RankId GetForPlayer(u8 playerId);

// Badge glyph for a rank, or 0 when the rank should not draw a badge.
wchar_t GetBadgeGlyph(RankId rank);

// The rank as it goes into a text string: the badge glyph when the font carries it,
// the plain digit otherwise.  Never empty -- rank 0 reads "0".  Returns the number of
// characters written, terminator excluded.
u32 FormatLabel(RankId rank, wchar_t* dst, u32 dstLen);

// Writes "<badge><space><name>" into dst.  Returns false and leaves dst untouched
// when the rank draws no badge or the name does not fit.
bool PrefixWithBadge(RankId rank, const wchar_t* name, wchar_t* dst, u32 dstLen);

}  // namespace Rank
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_RANK_MANAGER_HPP_
