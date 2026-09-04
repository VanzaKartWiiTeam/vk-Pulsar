#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <PulsarSystem.hpp>
#include <Debug/BetaLog.hpp>
#include <Gamemodes/Countdown/Countdown.hpp>
#include <Extensions/LECODE/Lex.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <MarioKartWii/Item/ItemBehaviour.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <MarioKartWii/UI/Ctrl/UIControl.hpp>


extern "C" {
u32 CountdownActive;
u8 CountdownRaceOver;
u8 CountdownFinalStretch;
u8 CountdownHitCount;
u8 CountdownRacerCount;
void* CountdownTimerObj;
void* CountdownDisplayPtr; //animation frame of lap_riight, the score digit
void* CountdownLapCtrl;    //the CtrlRaceLap, so the SCORE caption can be found by name
float CountdownHitBonus;   //how far the score animation advances per point (one texture)
float CountdownDisplayCap; //the animation stops here: frame 10 is the axe
u32 CountdownFramesBefore; //raceFrameCounter before the hit bonus, so the beta log can show both sides
u32 CountdownEndFrame;
u32 CountdownRampFrame;
u32 CountdownFinalFrame;
u32 CountdownTimeLimitMs;
}

extern "C" void behaviourTable__Q24Item8Behavior(void*);
extern "C" void U32_MUSIC_SPEED(void*);
extern "C" void sInstance__Q24Kart7Manager(void*);
extern "C" void sInstance__Q24Item7Manager(void*);
extern "C" void sInstance__Q25Audio10RSARPlayer(void*);
extern "C" void sInstance__10SectionMgr(void*);
extern "C" void fun_playSound(void*);
extern "C" void EndRaceEarly(void*);
extern "C" void CountdownSymbol1(void);
extern "C" void CountdownSymbol2(void);
extern "C" void OSReport(const char* format, ...);
extern "C" void UseMegaMushroom__Q24Item6PlayerFv(Item::Player* self);

/*
Called from Countdown8 on every scoring hit. It is called unconditionally rather than guarded by
an #ifdef inside the ASM block, because a preprocessor directive cannot live inside the ASM()
macro argument. Outside a beta build the body compiles away and this is one call on a path that
runs a handful of times per race.

The float is passed as raw bits: getting a float into a varargs call from hand written ASM is far
more trouble than reinterpreting it here.
*/
/*
One pane, one digit: frames 0 to 9 are the digits and frame 10 is the axe, which the animation
holds for every score above it. The score itself is not clamped anywhere - it just stops being
readable once the axe is up.

The SCORE caption is hidden at the same moment, so what is left on screen is the axe alone. Both
are driven from the score rather than toggled once, so a score that goes back down - or a fresh
race on the same control - puts the caption back.
*/
static void SetPaneVisible(lyt::Pane* pane, bool visible) {
    if(pane == nullptr) return;
    if(visible) pane->flag |= 0x01;
    else pane->flag &= ~0x01;
}

static void UpdateScoreDisplay(u32 score) {
    const bool axeUp = score >= Pulsar::Countdown::scoreAxeAt;

    float* const digit = static_cast<float*>(CountdownDisplayPtr);
    if(digit != nullptr) {
        *digit = static_cast<float>(axeUp ? Pulsar::Countdown::scoreAxeAt : score);
    }

    LayoutUIControl* const ctrl = static_cast<LayoutUIControl*>(CountdownLapCtrl);
    if(ctrl == nullptr) return;
    //In game_image_lapCD.brlyt the pane called slash is the one textured with tt_score_E.
    SetPaneVisible(ctrl->layout.GetPaneByName("slash"), !axeUp);
}

extern "C" void CountdownHitLog(u32 score, u32 raceFrames, u32 displayBits, u32 capBits) {
    UpdateScoreDisplay(score);
#ifdef BETA
    const u32 before = CountdownFramesBefore;
    OS::Report("[Countdown] HIT score=%d frames %d -> %d (delta %d, want -180) axe=%d\n",
               (int)score, (int)before, (int)raceFrames, (int)raceFrames - (int)before,
               (int)(score >= Pulsar::Countdown::scoreAxeAt));
#endif
}

extern "C" void Countdown4End(void*);
asmFunc Countdown4() {
    ASM(
    nofralloc;
    lis       r4, CountdownActive@ha;
    lwz       r4, CountdownActive@l(r4);
    cmpwi     r4, 0x1;
    bne-      cd4_default;
    lis       r4, CountdownTimeLimitMs@ha;
    lwz       r4, CountdownTimeLimitMs@l(r4);
    b         cd4_done;

cd4_default:
    subi      r4, r3, 0x6C20;

cd4_done:
    b Countdown4End;
    )
}

