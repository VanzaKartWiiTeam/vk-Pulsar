#include <kamek.hpp>


SectionLoadHook* SectionLoadHook::sHooks = nullptr;
DoFuncsHook* RaceLoadHook::raceLoadHooks = nullptr;
DoFuncsHook* RaceFrameHook::raceFrameHooks = nullptr;

nw4r::ut::List BootHook::list = { nullptr, nullptr, 0, offsetof(BootHook, link) };

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
// The generic E mapping for 0x80543BB4 is incorrect for the NTSC-U DOL used
// by Riivolution. Use explicit per-region boot points so BootHook::Exec runs
// at the original initialization time on every supported region.
kmRegionCall(0x80543BB4, BootHook::Exec, 'P');
kmRegionCall(0x80543BB4, BootHook::Exec, 'E');
kmRegionCall(0x80543534, BootHook::Exec, 'J');
kmRegionCall(0x80531C0C, BootHook::Exec, 'K');
kmBranch(0x80554728, RaceLoadHook::Exec);
kmBranch(0x8053369c, RaceFrameHook::Exec); //Raceinfo::Update()

kmBranch(0x8063507c, SectionLoadHook::Exec);

