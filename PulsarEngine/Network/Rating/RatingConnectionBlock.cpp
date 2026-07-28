#include <kamek.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>

namespace Pulsar {
namespace Network {

extern "C" void DWC_LoginAsync(wchar_t* miiName, int unk, void* callback, RKNet::Controller* self);

// Bind the active license to its server profile before login.  High custom ratings
// must never block DWC login; the stock u16 fields are only a compatibility mirror.
static void BindRatingProfileAndLogin(wchar_t* miiName, int unk, void* callback, RKNet::Controller* self) {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys != nullptr && rksys->curLicenseId < 4) {
        const u32 licenseId = rksys->curLicenseId;
        const s32 profileId = rksys->licenses[licenseId].dwcAccUserData.gsProfileId;
        PointRating::BindLicenseProfileId(licenseId, profileId);
    }
    DWC_LoginAsync(miiName, unk, callback, self);
}
kmCall(0x80658cdc, BindRatingProfileAndLogin);

} // namespace Network
} // namespace Pulsar