extern "C" void Countdown5End(void*);
asmFunc Countdown5() {
    ASM(
    nofralloc;
    lis       r4, CountdownTimerObj@ha;
    stw       r3, CountdownTimerObj@l(r4);
    lis       r4, CountdownActive@ha;
    lwz       r4, CountdownActive@l(r4);
    cmpwi     r4, 0x1;
    bne+      cd5_end;
    lis       r11, CountdownFinalStretch@ha;
    lbz       r4, CountdownFinalStretch@l(r11);
    cmpwi     cr6, r4, 0x1;
    lbz       r4, 0x42(r3);
    cmpwi     r4, 0;
    bne+      cd5_tick;
    li        r4, 0x1;
    stb       r4, 0x42(r3);
    li        r4, 0x2;
    stb       r4, 0x15(r3);
    stb       r4, 0x21(r3);
    li        r4, 0x1E;
    stb       r4, 0x16(r3);
    stb       r4, 0x22(r3);
    lis       r4, behaviourTable__Q24Item8Behavior@ha;
    addi      r4, r4, behaviourTable__Q24Item8Behavior@l;
    lwz       r0, 0x14C(r4);
    stw       r0, 0x1A0(r4);
    li        r0, 0;
    stw       r0, 0x74(r4);
    stw       r0, 0xA4(r4);
    stw       r0, 0x1E0(r4);
    li        r0, 0x1;
    stw       r0, 0x84(r4);
    stw       r0, 0x90(r4);
    stw       r0, 0x140(r4);
    stw       r0, 0x148(r4);
    stw       r0, 0x1E8(r4);
    li        r0, 0x2;
    stw       r0, 0x98(r4);
    stw       r0, 0x194(r4);
    stw       r0, 0x1CC(r4);
    li        r0, 0x3;
    stw       r0, 0xA0(r4);
    li        r0, 0x7;
    stw       r0, 0x1C4(r4);
    li        r0, 0x8;
    stw       r0, 0x18C(r4);
    li        r0, 0x9;
    stw       r0, 0x138(r4);
    lwz       r0, 0x88(r4);
    stw       r0, 0x14C(r4);
    b         cd5_end;

cd5_tick:
    lwz       r4, 0x48(r3);
    lis       r10, CountdownRampFrame@ha;
    lwz       r10, CountdownRampFrame@l(r10);
    cmpw      r4, r10;
    blt+      cd5_end;
    lis       r10, CountdownEndFrame@ha;
    lwz       r10, CountdownEndFrame@l(r10);
    cmpw      r4, r10;
    blt-      cd5_ramp;
    bne-      cr6, cd5_full;
    li        r10, 0x1;
    lis       r11, CountdownRaceOver@ha;
    stb       r10, CountdownRaceOver@l(r11);

cd5_full:
    lis       r0, 0x3F80;
    lis       r4, U32_MUSIC_SPEED@ha;
    addi      r4, r4, U32_MUSIC_SPEED@l;
    stw       r0, 0x0(r4);
    b         cd5_end;

cd5_ramp:
    lis       r0, 0x3F8E;
    lis       r4, U32_MUSIC_SPEED@ha;
    addi      r4, r4, U32_MUSIC_SPEED@l;
    lwz       r12, 0x0(r4);
    cmpw      r12, r0;
    beq+      cd5_end;
    addi      r12, r12, 0x3FB;
    cmpw      r12, r0;
    blt+      cd5_store;
    lis       r12, 0x3F8E;

cd5_store:
    stw       r12, 0x0(r4);

cd5_end:
    stwu      r1, -0x40(r1);
    b Countdown5End;
    )
}

extern "C" void Countdown6End(void*);
asmFunc Countdown6() {
    ASM(
    nofralloc;
    stwu      r1, -0x10(r1);
    lis       r12, CountdownActive@ha;
    lwz       r11, CountdownActive@l(r12);
    cmpwi     r11, 0;
    beq-      cd6_end;
    li        r0, 0x1;
    stb       r0, 0xA7(r3);

cd6_end:
    b Countdown6End;
    )
}

extern "C" void Countdown7End(void*);
asmFunc Countdown7() {
    ASM(
    nofralloc;
    lis       r3, CountdownActive@ha;
    lwz       r3, CountdownActive@l(r3);
    cmpwi     r3, 0;
    beq+      cd7_end;
    cmpwi     r25, 0;
    bne-      cd7_end;
    cmpw      r23, r24;
    bne-      cd7_end;
    cmpw      r19, r20;
    bne-      cd7_end;
    lwz       r3, 0x4(r30);
    lwz       r4, 0x0(r31);
    lwz       r3, 0x4(r3);
    lwz       r4, 0x4(r4);
    lwz       r3, 0x4(r3);
    lwz       r4, 0x4(r4);
    rlwinm    r3,r3,6,31,31;
    rlwinm    r4,r4,6,31,31;
    cmpw      r3, r4;
    beq+      cd7_end;
    blt-      cd7_second;
    lis       r3, sInstance__Q24Kart7Manager@ha;
    lwz       r3, sInstance__Q24Kart7Manager@l(r3);
    lwz       r3, 0x20(r3);
    li        r12, -0x1;

cd7_loop1:
    addi      r12, r12, 0x1;
    cmpwi     r12, 0xC;
    beq-      cd7_end;
    mulli     r4, r12, 0x4;
    lwzx      r4, r3, r4;
    lwz       r4, 0x0(r4);
    lwz       r4, 0x30(r4);
    cmpw      r30, r4;
    bne+      cd7_loop1;
    lis       r4, sInstance__Q24Item7Manager@ha;
    lwz       r4, sInstance__Q24Item7Manager@l(r4);
    lwz       r4, 0x14(r4);
    mulli     r3, r12, 0x248;
    add       r4, r3, r4;
    lbz       r3, 0xA7(r4);
    cmpwi     r3, 0x1;
    bne+      cd7_end;
    li        r3, 0;
    stb       r3, 0xA7(r4);
    lwz       r3, 0xA8(r4);
    cmpwi     r3, 0;
    beq+      cd7_jump1;
    li        r3, 0x1;
    stw       r3, 0xA8(r4);

cd7_jump1:
    lis       r12, CountdownSymbol1@ha;
    addi      r12, r12, CountdownSymbol1@l;
    mtctr     r12;
    bctr;

cd7_second:
    lis       r3, sInstance__Q24Kart7Manager@ha;
    lwz       r3, sInstance__Q24Kart7Manager@l(r3);
    lwz       r3, 0x20(r3);
    li        r12, -0x1;

cd7_loop2:
    addi      r12, r12, 0x1;
    cmpwi     r12, 0xC;
    beq-      cd7_end;
    mulli     r4, r12, 0x4;
    lwzx      r4, r3, r4;
    cmpw      r31, r4;
    bne+      cd7_loop2;
    lis       r4, sInstance__Q24Item7Manager@ha;
    lwz       r4, sInstance__Q24Item7Manager@l(r4);
    lwz       r4, 0x14(r4);
    mulli     r3, r12, 0x248;
    add       r4, r3, r4;
    lbz       r3, 0xA7(r4);
    cmpwi     r3, 0x1;
    bne+      cd7_end;
    li        r3, 0;
    stb       r3, 0xA7(r4);
    lwz       r3, 0xA8(r4);
    cmpwi     r3, 0;
    beq+      cd7_jump2;
    li        r3, 0x1;
    stw       r3, 0xA8(r4);

cd7_jump2:
    lis       r12, CountdownSymbol2@ha;
    addi      r12, r12, CountdownSymbol2@l;
    mtctr     r12;
    bctr;

cd7_end:
    cmpwi     r25, 0;
    b Countdown7End;
    )
}

