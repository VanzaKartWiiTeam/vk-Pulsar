#include <kamek.hpp>


SectionLoadHook* SectionLoadHook::sHooks = nullptr;
DoFuncsHook* RaceLoadHook::raceLoadHooks = nullptr;
DoFuncsHook* RaceFrameHook::raceFrameHooks = nullptr;
DoFuncsHook* PageLoadHook::pageLoadHooks = nullptr;
DoFuncsHook* PageLoadHook2::pageLoadHooks2 = nullptr;

nw4r::ut::List BootHook::list = { nullptr, nullptr, 0, offsetof(BootHook, link) };
bool BootHook::executed = false;
bool BootHook::executedFromFallback = false;

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
    OSReport("[VK] BootHookEntry: starting, %d BootHooks in list\n", (int)BootHook::list.count);
    BootHook::Exec();
    OSReport("[VK] BootHookEntry: done\n");
    return section; //replaces the `mr r3, r31` that the hook overwrote
}
// The generic E mapping for this address is incorrect for the NTSC-U DOL used
// by Riivolution. Use explicit per-region boot points so BootHook::Exec runs
// at the original initialization time on every supported region.
kmRegionCall(0x80543BB8, BootHookEntry, 'P');
//0x8053E67C, not the PAL address: kmRegionCall takes an address already native to its
//region, so the PAL value landed on 9421FFF0 in the NTSC-U module -- the prologue of an
//unrelated function, whose stack frame setup was then replaced by this call. That both
//skipped the real boot point and corrupted that function, crashing RMCE on the title
//screen. 8053E67C is where versions.txt maps the PAL boot point, and it holds the same
//mr r3, r31 with the same surrounding code, exactly like the J and K entries below.
kmRegionCall(0x8053E67C, BootHookEntry, 'E');
kmRegionCall(0x80543538, BootHookEntry, 'J');
kmRegionCall(0x80531C10, BootHookEntry, 'K');
kmBranch(0x80554728, RaceLoadHook::Exec);
kmBranch(0x8053369c, RaceFrameHook::Exec); //Raceinfo::Update()

kmBranch(0x8063507c, SectionLoadHook::Exec);

kmBranch(0x80601c04, PageLoadHook::Exec);
kmBranch(0x80601c60, PageLoadHook2::Exec);

