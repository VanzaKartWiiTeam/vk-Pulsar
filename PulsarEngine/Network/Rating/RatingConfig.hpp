#ifndef _PULSAR_RATING_CONFIG_HPP_
#define _PULSAR_RATING_CONFIG_HPP_

#include <kamek.hpp>

/*
    Every tunable of the rating system lives here, so that balancing the mod never
    requires touching logic.  Ratings are stored as (displayed VR / 100): an internal
    50.0f is 5000 VR, 5000.0f is 500000 VR.
*/

namespace Pulsar {
namespace PointRating {
namespace Config {

// ---------------------------------------------------------------- rating scale
static const u16 MIN_RATING = 1;  // 100 displayed VR
static const u16 MAX_RATING = 5000;  // 500000 displayed VR
static const float DEFAULT_RATING = 50.0f;  // 5000 displayed VR

// ------------------------------------------------------------------ rank tiers
// Promotion every RANK_STEP, and a rank is only lost after falling DERANK_MARGIN
// below its own threshold.  Raising MAX_RANK is enough to add new tiers, provided
// the matching badge glyphs exist in the fonts.
static const u8 MAX_RANK = 8;
static const float RANK_STEP = 250.0f;  // 25000 displayed VR
static const float DERANK_MARGIN = 5.0f;  // 500 displayed VR

// Rank N is drawn as the private-use glyph BADGE_GLYPH_BASE + N.  Rank 0 draws nothing.
static const wchar_t BADGE_GLYPH_BASE = 0xF07C;

/*
    Puts the prestige rank where Mario Kart Wii draws the star rank, above the player
    name.  The rank rides SELECTPlayerData::starRank, which vanilla already transmits
    per console slot, so this costs no extra bytes and covers both local players.

    The mapping is now known, measured in game:

        CalcRank(wheelType, starRank) = wheelType * 4 + starRank
        rankBMG[playerId]             = RANK_BMG_BASE + CalcRank(...)

    observed as wheelType 0, starRank 3 -> index 3 -> BMG 0x25F1, and the range sits in
    the same BMG block as the VR and BR unit ids (0x25E4, 0x25E5), which corroborates it.

    Still 0, and what is missing is no longer knowledge but assets.  The twelve ids
    RANK_BMG_BASE .. RANK_BMG_BASE + 11 are the vanilla wheel-and-star icons: mapping a
    prestige rank onto them shows a star picture, not a rank badge.  Turning this on for
    real wants one BMG message per rank carrying the matching glyph, and then CalcRank
    returning our own index.  Sending and drawing have to be switched on together.
*/
#define RATING_RANK_REPLACES_STARS 0
static const u32 RANK_BMG_BASE = 0x25EE;
static const u32 RANK_BMG_VANILLA_COUNT = 12;  // wheelType 0-2 x starRank 0-3

/*
    The cheap route, taken from Retro Rewind: a single `li r3, N` inside
    Pages::Vote::BeforeEntranceAnimations decides the rank icon index outright. Writing
    the prestige rank there replaces the star icon with no packet change, no BMG lookup
    and no new asset -- the game already picks an icon per index.

    The tradeoff is inherent to the technique and RR lives with it: the immediate is one
    global value, written once per section from the local licence, so every entry drawn by
    that path shows *your* rank. You see yours; you do not see anyone else's. Per-player
    ranks need the starRank route above instead.

    The patched address is 0x806436a0; it lives as a literal in RatingHooks.cpp because
    the kmRuntime macros paste it into an identifier and reject a named constant.

    Set to 0 to go back to the vanilla star icon.
*/
#define RATING_RANK_ICON_LOCAL 1

// ------------------------------------------------------------- delta curve
// B-spline sampled to turn a rating difference into a per-opponent gain or loss.
static const int SPLINE_BIAS = 7499;
static const float SPLINE_SCALE = 0.00020004f;  // 1 / (2 * SPLINE_BIAS)
static const float POS_SPREAD = 4.0f;
static const float NEG_SPREAD = 16.0f;  // losses react harder to the gap than wins
static const float POS_MIN = 0.02f;
static const float POS_MAX = 0.24f;
static const float NEG_MIN = -0.19f;
static const float NEG_MAX = 0.0f;

// ------------------------------------------------------------------ gain caps
// Below GAIN_CAP_FREE_BELOW there is no practical ceiling; past GAIN_CAP_FLAT_ABOVE
// the ceiling is GAIN_CAP_MIN, with a linear ramp in between.
static const float GAIN_CAP_FREE_BELOW = 1500.0f;
static const float GAIN_CAP_FLAT_ABOVE = 9000.0f;
static const float GAIN_CAP_MIN = 0.10f;
static const float GAIN_CAP_UNBOUNDED = 1e6f;
static const float GAIN_CAP_RAMP = 999.9f;

// ------------------------------------------------------------------ loss caps
// LOSS_CAP_SOFT applies at or below LOSS_RAMP_LOW, LOSS_CAP_HARD at or above
// LOSS_RAMP_HIGH.  Both bounds are negative and the ramp never crosses zero:
// a cap that turned positive would silently become a guaranteed *gain*.
static const float LOSS_RAMP_LOW = 150.0f;
static const float LOSS_RAMP_HIGH = 500.0f;
static const float LOSS_CAP_SOFT = -0.5f;
static const float LOSS_CAP_HARD = -2.09f;  // about -209 displayed VR

// Extra protection under LOSS_RAMP_LOW: the raw loss is divided by this factor.
static const float LOW_VR_DIVIDER_MAX = 7.5f;
static const float LOW_VR_DIVIDER_SLOPE = 6.5f;

// Anti-farm: if everyone else dropped, a full room only costs this much.
static const float ALL_DISCONNECTED_PENALTY = -0.01f;
static const u32 ALL_DISCONNECTED_MIN_PLAYERS = 4;
// Losses smaller than this are rounded away so a good race never shows -0 VR.
static const float NEGLIGIBLE_LOSS = -0.0101f;

// -------------------------------------------------------------------- network
// QR2 publishes rating * 100 under keys 0x65 (VR) and 0x66 (BR).
static const int QR2_KEY_VR = 0x65;
static const int QR2_KEY_BR = 0x66;
static const int QR2_MIN = 1;
static const int QR2_MAX = 500000;

static const char* const MULTIPLIER_URL = "http://sitodaking.it:8000/VanzaKart/multiplierBeta.txt";
static const float MULTIPLIER_MIN = 0.0f;
static const float MULTIPLIER_MAX = 100.0f;
static const float MULTIPLIER_DEFAULT = 1.0f;

// Server-side rating sync.  The architecture is compiled in, but stays inert until
// VanzaKart actually serves the endpoint; VKRating.pul remains authoritative.
#define RATING_SYNC_ENABLED 0
static const char* const SYNC_REPORT_KEY = "wl:mkw_vrbr";
// Answers with {"found":1,"vr":<rating*100>,"br":<rating*100>} for the given profile.
static const char* const SYNC_DOWNLOAD_URL_FORMAT = "http://sitodaking.it:8000/VanzaKart/api/ratings?pid=%ld";
static const u32 SYNC_RESPONSE_MAX = 256;

// -------------------------------------------------------------------- storage
static const u32 MAGIC = 'RRRT';
static const u16 VERSION = 3;
static const u32 MAX_LICENSES = 4;
static const u32 MAX_PROFILES = 100;
static const char* const SAVE_FILE_NAME = "VKRating.pul";

}  // namespace Config
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_RATING_CONFIG_HPP_
