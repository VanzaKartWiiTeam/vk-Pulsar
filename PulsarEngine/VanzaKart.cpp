#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <runtimeWrite.hpp>
#include <Dolphin/DolphinIOS.hpp>
#include <VanzaKart.hpp>

namespace VanzaKart {

// Mission Mode Fix flag - controls transmission system
u16 U16_MISSION_MODE_FIX = 0;

// Hide the channel button on the title screen / main menu on emulator only
kmRuntimeUse(0x80625E1C);
void HideChannelButton() {
    kmRuntimeWrite32A(0x80625E1C, 0x38800004);
    if (Dolphin::IsEmulator()) {
        kmRuntimeWrite32A(0x80625E1C, 0x38800003);
    }
}
static SectionLoadHook hideChannelButton(HideChannelButton);

// Initialize transmission array or sum idk lol
Pulsar::System* System::Create() {
    System* system = new System();
    
    // Initialize all transmissions to default
    for(int i = 0; i < 12; ++i) {
        system->transmissions[i] = TRANSMISSION_DEFAULT;
    }
    
    return system;
}

/*
This registration is what makes the factory above run at all, and without it the whole
transmission system was writing outside its own object.

Pulsar::System::CreateSystem only calls a factory when System::inherit is set; nothing set it, so
it fell through to `new System()` and allocated a plain Pulsar::System - 0xE0 bytes.
VanzaKart::System is 0x114 and places transmissions[12] at offset 0xE4, so every access to that
array was 0x34 bytes past the end of the allocation. What sits there is the heap block header of
the next allocation and the EGG::TaskThread that Pulsar::System's own constructor creates
immediately after itself.

The damage stayed hidden for a long time because it was so narrow: SetCPUKartType only writes a
slot that currently reads as TRANSMISSION_DEFAULT, so it skipped every word that happened to be
non zero - the vtable, the heap pointer, the thread pointer - and wrote its random 1 or 2 into
the only two words there that are legitimately zero, the head and tail of the task thread's send
queue. The thread then faulted inside OSWakeupThread the next time it was handed a task, minutes
later and several menus away from the race that caused it.
*/
static Pulsar::System::Inherit vanzaKartSystem(System::Create);

} // namespace VanzaKart

