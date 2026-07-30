#include <Network/Rating/RatingCalculator.hpp>
#include <Network/Rating/RatingConfig.hpp>

namespace Pulsar {
namespace PointRating {
namespace Calculator {

static const s16 SPLINE_CONTROL_POINTS[5] = {0, 1, 8, 50, 125};

float Clamp(float value, float min, float max) {
    return (value < min) ? min : (value > max) ? max
                                               : value;
}

float ClampToLimits(float rating) {
    return Clamp(rating, (float)Config::MIN_RATING, (float)Config::MAX_RATING);
}

float TruncateToCentis(float value) {
    return (float)((int)(value * 100.0f)) / 100.0f;
}

static float EvaluateSpline(float x) {
    float result = 0.0f;
    for (int i = -2; i <= 6; ++i) {
        const int idx = (i < 0) ? 0 : (i > 4) ? 4
                                              : i;
        float d = x - (float)i;
        if (d < 0.0f) d = -d;

        float w = 0.0f;
        if (d <= 1.0f) {
            w = (4.0f - 6.0f * d * d + 3.0f * d * d * d) / 6.0f;
        } else if (d < 2.0f) {
            const float t = 2.0f - d;
            w = (t * t * t) / 6.0f;
        }
        result += w * (float)SPLINE_CONTROL_POINTS[idx];
    }
    return result / 30.0f;
}

// Points won against an opponent we finished ahead of.
static float CalcPosPoints(float self, float opponent) {
    float sample = (float)Config::SPLINE_BIAS + (opponent - self) * Config::POS_SPREAD;
    sample = Clamp(sample, 0.0f, (float)(Config::SPLINE_BIAS * 2));
    return Clamp(EvaluateSpline(Config::SPLINE_SCALE * sample), Config::POS_MIN, Config::POS_MAX);
}

// Points lost to an opponent who finished ahead of us.
static float CalcNegPoints(float self, float opponent) {
    float sample = (float)Config::SPLINE_BIAS - (opponent - self) * Config::NEG_SPREAD;
    sample = Clamp(sample, 0.0f, (float)(Config::SPLINE_BIAS * 2));
    return Clamp(-EvaluateSpline(Config::SPLINE_SCALE * sample), Config::NEG_MIN, Config::NEG_MAX);
}

static float GetGainCap(float rating) {
    if (rating < Config::GAIN_CAP_FREE_BELOW) return Config::GAIN_CAP_UNBOUNDED;
    if (rating >= Config::GAIN_CAP_FLAT_ABOVE) return Config::GAIN_CAP_MIN;
    const float span = Config::GAIN_CAP_FLAT_ABOVE - Config::GAIN_CAP_FREE_BELOW;
    const float t = (rating - Config::GAIN_CAP_FREE_BELOW) / span;
    return Config::GAIN_CAP_MIN + Config::GAIN_CAP_RAMP * (1.0f - t);
}

/*
    The ramp is clamped at both ends on purpose.  Extrapolating below LOSS_RAMP_LOW
    makes the interpolation factor negative, and the cap crosses zero under roughly
    4000 displayed VR -- a *positive* loss cap, which Clamp() then turns into a
    guaranteed minimum gain for a player who finished last.
*/
static float GetLossCap(float rating) {
    if (rating >= Config::LOSS_RAMP_HIGH) return Config::LOSS_CAP_HARD;
    if (rating <= Config::LOSS_RAMP_LOW) return Config::LOSS_CAP_SOFT;
    const float span = Config::LOSS_RAMP_HIGH - Config::LOSS_RAMP_LOW;
    const float t = (rating - Config::LOSS_RAMP_LOW) / span;
    return Config::LOSS_CAP_SOFT + (Config::LOSS_CAP_HARD - Config::LOSS_CAP_SOFT) * t;
}

// Softens losses for players still climbing out of the bottom of the ladder.
static float GetLowVrLossDivider(float rating) {
    if (rating >= Config::LOSS_RAMP_LOW) return 1.0f;
    if (rating <= 0.0f) return Config::LOW_VR_DIVIDER_MAX;
    const float t = rating / Config::LOSS_RAMP_LOW;
    return Config::LOW_VR_DIVIDER_MAX - Config::LOW_VR_DIVIDER_SLOPE * t;
}

void ComputeDeltas(const PlayerSnapshot* players, const RaceContext& ctx, float* deltas) {
    if (players == nullptr || deltas == nullptr) return;
    const u32 count = ctx.playerCount;

    for (u32 i = 0; i < count; ++i) deltas[i] = 0.0f;
    if (!ctx.isRanked) return;

    // Every player is compared against every other one; the snapshot means this
    // stays pure arithmetic instead of re-reading the save file 150 times.
    for (u32 i = 0; i < count; ++i) {
        const PlayerSnapshot& me = players[i];
        float sum = 0.0f;
        for (u32 j = 0; j < count; ++j) {
            if (i == j) continue;
            const PlayerSnapshot& them = players[j];

            bool iWon;
            bool iLost;
            if (ctx.isBattle) {
                iWon = them.battleScore < me.battleScore;
                iLost = me.battleScore < them.battleScore;
            } else {
                iWon = me.position < them.position;
                iLost = them.position < me.position;
            }

            if (iWon)
                sum += CalcPosPoints(me.rating, them.rating);
            else if (iLost)
                sum += CalcNegPoints(me.rating, them.rating);
        }
        deltas[i] = sum;
    }

    for (u32 i = 0; i < count; ++i) {
        const float oldRating = players[i].rating;
        float delta = deltas[i];

        if (ctx.isVR && oldRating < Config::LOSS_RAMP_LOW && delta < 0.0f) {
            const float divider = GetLowVrLossDivider(oldRating);
            if (divider > 1.0f) delta /= divider;
        }

        // The multiplier is applied before the caps, so a 3x event does not always
        // deliver exactly triple once the ceiling bites.
        delta *= ctx.multiplier;
        delta = Clamp(delta, GetLossCap(oldRating), GetGainCap(oldRating));

        if (ctx.isVR) {
            if (ctx.allDisconnected) {
                delta = (ctx.playerCount >= Config::ALL_DISCONNECTED_MIN_PLAYERS)
                            ? Config::ALL_DISCONNECTED_PENALTY
                            : 0.0f;
            } else if (delta >= Config::NEGLIGIBLE_LOSS && delta < 0.0f) {
                delta = 0.0f;
            }
        }

        deltas[i] = delta;
    }
}

}  // namespace Calculator
}  // namespace PointRating
}  // namespace Pulsar