extern "C" void Countdown8End(void*);
asmFunc Countdown8() {
    ASM(
    nofralloc;
    lis       r6, CountdownHitCount@ha;
    stb       r0, CountdownHitCount@l(r6);
    lis       r6, CountdownActive@ha;
    lwz       r7, CountdownActive@l(r6);
    cmpwi     r7, 0x1;
    bne+      cd8_end;
    lis       r7, CountdownTimerObj@ha;
    lwz       r7, CountdownTimerObj@l(r7);
    cmpwi     r7, 0;
    beq-      cd8_display;
    lwz       r8, 0x48(r7);
    lis       r6, CountdownFramesBefore@ha;
    stw       r8, CountdownFramesBefore@l(r6);
    cmpwi     r8, 0xB4;
    bgt+      cd8_sub;
    li        r8, 0;
    stw       r8, 0x48(r7);
    b         cd8_display;

cd8_sub:
    subi      r8, r8, 0xB4;
    stw       r8, 0x48(r7);

cd8_display:
    lis       r7, CountdownHitBonus@ha;
    lfs       f0, CountdownHitBonus@l(r7);
    lis       r7, CountdownDisplayPtr@ha;
    lwz       r7, CountdownDisplayPtr@l(r7);
    cmpwi     r7, 0;
    beq-      cd8_end;
    lfs       f1, 0x0(r7);
    fadds     f1, f0, f1;
    lis       r6, CountdownDisplayCap@ha;
    lwz       r8, CountdownDisplayCap@l(r6);
    cmpwi     r8, 0;
    beq-      cd8_store;
    lfs       f0, CountdownDisplayCap@l(r6);
    fcmpo     cr0, f1, f0;
    ble-      cd8_store;
    fmr       f1, f0;

cd8_store:
    stfs      f1, 0x0(r7);
    mflr      r11;
    stwu      r1, -0x90(r1);
    stw       r0, 0x8C(r1);
    stmw      r3, 0x8(r1);
    mr        r3, r0;
    lis       r4, CountdownTimerObj@ha;
    lwz       r4, CountdownTimerObj@l(r4);
    cmpwi     r4, 0;
    beq-      cd8_noframes;
    lwz       r4, 0x48(r4);
    b         cd8_dolog;

cd8_noframes:
    li        r4, -0x1;

cd8_dolog:
    lwz       r5, 0x0(r7);
    lis       r6, CountdownDisplayCap@ha;
    lwz       r6, CountdownDisplayCap@l(r6);
    lis       r12, CountdownHitLog@h;
    ori       r12, r12, CountdownHitLog@l;
    mtctr     r12;
    bctrl;
    lis       r3, sInstance__Q25Audio10RSARPlayer@ha;
    lwz       r3, sInstance__Q25Audio10RSARPlayer@l(r3);
    li        r4, 0xE7;
    lis       r12, fun_playSound@h;
    ori       r12, r12, fun_playSound@l;
    mtctr     r12;
    bctrl;
    lmw       r3, 0x8(r1);
    lwz       r0, 0x8C(r1);
    addi      r1, r1, 0x90;
    mtlr      r11;

cd8_end:
    stw       r0, 0x8(r4);
    b Countdown8End;
    )
}

extern "C" void Countdown9End(void*);
asmFunc Countdown9() {
    ASM(
    nofralloc;
    lis       r12, CountdownActive@ha;
    lwz       r0, CountdownActive@l(r12);
    cmpwi     r0, 0;
    beq-      cd9_end;
    lis       r12, sInstance__10SectionMgr@ha;
    addi      r12, r12, sInstance__10SectionMgr@l;
    lwz       r0, 0x0(r12);
    cmpwi     r0, 0;
    beq-      cd9_reset;
    mr        r12, r0;
    lwz       r0, 0x0(r12);
    cmpwi     r0, 0;
    beq-      cd9_reset;
    mr        r12, r0;
    lwz       r0, 0x0(r12);
    cmpwi     r0, 0x20;
    beq+      cd9_end;
    cmpwi     r0, 0x70;
    beq+      cd9_end;

cd9_reset:
    lis       r12, 0x3F80;
    stw       r12, 0x208(r31);

cd9_end:
    lfs       f31, 0x208(r31);
    b Countdown9End;
    )
}

extern "C" void Countdown10End(void*);
asmFunc Countdown10() {
    ASM(
    nofralloc;
    lis       r4, CountdownActive@ha;
    lwz       r3, CountdownActive@l(r4);
    cmpwi     r3, 0;
    beq-      cd10_end;

    lis       r4, CountdownLapCtrl@ha;
    stw       r28, CountdownLapCtrl@l(r4);

    lwz       r5, 0x98(r28);
    lwz       r5, 0x44(r5);
    lwz       r5, 0x0(r5);
    li        r3, 0;
    stwu      r3, 0x10(r5);
    lis       r4, CountdownDisplayPtr@ha;
    stw       r5, CountdownDisplayPtr@l(r4);

cd10_end:
    mr        r3, r28;
    b Countdown10End;
    )
}

