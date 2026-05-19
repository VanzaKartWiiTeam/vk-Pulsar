/**
 * Common Gecko Codes for MKWii
 * 
 * Collection of popular Gecko codes converted to Kamek format.
 * Uncomment the ones you want to use!
 */

#include <kamek.hpp>

namespace Pulsar {
namespace Codes {

// ═══════════════════════════════════════════════════════════
// 🎨 VISUAL / UI CODES
// ═══════════════════════════════════════════════════════════

// Remove Background Blur [Davidevgen]
kmWrite32(0x80258184, 0x30);

// Disable Camera Shaking from Bombs [ZPL]
kmWrite32(0x805a906c, 0x4E800020);

// Show Position in Live View [MrBean35000vr]
kmWrite32(0x806335B0, 0x3860021A);

// CPUs/Online Players Have High Detail Particles [Ro]
// kmWrite32(0x8068E494, 0x38600001);

// No Shell Tail Dissolve [Ro]
// kmWrite32(0x8068DD68, 0x38600000);

kmWrite32(0x80895CC0, 0x00FF00FD);
kmWrite32(0x80895CC4, 0x00FF00FD);
kmWrite32(0x80895CC8, 0x00FF00FD);
kmWrite32(0x80895CCC, 0x00FF00FD);
kmWrite32(0x80895CD0, 0x00FF00FD);
kmWrite32(0x80895CD4, 0x00FF0046);
kmWrite32(0x80895CD8, 0x00FF00FF);
kmWrite32(0x80895CDC, 0x00FF00FF);
kmWrite32(0x80895CE0, 0x00FF00FF);
kmWrite32(0x80895CE4, 0x00FF00FF);
kmWrite32(0x80895CE8, 0x00FF00FF);
kmWrite32(0x80895CEC, 0x00FF0046);

// ═══════════════════════════════════════════════════════════
// 🎮 GAMEPLAY CODES
// ═══════════════════════════════════════════════════════════

// Mushroom Glitch Fix [Vabold]
kmWrite8(0x807BA077, 0x00);

// Instant Voting Roulette Decide [Ro]
// Makes track voting instant
kmWrite32(0x80643BC4, 0x60000000);
kmWrite32(0x80643C2C, 0x60000000);

// Force player to not be penalized [B_squo]
// kmWrite32(0x80549898, 0x38600000);
// kmWrite32(0x8054989c, 0x4E800020);

// Allow Pausing Before Race Starts [Sponge]
kmWrite32(0x80856a28, 0x40810050);

// Fix star offroad glitch after cannon [Ro]
asmFunc StarOffroadFix() {
    ASM(
        nofralloc;
        andi.r11, r0, 0x80;
        andis.r12, r0, 0x8000;
        or.r0, r11, r12;
        blr;)
}
kmCall(0x8057C3F8, StarOffroadFix);

// ═══════════════════════════════════════════════════════════
// 🌐 ONLINE CODES
// ═══════════════════════════════════════════════════════════

// No Disconnect on Countdown [_tZ]
// Prevents disconnect during countdown
kmWrite32(0x80655578, 0x60000000);

// Prevent Lag Abuse [???]
// kmWrite32(0x80654b00, 0x4E800020);

// Disable 6 minute time limit Online [CLF78]
// kmWrite32(0x8053F478, 0x4800000C);

// Ultra Uncut [MrBean35000vr + Chadderz]
asmFunc GetUltraUncut() {
    ASM(
        nofralloc;
        loc_0x0 : lbz r3, 0x1C(r29);
        cmplwi r3, 0x1;
        ble + loc_0x10;
        mr r0, r30;

        loc_0x10 : cmplw r30, r0;
        blr;)
}
kmCall(0x8053511C, GetUltraUncut);

// Allow WFC on Wiimmfi Patched ISOs
kmWrite32(0x800EE3A0, 0x2C030000);
kmWrite32(0x800ECAAC, 0x7C7E1B78);

// Fix Unfocused Small Mii Icon Border [B_squo]
kmWrite32(0x807eb774, 0x2c000017);

// Cancel Friend Room Joining by Pressing B [Ro]
extern "C" void ptr_inputBase(void*);
asmFunc friendRoomJoinCancel() {
    ASM(
        nofralloc;
        lis r31, ptr_inputBase @ha;
        lwz r31, ptr_inputBase @l(r31);
        lhz r31, 0x60(r31);
        andi.r31, r31, 0x2;
        beq end;

        li r3, 3;

        end :;
        cmpwi r3, 3;
        blr;)
}
kmCall(0x805DD85C, friendRoomJoinCancel);

// ═══════════════════════════════════════════════════════════
// 📊 VR SYSTEM CHANGES
// ═══════════════════════════════════════════════════════════

// Change VR Limit to 30000 [XeR]
// Default is 9999

kmWrite16(0x8052D286, 0x00007530);
kmWrite16(0x8052D28E, 0x00007530);
kmWrite16(0x8064F6DA, 0x00007530);
kmWrite16(0x8064F6E6, 0x00007530);
kmWrite16(0x8085654E, 0x00007530);
kmWrite16(0x80856556, 0x00007530);
kmWrite16(0x8085C23E, 0x00007530);
kmWrite16(0x8085C246, 0x00007530);
kmWrite16(0x8064F76A, 0x00007530);
kmWrite16(0x8064F776, 0x00007530);
kmWrite16(0x808565BA, 0x00007530);
kmWrite16(0x808565C2, 0x00007530);
kmWrite16(0x8085C322, 0x00007530);
kmWrite16(0x8085C32A, 0x00007530);

}  // namespace Codes
}  // namespace Pulsar
