#include <kamek.hpp>
#include <core/rvl/DWC/DWC.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Network {
//Region Patch (Leseratte)
static void PatchLoginRegion() {
    u32 region = System::sInstance->GetInfo().GetWiimmfiRegion();
    char path[0x9];
    snprintf(path, 0x9, "%08d", region + 100000);
    for(int i = 0; i < 8; ++i) {
        DWC::loginRegion[i] = path[i];
    }
}
BootHook LoginRegion(PatchLoginRegion, 2);


//PatchRegion(0x8065920c);
//PatchRegion(0x80659260);
//PatchRegion(0x80659724);
//PatchRegion(0x80659778);

/*
    Builds the DWC matchmaking key ("vs_%d"/"bt_%d") both for the QR2 record this console
    publishes and for the filter it searches with. This has to be the region of the mode that
    was picked (REGIONID), not the pack's base region: with the base region every mode ended up
    publishing "vs_104", so Item Rain, 200cc and OTT players were matched into each other's
    rooms and the server only ever saw them as plain VS. UI/PlayerCount.cpp already reads this
    key back expecting the per-mode value.
*/
int PatchRegion(char* path, u32 len, const char* fmt, const char* mode) {
    return snprintf(path, len, fmt, mode, REGIONID);
}
kmCall(0x8065921c, PatchRegion);
kmCall(0x80659270, PatchRegion);
kmCall(0x80659734, PatchRegion);
kmCall(0x80659788, PatchRegion);


//kmWrite32(0x8065a038, 0x7C050378);
//kmWrite32(0x8065a084, 0x7C050378);
/*
    Joining a friend has to stay possible across the mode regions, so both sides are checked
    against the whole set. REGIONID, not the base region, is what this console is actually in.

    friendRegionId lives in a register handed over by the asm above, so it is only ever compared
    against constants here - never passed to a function, which could clobber it if the compiler
    picked a volatile register and did not inline the callee. rr-pulsar writes the same check out
    the same way; that is why IsModeRegion is not used in this one place.
*/
static int GetFriendsSearchType(int curType, u32 regionId) {
    register u8 friendRegionId;
    asm(mr friendRegionId, r0;);
    if (REGIONID == REGION_VS || REGIONID == REGION_OTT || REGIONID == REGION_200CC || REGIONID == REGION_ITEMRAIN ||
        friendRegionId == REGION_VS || friendRegionId == REGION_OTT || friendRegionId == REGION_200CC || friendRegionId == REGION_ITEMRAIN) {
        if (curType == 7) return 6;
        return 9;
    }
    if(REGIONID != friendRegionId) return curType;
    else if(curType == 7) return 6;
    else return 9;
}
kmBranch(0x8065a03c, GetFriendsSearchType);
kmBranch(0x8065a088, GetFriendsSearchType);



static u32 PatchRKNetControllerRegion() {
    return REGIONID;
}
kmCall(0x80653640, PatchRKNetControllerRegion);
kmWrite32(0x80653644, 0x7c651b78);
kmCall(0x806536ac, PatchRKNetControllerRegion); //for battle
kmWrite32(0x806536b0, 0x7c661b78);

//kmCall(0x80653700, PatchRKNetControllerRegion); this is for battle, right now it'll store 2 (if pal)/FF



//kmWrite32(0x8065A034, 0x3880008E);
//kmWrite32(0x8065A080, 0x3880008E);

}//namespace Network
}//namespace Pulsar