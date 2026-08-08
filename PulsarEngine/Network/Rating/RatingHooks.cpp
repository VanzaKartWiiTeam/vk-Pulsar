#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <include/c_stdio.h>
#include <Network/Rating/RatingManager.hpp>
#include <Network/Rating/RatingStorage.hpp>
#include <Network/Rating/RatingNetwork.hpp>
#include <Network/Rating/RankManager.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>

/*
    Every address the rating system patches lives here, so the logic modules stay free
    of them.  Each hook below is a thunk: it adapts the game's calling convention and
    immediately hands over to a module.

    One deliberate exception: the three kmBranch in RatingUI.cpp.  Those do not adapt a
    call, they *replace* a game function outright, so the address and the body are the
    same statement -- splitting them would only add indirection.  Anything that is a
    genuine hook belongs here.
*/

namespace Pulsar {
namespace PointRating {
namespace Hooks {

// ---------------------------------------------------- end of race recomputation
// RacedataScenario::UpdatePoints. Patched at runtime because the branch has to be
// undone again for rooms where no rating is at stake.
kmRuntimeUse(0x8052e950);
static const u32 UPDATEPOINTS_ORIGINAL_PROLOGUE = 0x9421ff70;  // stwu r1, -0x90(r1)

static void UpdatePointsThunk(RacedataScenario* scenario) {
    Manager::UpdatePoints(scenario);
}

static void ApplyRatingPatch() {
    kmRuntimeBranchA(0x8052e950, UpdatePointsThunk);

    // Runs from a section load hook, where the controller may not exist yet: with no
    // controller there is no online room, so the vanilla routine must stay in place.
    const RKNet::Controller* ctrl = RKNet::Controller::sInstance;
    if (ctrl == nullptr) {
        kmRuntimeWrite32A(0x8052e950, UPDATEPOINTS_ORIGINAL_PROLOGUE);
        return;
    }

    const bool isFroom = ctrl->roomType == RKNet::ROOMTYPE_FROOM_HOST ||
                         ctrl->roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
    const bool unrankedFroom = isFroom && !Manager::IsRankedFroom();
    if (unrankedFroom || ctrl->roomType == RKNet::ROOMTYPE_NONE) {
        kmRuntimeWrite32A(0x8052e950, UPDATEPOINTS_ORIGINAL_PROLOGUE);
    }
}
static SectionLoadHook ratingHook(ApplyRatingPatch);

// ------------------------------------------------------------------ rank icon
/*
    Retro Rewind's technique, address found by B_squo, original idea by Zeraora.
    See the RATING_RANK_ICON block in RatingConfig.hpp for the whole chain; the short
    version is that these three `bl OnlineParams::CalcRank` become `li r3, idx`, and that
    index reaches every console through the SELECT packet's starRank field.

    0x806436a0 fills combos[0].rank, 0x806436e0 and 0x806436fc are the two branches that
    fill combos[1].rank for a second local player. Both local players share the licence,
    so all three get the same index.
*/
#if RATING_RANK_ICON
kmRuntimeUse(0x806436a0);
kmRuntimeUse(0x806436e0);
kmRuntimeUse(0x806436fc);

static void ApplyRankIcon() {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    RankId rank = 0;
    if (rksys != nullptr && rksys->curLicenseId >= 0 && rksys->curLicenseId < (int)Config::MAX_LICENSES) {
        rank = Rank::GetLocal(rksys->curLicenseId);
    }
    if (rank > Config::MAX_RANK) rank = Config::MAX_RANK;

    // The index is the rank itself: the pack maps BMG RANK_BMG_BASE + N to the badge glyph
    // BADGE_GLYPH_BASE + N, and rank 0 falls on the vanilla blank at index 0.
    const u32 instruction = 0x38600000 | ((u32)rank & 0xFFFF);  // li r3, rank

    // The addresses have to be the literals the kmRuntimeUse above declared: the macro
    // pastes them into identifiers, so a named constant will not compile.
    kmRuntimeWrite32A(0x806436a0, instruction);
    kmRuntimeWrite32A(0x806436e0, instruction);
    kmRuntimeWrite32A(0x806436fc, instruction);
}
static SectionLoadHook rankIconHook(ApplyRankIcon);
#endif

// --------------------------------------------------------------- licence mirror
// Injects the stored rating when a licence is read, and puts the vanilla values back
// before rksys.dat is written so the stock save is never polluted.
extern "C" int SaveManager_ReadLicenseHook() {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (mgr == nullptr) return 1;

    RKSYS::LicenseMgr* lic = nullptr;
    asm("mr %0, r31" : "=r"(lic));
    if (lic == nullptr) return 1;

    const u32 base = reinterpret_cast<u32>(&mgr->licenses[0]);
    const u32 addr = reinterpret_cast<u32>(lic);
    if (addr >= base) {
        const u32 idx = (addr - base) / sizeof(RKSYS::LicenseMgr);
        if (idx < Config::MAX_LICENSES) Storage::ApplyToLicense(idx, *lic);
    }
    return 1;
}
kmCall(0x805455a8, SaveManager_ReadLicenseHook);

extern "C" void SaveManager_WriteLicenseHook(RKSYS::Binary* raw, u32 idx) {
    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (mgr == nullptr || idx >= Config::MAX_LICENSES) return;

    Storage::StoreFromLicense(idx, mgr->licenses[idx]);

    u16 originalVr = 0;
    u16 originalBr = 0;
    if (raw != nullptr && Storage::GetOriginalLicenseRatings(idx, originalVr, originalBr)) {
        raw->core.licenses[idx].magic = 'RKPD';
        raw->core.licenses[idx].vr = originalVr;
        raw->core.licenses[idx].br = originalBr;
    }
}
kmCall(0x80546f9c, SaveManager_WriteLicenseHook);

// ------------------------------------------------------------------ QR2 keys
// The server browser publishes the rating under ev (0x65) and eb (0x66). Both are
// registered by PlayerCount.cpp; here we only serve their values.
kmRuntimeUse(0x8010f434);  // qr2_buffer_addA
kmRuntimeUse(0x800e4c88);  // original server key callback
kmRuntimeUse(0x8010f354);  // qr2_keybuffer_add
kmRuntimeUse(0x8010ecac);  // qr2_init_socketA

typedef void (*qr2_buffer_addA_t)(void*, const char*);
typedef void (*qr2_keybuffer_add_t)(void*, int);
typedef void (*ServerKeyCallback)(int, void*);
typedef void (*KeyListCallback)(int, void*);

static const qr2_buffer_addA_t qr2_buffer_addA = (qr2_buffer_addA_t)kmRuntimeAddr(0x8010f434);
static const qr2_keybuffer_add_t qr2_keybuffer_add = (qr2_keybuffer_add_t)kmRuntimeAddr(0x8010f354);
static const ServerKeyCallback OriginalServerKeyCallback = (ServerKeyCallback)kmRuntimeAddr(0x800e4c88);
static KeyListCallback OriginalKeyListCallback = nullptr;

static void RatingServerKeyCallback(int key, void* buffer) {
    if (key != Config::QR2_KEY_VR && key != Config::QR2_KEY_BR) {
        OriginalServerKeyCallback(key, buffer);
        return;
    }

    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const u32 licenseId = (rksys != nullptr) ? rksys->curLicenseId : 0;
    const RatingValue rating = (key == Config::QR2_KEY_VR) ? Storage::GetVR(licenseId)
                                                           : Storage::GetBR(licenseId);
    char text[32];
    snprintf(text, sizeof(text), "%d", Net::ClampForQr2(rating));
    qr2_buffer_addA(buffer, text);
}

// Private rooms drop ev/eb from the key list, so they are put back here.
static void RatingKeyListCallback(int keyType, void* buffer) {
    if (OriginalKeyListCallback != nullptr) OriginalKeyListCallback(keyType, buffer);
    if (keyType != 0) return;

    unsigned char* bytes = static_cast<unsigned char*>(buffer);
    const int count = *reinterpret_cast<int*>(bytes + 0x100);
    bool hasVr = false;
    bool hasBr = false;
    for (int i = 0; i < count; ++i) {
        if (bytes[i] == Config::QR2_KEY_VR) hasVr = true;
        if (bytes[i] == Config::QR2_KEY_BR) hasBr = true;
    }
    if (!hasVr) qr2_keybuffer_add(buffer, Config::QR2_KEY_VR);
    if (!hasBr) qr2_keybuffer_add(buffer, Config::QR2_KEY_BR);
}

typedef int (*qr2_init_socketA_t)(void*, int, int, const char*, const char*, int, int, void*, void*, void*,
                                  void*, void*, void*, void*);
static const qr2_init_socketA_t qr2_init_socketA_Real = (qr2_init_socketA_t)kmRuntimeAddr(0x8010ecac);

static int HookQr2Init(void* q, int s, int port, const char* game, const char* secret, int isPublic, int nat,
                       void* serverCb, void* playerCb, void* teamCb, void* keyListCb, void* countCb,
                       void* errorCb, void* userdata) {
    OriginalKeyListCallback = (KeyListCallback)keyListCb;
    return qr2_init_socketA_Real(q, s, port, game, secret, isPublic, nat, (void*)RatingServerKeyCallback,
                                 playerCb, teamCb, (void*)RatingKeyListCallback, countCb, errorCb, userdata);
}
kmCall(0x800d4f28, HookQr2Init);

// ------------------------------------------------------------------- DWC login
// Binds the licence to its online profile before logging in. A high custom rating must
// never block login: the vanilla u16 fields are only a compatibility mirror.
extern "C" void DWC_LoginAsync(wchar_t* miiName, int unk, void* callback, RKNet::Controller* self);

static void BindRatingProfileAndLogin(wchar_t* miiName, int unk, void* callback, RKNet::Controller* self) {
    Net::BindActiveLicenseProfile();
    DWC_LoginAsync(miiName, unk, callback, self);
}
kmCall(0x80658cdc, BindRatingProfileAndLogin);

}  // namespace Hooks
}  // namespace PointRating
}  // namespace Pulsar