extern "C" void Countdown11End(void*);
asmFunc Countdown11() {
    ASM(
    nofralloc;
    lis       r12, CountdownActive@ha;
    lwz       r4, CountdownActive@l(r12);
    cmpwi     r4, 0;
    beq-      cd11_end;
    lis       r4, sInstance__Q24Kart7Manager@ha;
    lwz       r4, sInstance__Q24Kart7Manager@l(r4);
    lwz       r4, 0x20(r4);
    li        r6, 0;

cd11_loop:
    lwz       r5, 0x0(r4);
    lwz       r5, 0x10(r5);
    lwz       r5, 0x1C(r5);
    cmpw      r5, r30;
    beq+      cd11_found;
    addi      r6, r6, 0x1;
    addi      r4, r4, 0x4;
    lis       r5, CountdownRacerCount@ha;
    lbz       r5, CountdownRacerCount@l(r5);
    cmpw      r6, r5;
    bge-      cd11_end;
    b         cd11_loop;

cd11_found:
    lis       r11, CountdownTimerObj@ha;
    lwz       r11, CountdownTimerObj@l(r11);
    cmpwi     r11, 0;
    beq-      cd11_end;
    lwz       r11, 0x48(r11);
    lis       r5, CountdownEndFrame@ha;
    lwz       r5, CountdownEndFrame@l(r5);
    cmpw      r11, r5;
    blt-      cd11_end;
    lwz       r3, 0x14(r30);
    ori       r3, r3, 0x800;
    stw       r3, 0x14(r30);
    li        r3, 0;

cd11_end:
    cmpwi     r3, 0;
    b Countdown11End;
    )
}

extern "C" void Countdown12End(void*);
asmFunc Countdown12() {
    ASM(
    nofralloc;
    lis       r11, CountdownActive@ha;
    lwz       r11, CountdownActive@l(r11);
    cmpwi     r11, 0;
    beq-      cd12_default;
    lis       r11, sInstance__Q24Kart7Manager@ha;
    lwz       r11, sInstance__Q24Kart7Manager@l(r11);
    lwz       r11, 0x20(r11);
    li        r3, 0;

cd12_loop:
    lwz       r10, 0x0(r11);
    lwz       r10, 0x10(r10);
    lwz       r10, 0x1C(r10);
    cmpw      r10, r4;
    beq+      cd12_found;
    addi      r3, r3, 0x1;
    addi      r11, r11, 0x4;
    lis       r7, CountdownRacerCount@ha;
    lbz       r7, CountdownRacerCount@l(r7);
    cmpw      r3, r7;
    bge-      cd12_default;
    b         cd12_loop;

cd12_found:
    lis       r8, CountdownTimerObj@ha;
    lwz       r8, CountdownTimerObj@l(r8);
    cmpwi     r8, 0;
    beq-      cd12_default;
    lwz       r8, 0x48(r8);
    lis       r7, CountdownEndFrame@ha;
    lwz       r7, CountdownEndFrame@l(r7);
    cmpw      r8, r7;
    bge-      cd12_skip;

cd12_default:
    and.      r0, r5, r0;

cd12_skip:
    b Countdown12End;
    )
}

extern "C" void Countdown14End(void*);
asmFunc Countdown14() {
    ASM(
    nofralloc;
    lis       r11, CountdownActive@ha;
    lwz       r11, CountdownActive@l(r11);
    cmpwi     r11, 0;
    lbz       r30, 0x3A(r3);
    beq-      cd14_end;
    lis       r11, sInstance__Q24Kart7Manager@ha;
    lwz       r11, sInstance__Q24Kart7Manager@l(r11);
    lwz       r11, 0x20(r11);
    li        r3, 0;

cd14_loop:
    lwz       r10, 0x0(r11);
    lwz       r10, 0x10(r10);
    lwz       r10, 0x1C(r10);
    cmpw      r10, r9;
    beq+      cd14_found;
    addi      r3, r3, 0x1;
    addi      r11, r11, 0x4;
    lis       r4, CountdownRacerCount@ha;
    lbz       r4, CountdownRacerCount@l(r4);
    cmpw      r3, r4;
    bge-      cd14_end;
    b         cd14_loop;

cd14_found:
    lis       r3, CountdownTimerObj@ha;
    lwz       r3, CountdownTimerObj@l(r3);
    cmpwi     r3, 0;
    beq-      cd14_end;
    lwz       r3, 0x48(r3);
    lis       r4, CountdownEndFrame@ha;
    lwz       r4, CountdownEndFrame@l(r4);
    cmpw      r3, r4;
    blt+      cd14_end;
    li        r30, 0;

cd14_end:
    b Countdown14End;
    )
}

extern "C" void Countdown15End(void*);
asmFunc Countdown15() {
    ASM(
    nofralloc;
    lwz       r0, 0x124(r3);
    lis       r12, CountdownRacerCount@ha;
    stb       r0, CountdownRacerCount@l(r12);
    b Countdown15End;
    )
}

extern "C" void Countdown16End(void*);
asmFunc Countdown16() {
    ASM(
    nofralloc;
    lis       r11, CountdownActive@ha;
    lwz       r11, CountdownActive@l(r11);
    cmpwi     cr7, r11, 0;
    cmpwi     r14, 0x7FFF;
    beq-      cr7, cd16_end;
    lis       r11, CountdownTimerObj@ha;
    lwz       r11, CountdownTimerObj@l(r11);
    cmpwi     cr7, r11, 0;
    beq-      cr7, cd16_end;
    lis       r5, CountdownHitCount@ha;
    lbz       r10, CountdownHitCount@l(r5);
    lwz       r11, 0x48(r11);
    mulli     r10, r10, 0xB4;
    add       r11, r11, r10;
    lis       r10, CountdownFinalFrame@ha;
    lwz       r10, CountdownFinalFrame@l(r10);
    cmpw      cr7, r11, r10;
    blt-      cr7, cd16_end;
    li        r11, 0x1;
    lis       r5, CountdownFinalStretch@ha;
    stb       r11, CountdownFinalStretch@l(r5);

cd16_end:
    mr        r3, r30;
    mr        r4, r31;
    b Countdown16End;
    )
}

