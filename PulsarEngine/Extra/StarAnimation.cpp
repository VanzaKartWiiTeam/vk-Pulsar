#include <kamek.hpp>
#include <Debug/BetaLog.hpp>
#include <Race/RoomContext.hpp>

namespace VanzaKart {

/*
Star Animation [The Insane Kart Wii Team]
Improves the star item animation for a better visual effect.

Mogi mode has to look exactly like vanilla, so the tweak is gated behind a flag the ASM reads
instead of a branch into C++: the hook site is a kmCall replacing a single load, and calling out
from there would mean saving and restoring registers for what is one byte of state.

The flag is stored inverted on purpose. It lives in .bss, so before the first page load it reads
zero, and zero has to mean "behave like the build always did".
*/
extern "C" {
u8 StarAnimationDisabled;
}

asmFunc StarAnimation() {
    ASM(
        nofralloc;
        lhz r0, 0xF6(r31);
        lis r12, StarAnimationDisabled@ha;
        lbz r12, StarAnimationDisabled@l(r12);
        cmpwi r12, 0x0;
        bne- loc_0x20;
        lwz r12, 0x0(r31);
        lwz r12, 0x4(r12);
        lwz r12, 0x8(r12);
        andis. r12, r12, 0x8000;
        beq- loc_0x20;
        li r0, 0x8;
        sth r0, 0xF6(r31);

    loc_0x20:
        blr;
    )
}

kmCall(0x807CD2DC, StarAnimation);

static void UpdateStarAnimation() {
    const u8 disabled = RoomContext::IsMogi() ? 1 : 0;
    if(disabled != StarAnimationDisabled) {
        PUL_BETA_LOG("[StarAnimation] disabled=%d (mogi)\n", (int)disabled);
    }
    StarAnimationDisabled = disabled;
}
static PageLoadHook StarAnimationToggle(UpdateStarAnimation);
static RaceLoadHook StarAnimationRaceToggle(UpdateStarAnimation);

} // namespace VanzaKart
