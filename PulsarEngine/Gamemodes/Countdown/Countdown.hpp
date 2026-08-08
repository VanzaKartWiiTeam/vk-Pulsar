#ifndef _PULSAR_COUNTDOWN_
#define _PULSAR_COUNTDOWN_
#include <kamek.hpp>

namespace Pulsar {
namespace Countdown {

static const u32 endFrame = 0x2328;
static const u32 musicRampFrame = 0x189C;
static const u32 hitBonusFrames = 0xB4;

bool IsEnabled();

u32 GetTimeLimitMs();

}  // namespace Countdown
}  // namespace Pulsar
#endif