extern "C" void EndCountdownRaceEnd(void*);
asmFunc EndCountdownRace() {
    ASM(
    nofralloc;
    mflr      r0;
    stwu      r1, -0x80(r1);
    stw       r0, 0x7C(r1);
    stmw      r3, 0x8(r1);
    li        r4, 0x2;
    li        r5, 0x1;
    lis       r12, EndRaceEarly@h;
    ori       r12, r12, EndRaceEarly@l;
    mtctr     r12;
    bctrl;
    lmw       r3, 0x8(r1);
    lwz       r0, 0x7C(r1);
    mtlr      r0;
    addi      r1, r1, 0x80;
    lbz       r0, 0x20(r3);
    b EndCountdownRaceEnd;
    )
}

kmRuntimeUse(0x8053F3BC);
kmRuntimeUse(0x80535904);
kmRuntimeUse(0x8079864C);
kmRuntimeUse(0x8056FFDC);
kmRuntimeUse(0x805918D8);
kmRuntimeUse(0x806F9D1C);
kmRuntimeUse(0x807EF7A4);
kmRuntimeUse(0x805948C4);
kmRuntimeUse(0x805887E0);
kmRuntimeUse(0x80574D78);
kmRuntimeUse(0x80860608);
kmRuntimeUse(0x807308B8);
kmRuntimeUse(0x807308BC);
kmRuntimeUse(0x808A56EC);
kmRuntimeUse(0x8053FC10);
kmRuntimeUse(0x8053F644);
kmRuntimeUse(0x8053F6C0);
kmRuntimeUse(0x805349EC);
kmRuntimeUse(0x807EA790);
kmRuntimeUse(0x80535350);
kmRuntimeUse(0x808A9CC7);
kmRuntimeUse(0x807AF1BC);
kmRuntimeUse(0x80860AF0);

