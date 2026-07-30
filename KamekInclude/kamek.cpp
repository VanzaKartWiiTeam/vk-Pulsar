#include <kamek.hpp>


SectionLoadHook* SectionLoadHook::sHooks = nullptr;
DoFuncsHook* RaceLoadHook::raceLoadHooks = nullptr;
DoFuncsHook* RaceFrameHook::raceFrameHooks = nullptr;

nw4r::ut::List BootHook::list = { nullptr, nullptr, 0, offsetof(BootHook, link) };
bool BootHook::executed = false;

DoFuncsHook::DoFuncsHook(Func& f, DoFuncsHook** prev) : func(f) {
    next = *prev;
    *prev = this;
}

void DoFuncsHook::Exec(DoFuncsHook* first) {
    for(DoFuncsHook* p = first; p; p = p->next) {
        p->func();
    }
}


//kmBranch(0x80001500, BootHook2::Exec);
// The instruction at 0x80543BB4 (P) is a `bl` to a main.dol import, so StaticR.rel
// carries a relocation entry for it. OSLink applies that relocation after Kamek has
// written its patch, silently restoring the original call: BootHook::Exec never ran
// and System::sInstance stayed null, which crashed every Pulsar page.
// The next instruction, `mr r3, r31`, has no relocation entry, so hook that instead
// and reproduce it by returning r31 in r3. Execution timing is unchanged.
extern "C" void OSReport(const char* format, ...);

static void* BootHookEntry() {
    register void* section;
    asm(mr section, r31;);
    OSReport("[VK] BootHookEntry: parte, %d BootHook in lista\n", (int)BootHook::list.count);
    BootHook::Exec();
    OSReport("[VK] BootHookEntry: completato\n");
    return section; //replaces the `mr r3, r31` that the hook overwrote
}
// The generic E mapping for this address is incorrect for the NTSC-U DOL used
// by Riivolution. Use explicit per-region boot points so BootHook::Exec runs
// at the original initialization time on every supported region.
kmRegionCall(0x80543BB8, BootHookEntry, 'P');
kmRegionCall(0x80543BB8, BootHookEntry, 'E');
kmRegionCall(0x80543538, BootHookEntry, 'J');
kmRegionCall(0x80531C10, BootHookEntry, 'K');
kmBranch(0x80554728, RaceLoadHook::Exec);
kmBranch(0x8053369c, RaceFrameHook::Exec); //Raceinfo::Update()

kmBranch(0x8063507c, SectionLoadHook::Exec);

