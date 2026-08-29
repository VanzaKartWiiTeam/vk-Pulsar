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

} // namespace VanzaKart