namespace Pulsar {
namespace Countdown {

bool IsEnabled() {
    const System* system = System::sInstance;
    if(system == nullptr) return false;
    return system->IsContext(PULSAR_MODE_COUNTDOWN);
}

u32 GetTimeLimitMs() {
    const System* system = System::sInstance;
    if(system != nullptr) {
        const LECODE::CTDN* ctdn = system->lecodeMgr.lexMgr.ctdn;
        if(ctdn != nullptr) {
            u32 count = ctdn->dataSize / sizeof(u16);
            if(count > 6) count = 6;
            if(count != 0) {
                const u16* limits = ctdn->timeLimit;
                bool uniform = true;
                for(u32 i = 1; i < count; ++i) {
                    if(limits[i] != limits[0]) { uniform = false; break; }
                }
                u32 idx = 0;
                if(!uniform) {
                    idx = 2; //150cc
                    const Racedata* racedata = Racedata::sInstance;
                    if(racedata != nullptr) {
                        switch(racedata->racesScenario.settings.engineClass) {
                            case CC_50: idx = 0; break;
                            case CC_100: idx = system->IsContext(PULSAR_200) ? 3 : 1; break;
                            default: idx = 2; break;
                        }
                    }
                    if(idx >= count) idx = count - 1;
                }
                const u16 seconds = limits[idx];
                if(seconds != 0) return seconds * 1000;
            }
        }
    }
    return (endFrame * 1000) / 60;
}

u8 GetLapCount(u8 kmpLapCount) {
    if(!IsEnabled() || kmpLapCount <= 1) return kmpLapCount;
    PUL_BETA_LOG("[Countdown] lap count %d -> %d\n", (int)kmpLapCount, (int)raceLapCount);
    return raceLapCount;
}

static void RefreshLimits() {
    //Re-established here as well as in ApplyPatches: Countdown8 clamps the score animation against
    //CountdownDisplayCap, so a zero there would freeze the counter instead of capping it.
    CountdownHitBonus = displayStepPerPoint;
    CountdownDisplayCap = displayCap;

    const u32 ms = GetTimeLimitMs();
    const u32 frames = (ms / 1000) * 60;
    CountdownTimeLimitMs = ms;
    CountdownEndFrame = frames;
    CountdownRampFrame = frames > 2700 ? frames - 2700 : 0;
    CountdownFinalFrame = frames + 1800;
}

/*
Thundercloud replacement.

Countdown5 rewrites Item::Behavior::behaviourTable so that the thundercloud slot hands out a
Mega Mushroom instead: entry 14 gets objId OBJ_MEGA_MUSHROOM and the Mega's use function. That
covers every item that goes through the table, but the cloud never does - it auto fires, and
the auto fire path calls Item::Player::UseTC directly, so a real cloud still spawned in
countdown races. Branching UseTC itself catches both paths.

The branch is only installed while the mode is on (see branches[] below), so UseTC is untouched
everywhere else and there is no need to call the original from here.
*/
static void CountdownUseTC(Item::Player* self) {
    PUL_BETA_LOG("[Countdown] UseTC intercepted for player %d -> Mega Mushroom\n", self->id);
    UseMegaMushroom__Q24Item6PlayerFv(self);
}

/*
Item icons.

Countdown rewrites Item::Behavior::behaviourTable, so six slots hand out something other than
what their vanilla icon shows. Rather than shipping new art, each of those slots is pointed at
the pane of the item it actually gives: item.brlyt already carries a pane for every vanilla
item, and every countdown replacement happens to be a vanilla item.

What Countdown5 really writes (offsets decoded against Item::Behavior, 0x1C bytes per entry):
    MUSHROOM           objId OBJ_GREEN_SHELL,    useType FIRE    -> throws one green shell
    TRIPLE_MUSHROOM    objId OBJ_RED_SHELL,      useType CIRCLE  -> three circling red shells
    MEGA_MUSHROOM      objId OBJ_BOBOMB,         useType FIRE    -> throws a bob-omb
    THUNDER_CLOUD      objId OBJ_MEGA_MUSHROOM, Mega's useFunc   -> Mega Mushroom
    TRIPLE_GREEN_SHELL objId OBJ_FAKE_ITEM_BOX                   -> fake item boxes
    TRIPLE_RED_SHELL   objId OBJ_GREEN_SHELL                     -> green shells

The earlier version of this table pointed three of those at Picture_01/02/03 (placeholder
spheres) and two more at textures that only exist swapped inside CTDN.szs. That archive is a
Scene/UI/Race.szs variant which this pack does not load, so those icons were either placeholders
or plain wrong; and swapping the textures for real would change them in every other mode too,
because Race.szs is shared. Naming the right pane costs nothing and is scoped to the mode, since
this function is only branched in while countdown is active.

This replaces GetItemIconPaneName wholesale, so every item has to be listed.
*/
static const char* CountdownItemIconPaneName(ItemId id, u32 count) {
    switch(id) {
        case GREEN_SHELL:        return "kame_green";
        case RED_SHELL:          return "kame_red";
        case BANANA:             return "banana";
        case FAKE_ITEM_BOX:      return "dummybox";
        case MUSHROOM:           return "kame_green";                               //one thrown green shell
        case TRIPLE_MUSHROOM:    return count >= 2 ? "kame_red_3" : "kame_red";     //circling red shells
        case BOBOMB:             return "bomb_hei";
        case BLUE_SHELL:         return "kame_wing";
        case LIGHTNING:          return "thunder";
        case STAR:               return "star";
        case GOLDEN_MUSHROOM:    return "GoldenKinoko";
        case MEGA_MUSHROOM:      return "bomb_hei";                                 //throws a bob-omb
        case BLOOPER:            return "gesso";
        case POW_BLOCK:          return "pow";
        case THUNDER_CLOUD:      return "kinoko_big";                               //Mega Mushroom
        case BULLET_BILL:        return "killer";
        case TRIPLE_GREEN_SHELL: return "dummybox";                                 //fake item boxes
        case TRIPLE_RED_SHELL:   return count >= 2 ? "kame_green_3" : "kame_green"; //green shells
        case TRIPLE_BANANA:      return count >= 2 ? "banana_3" : "banana";
        default:                 return "empty";
    }
}

/*
Wrapper so a beta build can report the icon picked for the six remapped slots. The lookup runs on
every item window refresh, so only a change of item is reported, otherwise the console fills up
with the same line.
*/
static ItemId lastLoggedIconItem = ITEM_NONE;

static bool IsRemappedByCountdown(ItemId id) {
    return id == MUSHROOM || id == TRIPLE_MUSHROOM || id == MEGA_MUSHROOM
        || id == THUNDER_CLOUD || id == TRIPLE_GREEN_SHELL || id == TRIPLE_RED_SHELL;
}

static const char* CountdownItemIconPaneNameHook(ItemId id, u32 count) {
    const char* pane = CountdownItemIconPaneName(id, count);
    if(id != lastLoggedIconItem) {
        lastLoggedIconItem = id;
        if(IsRemappedByCountdown(id)) PUL_BETA_LOG("[Countdown] item 0x%X (count %d) -> %s\n", (int)id, (int)count, pane);
    }
    return pane;
}

struct Patch {
    u32 address;
    u32 patched;
    u32 expected;
};

struct BranchPatch {
    u32 address;
    void* dest;
    u32 expected;
};

static const BranchPatch branches[] ={
    { kmRuntimeAddr(0x8053F3BC), (void*)Countdown4, 0x388393E0 },
    { kmRuntimeAddr(0x80535904), (void*)Countdown5, 0x9421FFC0 },  //stwu r1, -0x40(r1)
    { kmRuntimeAddr(0x8079864C), (void*)Countdown6, 0x9421FFF0 },  //stwu r1, -0x10(r1)
    { kmRuntimeAddr(0x8056FFDC), (void*)Countdown7, 0x2C190000 },  //cmpwi r25, 0
    { kmRuntimeAddr(0x805918D8), (void*)Countdown8, 0x90040008 },  //stw r0, 0x8(r4)
    { kmRuntimeAddr(0x806F9D1C), (void*)Countdown9, 0xC3FF0208 },  //lfs f31, 0x208(r31)
    { kmRuntimeAddr(0x807EF7A4), (void*)Countdown10, 0x7F83E378 }, //mr r3, r28
    { kmRuntimeAddr(0x805948C4), (void*)Countdown11, 0x2C030000 }, //cmpwi r3, 0
    { kmRuntimeAddr(0x805887E0), (void*)Countdown12, 0x7CA00039 }, //and. r0, r5, r0
    { kmRuntimeAddr(0x80574D78), (void*)Countdown14, 0x8BC3003A }, //lbz r30, 0x3A(r3)
    { kmRuntimeAddr(0x80860608), (void*)Countdown15, 0x80030124 }, //lwz r0, 0x124(r3)
    { kmRuntimeAddr(0x807308B8), (void*)Countdown16, 0x7FC3F378 }, //mr r3, r30
    //No expected instruction for these two: they replace a whole function, so the first word is
    //whatever the compiler emitted and there is nothing meaningful to validate against.
    { kmRuntimeAddr(0x807AF1BC), (void*)CountdownUseTC, 0 },              //Item::Player::UseTC
    { kmRuntimeAddr(0x80860AF0), (void*)CountdownItemIconPaneNameHook, 0 } //GetItemIconPaneName
};
static const u32 branchCount = sizeof(branches) / sizeof(branches[0]);

static const Patch writes[] ={
    { kmRuntimeAddr(0x807308BC), 0x40800008, 0x7FE4FB78 }, //bge cr0, +8: consuma il cmpwi di Countdown16
    { kmRuntimeAddr(0x808A56EC), 0x00000016, 0x00000006 },
    { kmRuntimeAddr(0x8053FC10), 0x60000000, 0 },
    { kmRuntimeAddr(0x8053F644), 0x4800006C, 0x4182006C },
    { kmRuntimeAddr(0x8053F6C0), 0x48000030, 0x41820030 },
    { kmRuntimeAddr(0x805349EC), 0x60000000, 0x40820274 },
    { kmRuntimeAddr(0x807EA790), 0x38000001, 0x80630000 }
};
static const u32 writeCount = sizeof(writes) / sizeof(writes[0]);

static u32 originalBranches[branchCount];
static u32 originalWrites[writeCount];
static u32 originalEndRace;
static u8 originalHudNameByte;
//Countdown5 rewrites the item behaviour table in place and nothing ever put it back, so the
//remapped items leaked into every later race of the session: a leaked entry sends an item to an
//object id that race never loaded, and the first shoot faults on a null pointer.
static Item::Behavior originalBehaviours[19];
static bool capturedBehaviours = false;
static bool capturedOriginals = false;
static bool sitesValid = false;
static bool patchesApplied = false;
static bool endRacePatchInstalled = false;

/*
The snapshot has to be taken while the table is both initialised and unpatched, and neither is
guaranteed at the moment the mode is switched on: the table is filled at runtime, so an early
page load can see it still empty, and from the second countdown race on Countdown5 has already
rewritten it. Copying at the wrong moment handed the restore a table of nulls, which is exactly
what the next race then called into.

So the capture is attempted repeatedly and only accepted once the table reads as vanilla. The
check covers every entry Countdown5 touches plus three it never does, so a patched table and an
empty one are both rejected and the attempt is simply retried at the next race load.
*/
static bool IsBehaviourTableVanilla() {
    const Item::Behavior* table = Item::Behavior::behaviourTable;
    return table[MUSHROOM].objId == OBJ_MUSHROOM
        && table[TRIPLE_MUSHROOM].objId == OBJ_MUSHROOM
        && table[MEGA_MUSHROOM].objId == OBJ_MEGA_MUSHROOM
        && table[THUNDER_CLOUD].objId == OBJ_THUNDER_CLOUD
        && table[TRIPLE_GREEN_SHELL].objId == OBJ_GREEN_SHELL
        && table[TRIPLE_RED_SHELL].objId == OBJ_RED_SHELL
        && table[BANANA].objId == OBJ_BANANA
        && table[STAR].objId == OBJ_STAR
        && table[BULLET_BILL].objId == OBJ_BULLET_BILL;
}

static void CaptureBehaviours() {
    if(capturedBehaviours) return;
    if(!IsBehaviourTableVanilla()) {
        PUL_BETA_LOG("[Countdown] item behaviour table not vanilla yet, snapshot postponed\n");
        return;
    }
    capturedBehaviours = true;
    for(u32 i = 0; i < 19; ++i) originalBehaviours[i] = Item::Behavior::behaviourTable[i];
    PUL_BETA_LOG("[Countdown] item behaviour table snapshotted\n");
}

static void RestoreBehaviours() {
    if(!capturedBehaviours) return;
    for(u32 i = 0; i < 19; ++i) Item::Behavior::behaviourTable[i] = originalBehaviours[i];
}

/*
KamekRuntimeWrite::Branch refuses anything outside a 26 bit displacement and reports it by
returning false, which the install loop used to throw away. A mode running with some of its hooks
missing is worse than one that does not run at all, because each patched function assumes the
others are there, so the distances are checked up front and a failure disables the mode like any
other bad site.
*/
static bool IsBranchReachable(u32 from, u32 to) {
    const s32 delta = (s32)to - (s32)from;
    return delta >= -0x02000000 && delta <= 0x01FFFFFC;
}

static bool AreHooksReachable() {
    bool reachable = true;
    for(u32 i = 0; i < branchCount; ++i) {
        if(!IsBranchReachable(branches[i].address, (u32)branches[i].dest)) {
            OSReport("[Countdown] hook 0x%08X -> 0x%08X is out of branch range\n", branches[i].address, (u32)branches[i].dest);
            reachable = false;
        }
    }
    if(!IsBranchReachable(kmRuntimeAddr(0x80535350), (u32)EndCountdownRace)) {
        OSReport("[Countdown] EndRace hook is out of branch range\n");
        reachable = false;
    }
    return reachable;
}

static void CaptureOriginals() {
    if(capturedOriginals) return;
    capturedOriginals = true;
    sitesValid = true;

    for(u32 i = 0; i < branchCount; ++i) {
        const u32 original = *(volatile u32*)branches[i].address;
        originalBranches[i] = original;
        if(branches[i].expected != 0 && original != branches[i].expected) {
            OSReport("[Countdown] hook site 0x%08X: expected 0x%08X, found 0x%08X\n", branches[i].address, branches[i].expected, original);
            sitesValid = false;
        }
    }
    for(u32 i = 0; i < writeCount; ++i) {
        const u32 original = *(volatile u32*)writes[i].address;
        originalWrites[i] = original;
        if(writes[i].expected != 0 && original != writes[i].expected) {
            OSReport("[Countdown] patch site 0x%08X: expected 0x%08X, found 0x%08X\n", writes[i].address, writes[i].expected, original);
            sitesValid = false;
        }
    }
    originalEndRace = *(volatile u32*)kmRuntimeAddr(0x80535350);
    if(originalEndRace != 0x88030020) {
        OSReport("[Countdown] EndRace site 0x%08X: expected 0x88030020, found 0x%08X\n", kmRuntimeAddr(0x80535350), originalEndRace);
        sitesValid = false;
    }
    originalHudNameByte = *(volatile u8*)kmRuntimeAddr(0x808A9CC7);
    if(originalHudNameByte != 'l') {
        OSReport("[Countdown] HUD name 0x%08X: expected 'l', found 0x%02X\n", kmRuntimeAddr(0x808A9CC7), originalHudNameByte);
        sitesValid = false;
    }
    if(!AreHooksReachable()) sitesValid = false;
    if(!sitesValid) OSReport("[Countdown] mode disabled: the addresses do not match this version of the game\n");
}

static void ApplyPatches(bool enable) {
    CaptureOriginals();
    if(!sitesValid) enable = false;
    if(enable == patchesApplied) return;
    patchesApplied = enable;

    if(enable) {
        CaptureBehaviours();
        CountdownRaceOver = 0;
        CountdownFinalStretch = 0;
        CountdownHitCount = 0;
        CountdownTimerObj = nullptr;
        CountdownDisplayPtr = nullptr;
        CountdownLapCtrl = nullptr;
        //One point, one texture. This used to be hitBonusFrames / 60, ie 3.0f, which is why a
        //single hit jumped the counter by three: the value doubles as the frame step of the
        //score animation, not as the time bonus. The time bonus is still 0xB4 frames, applied
        //directly in Countdown8.
        CountdownHitBonus = displayStepPerPoint;
        CountdownDisplayCap = displayCap;
        RefreshLimits();
        CountdownActive = 1;
        for(u32 i = 0; i < branchCount; ++i) KamekRuntimeWrite::Branch(branches[i].address, (u32)branches[i].dest, false);
        for(u32 i = 0; i < writeCount; ++i) KamekRuntimeWrite::Write32(writes[i].address, writes[i].patched);
        KamekRuntimeWrite::Write8(kmRuntimeAddr(0x808A9CC7), 'm'); //lap_number -> map_number
        PUL_BETA_LOG("[Countdown] patches ON, %d branches, step %d.0 point(s), cap %d\n",
                     (int)branchCount, (int)displayStepPerPoint, (int)displayCap);
    }
    else {
        CountdownActive = 0;
        for(u32 i = 0; i < branchCount; ++i) KamekRuntimeWrite::Write32(branches[i].address, originalBranches[i]);
        for(u32 i = 0; i < writeCount; ++i) KamekRuntimeWrite::Write32(writes[i].address, originalWrites[i]);
        KamekRuntimeWrite::Write8(kmRuntimeAddr(0x808A9CC7), originalHudNameByte);
        KamekRuntimeWrite::Write32(kmRuntimeAddr(0x80535350), originalEndRace);
        RestoreBehaviours();
        //Both point into the race heap, which is about to be torn down.
        CountdownTimerObj = nullptr;
        CountdownDisplayPtr = nullptr;
        CountdownLapCtrl = nullptr;
        endRacePatchInstalled = false;
        CountdownRaceOver = 0;
        PUL_BETA_LOG("[Countdown] patches OFF, item behaviour table restored\n");
    }
}

//Runs on every page load, so it stays quiet: ApplyPatches already reports the transitions.
static void Toggle() {
    ApplyPatches(IsEnabled());
}
static PageLoadHook CountdownToggle(Toggle);

static void ResetForNewRace() {
    /*
        Unconditional, and ahead of the early return: whatever Countdown5 wrote during the previous
        race is undone here, so a race that is not a countdown race can never inherit it. Waiting
        for the mode to be switched off was not enough - the player can leave countdown through a
        path that never reaches a page load carrying the new context.
    */
    CaptureBehaviours();
    RestoreBehaviours();

    if(!patchesApplied) return;
    PUL_BETA_LOG("[Countdown] race start, score reset\n");
    CountdownRaceOver = 0;
    CountdownFinalStretch = 0;
    CountdownHitCount = 0;
    CountdownTimerObj = nullptr;
    CountdownDisplayPtr = nullptr;
    CountdownLapCtrl = nullptr;
    if(endRacePatchInstalled) {
        KamekRuntimeWrite::Write32(kmRuntimeAddr(0x80535350), originalEndRace);
        endRacePatchInstalled = false;
    }
    RefreshLimits();
}
static RaceLoadHook CountdownRaceStart(ResetForNewRace);

static void EndRaceWhenCountdownEnds() {
    if(!patchesApplied) return;
    RefreshLimits();

    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if(sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    const Page* page = sectionMgr->curSection->GetTopLayerPage();
    if(page == nullptr) return;

    const PageId id = page->pageId;
    const bool isRacePage = id >= 0x0C && id <= 0x19;
    const bool shouldEndRace = CountdownRaceOver != 0 && isRacePage;
    if(shouldEndRace == endRacePatchInstalled) return;
    endRacePatchInstalled = shouldEndRace;

    if(shouldEndRace) KamekRuntimeWrite::Branch(kmRuntimeAddr(0x80535350), (u32)EndCountdownRace, false);
    else KamekRuntimeWrite::Write32(kmRuntimeAddr(0x80535350), originalEndRace);
}
static PageLoadHook2 CountdownRaceEnd(EndRaceWhenCountdownEnds);

}  // namespace Countdown
}  // namespace Pulsar
