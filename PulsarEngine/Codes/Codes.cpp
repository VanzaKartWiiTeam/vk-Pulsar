/**
 * Common Gecko Codes for MKWii
 * 
 * Collection of popular Gecko codes converted to Kamek format.
 * Uncomment the ones you want to use!
 */

#include <kamek.hpp>

namespace Pulsar {
namespace Codes {

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

//Remove WW Button [Chadderz]
kmWrite16(0x8064B982, 0x00000005);
kmWrite32(0x8064BA10, 0x60000000);
kmWrite32(0x8064BA38, 0x60000000);
kmWrite32(0x8064BA50, 0x60000000);
kmWrite32(0x8064BA5C, 0x60000000);
kmWrite16(0x8064BC12, 0x00000001);
kmWrite16(0x8064BC3E, 0x00000484);
kmWrite16(0x8064BC4E, 0x000010D7);
kmWrite16(0x8064BCB6, 0x00000484);
kmWrite16(0x8064BCC2, 0x000010D7);

// Disable Data Save Reset for Region ID Change [Vega]
// Without this the game compares the saved region id against the current one and, on a
// mismatch, flags the save for reset. That wipes the license data (GetFavouriteCharacter
// then returns -1) and leaves the menus initialising from an invalid save.
// Present in rr-pulsar (Extra/Extra.cpp) and in the VanzaKart stable build; it was the
// only hook the stable had that this branch was missing.
kmWrite32(0x80544928, 0x7C601B78);

}  // namespace Codes
}  // namespace Pulsar
