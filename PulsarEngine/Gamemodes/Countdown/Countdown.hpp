#ifndef _PULSAR_COUNTDOWN_
#define _PULSAR_COUNTDOWN_
#include <kamek.hpp>

namespace Pulsar {
namespace Countdown {

static const u32 endFrame = 0x2328;
static const u32 musicRampFrame = 0x189C;
//Frames taken off the clock for every hit. Purely a time bonus: it used to double as the
//step of the score display, which is why one hit used to be worth three points.
static const u32 hitBonusFrames = 0xB4;
/*
The score HUD is game_image_lapCD.brlyt, driven by the lap control through map_number.brctr. Two
of its panes are animated: race_null on Group_00 is the lap number and belongs to the game, and
lap_riight on Group_01 is the score, which is ours. Only the second one may be touched - driving
race_null turns the lap counter into a running total of something else entirely.

The score pane runs game_image_lap_texture_pattern_0_10.brlan, one texture key per frame:
0.0 -> tt_d_number_3d_00 up to 9.0 -> tt_d_number_3d_09, and 10.0 -> tt_d_number_3d_10, the axe.
So a single digit shows 0 to 9 and then the axe takes its place and stays there, while the real
score carries on climbing out of sight.

From that point the SCORE caption goes with it: the axe replaces the whole readout, not just the
number, so the slash pane - which is what carries tt_score_E in this layout - is hidden too. It
is a sibling of the digit, not its parent, so hiding it costs nothing else.

The frame is recomputed from the score on every hit rather than accumulated one step at a time,
which is why the step and the cap the assembly used to apply are both zero: an accumulator drifts
the moment a point arrives from anywhere other than the hit hook.
*/
static const float displayStepPerPoint = 0.0f;
static const float displayCap = 0.0f;
static const u32 scoreAxeAt = 10;
//A countdown race is meant to end on the clock, not on the finish line, so the track length is
//stretched to ten laps. Tracks the author wrote as a single lap - the long point to point ones -
//are already longer than a normal race and keep their own count.
static const u8 raceLapCount = 8;

bool IsEnabled();

u32 GetTimeLimitMs();

//Returns what the lap count should be, given the one the track's STGI asks for. Outside countdown
//and on single lap tracks it hands the KMP value straight back.
u8 GetLapCount(u8 kmpLapCount);

}  // namespace Countdown
}  // namespace Pulsar
#endif
