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
    Whether a rank drawn inside a text string uses the badge glyph or the plain digit.
    Covers the Rank button on the WFC page, the race leaderboard, team select and Check
    Members -- Retro Rewind does the same thing, putting L"" straight into the
    string it hands to BMG_TEXT.

    Kept at 0 because the glyph would currently come out blank, and that is a font
    problem, not a code one.  Measured on the CMAP tables of Scene/UI/Font.szs:

        tt_kart_extension_font.brfnt         f05e-f06d and f074-f088, no ASCII at all
        kart_kanji_font.brfnt                ASCII 0020-007e and up, nothing in f0xx
        tt_kart_font_rodan_ntlg_pro_b.brfnt  71 glyphs, uppercase only

    The layouts ask for names that are not font files -- "mii_name" in
    common_w100_earth_mii_point_wifi.brlyt wants art_font_rodan_ntlg_pro_b, "go" in
    VRButton.brlyt wants ji_font -- but the coverage settles it without needing the
    mapping: a Mii name and the word "Rank" both contain lowercase, and kart_kanji_font
    is the only one of the three that has any.  That is also why the badge is invisible
    there while the text is not.

    So: add BADGE_GLYPH_BASE+1 .. BADGE_GLYPH_BASE+MAX_RANK to kart_kanji_font.brfnt,
    copying them from tt_kart_extension_font.brfnt in the same archive, then flip this
    to 1 and every one of those places shows the badge at once.
*/
#define RATING_BADGE_USES_GLYPH 1

/*
    The rank icon drawn next to the player name, the route Retro Rewind uses. Verified by
    disassembly, the whole chain is:

      1. SectionParams::combos[i].rank is filled by `bl OnlineParams::CalcRank` at
         0x806436a0 (player 1) and 0x806436e0 / 0x806436fc (player 2). Overwriting those
         three instructions with `li r3, idx` puts our own index there instead.
      2. SetPlayerData(character, kart, course, slot, combos[i].rank) copies it into
         SELECTPlayerData::starRank, which vanilla already transmits per console slot.
      3. On every console, OnlineParams::SetRankBMG(playerId, starRank) turns it into
         rankBMG[playerId] = (starRank < cap) ? RANK_BMG_BASE + starRank : 0,
         and 0 means no icon. That is what the lobby and Check Members draw.

    So it is *not* a local-only value: the immediate only decides what this console puts
    into its own packet, and every remote badge arrives in the sender's packet. Everyone
    sees everyone.

    SetRankBMG rejects any index at or above a hardcoded 12, the count of vanilla
    wheel-and-star icons, and writes 0 (no icon) instead. Retro Rewind raises that bound to
    make room for badges of its own, but VanzaKart does not need to: the pack reuses the
    twelve existing slots, so rank N is simply index N and MAX_RANK stays well inside.

    Rank N therefore draws BMG (RANK_BMG_BASE + N), 0x25EF for rank 1 up to 0x25F6 for
    rank 8, and each of those messages has to carry the badge glyph BADGE_GLYPH_BASE + N.
    Rank 0 keeps index 0 -> 0x25EE, the vanilla blank. That is one and the same convention
    as RATING_BADGE_USES_GLYPH above: this route draws the glyph as a BMG message the
    layout places on its own, that one splices it into the name string.
*/
static const u32 RANK_BMG_BASE = 0x25EE;
static const u32 RANK_BMG_VANILLA_COUNT = 12;  // wheelType 0-2 x starRank 0-3, and the hard cap

#define RATING_RANK_ICON 1

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

/*
    ------------------------------------------------------------------ multiplier layers
    Ported from Retro Rewind, which reads the date off its server clock. VanzaKart has no
    server clock, so the console RTC (SystemManager) is the source instead: same calendar
    day worldwide give or take a timezone, and each client computes its own delta anyway,
    so a disagreement costs fairness rather than desync.

    Weeks are counted from 1 January 2024 and the weekend bonus lands on odd weeks, one
    Wiimmfi region at a time, rotating every two weeks -- exactly RR's schedule, so the two
    mods stay in step.
*/
static const float EVENT_MULTIPLIER = 2.0f;
static const float WEEKEND_MULTIPLIER = 1.5f;
static const u16 WEEK_EPOCH_YEAR = 2024;
static const u8 WEEKEND_REGIONS[3] = {0x0C, 0x0B, 0x0D};  // vs_12, vs_11, vs_13

// Retro Rewind gives its beta builds a flat bonus; -DBETA is set by compile_and_link.py.
static const float BETA_MULTIPLIER = 1.25f;

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
