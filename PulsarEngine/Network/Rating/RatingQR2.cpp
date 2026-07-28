#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <include/c_stdio.h>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Network/Rating/PlayerRating.hpp>

namespace Pulsar {
namespace PointRating {

kmRuntimeUse(0x8010f434);
kmRuntimeUse(0x800e4c88);
kmRuntimeUse(0x8010f354);
kmRuntimeUse(0x8010ecac);

typedef void (*qr2_buffer_addA_t)(void*, const char*);
typedef void (*qr2_keybuffer_add_t)(void*, int);
typedef void (*ServerKeyCallback)(int, void*);
typedef void (*KeyListCallback)(int, void*);
static const qr2_buffer_addA_t qr2_buffer_addA = (qr2_buffer_addA_t)kmRuntimeAddr(0x8010f434);
static const qr2_keybuffer_add_t qr2_keybuffer_add = (qr2_keybuffer_add_t)kmRuntimeAddr(0x8010f354);
static const ServerKeyCallback OriginalServerKeyCallback = (ServerKeyCallback)kmRuntimeAddr(0x800e4c88);
static KeyListCallback OriginalKeyListCallback = nullptr;

static int ClampRatingForQr2(float rating) {
    int scaled = (int)(rating * 100.0f + 0.5f);
    if (scaled < 1) return 1;
    return scaled > 500000 ? 500000 : scaled;
}

static void RatingServerKeyCallback(int key, void* buffer) {
    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const u32 licenseId = rksys != nullptr ? rksys->curLicenseId : 0;
    if (key == 0x65 || key == 0x66) {
        const float rating = key == 0x65 ? GetUserVR(licenseId) : GetUserBR(licenseId);
        char text[32];
        snprintf(text, sizeof(text), "%d", ClampRatingForQr2(rating));
        qr2_buffer_addA(buffer, text);
        return;
    }
    OriginalServerKeyCallback(key, buffer);
}

static void RatingKeyListCallback(int keyType, void* buffer) {
    if (OriginalKeyListCallback != nullptr) OriginalKeyListCallback(keyType, buffer);
    if (keyType == 0) {
        unsigned char* bytes = static_cast<unsigned char*>(buffer);
        const int count = *reinterpret_cast<int*>(bytes + 0x100);
        bool hasVr = false, hasBr = false;
        for (int i = 0; i < count; ++i) {
            if (bytes[i] == 0x65) hasVr = true;
            if (bytes[i] == 0x66) hasBr = true;
        }
        if (!hasVr) qr2_keybuffer_add(buffer, 0x65);
        if (!hasBr) qr2_keybuffer_add(buffer, 0x66);
    }
}

typedef int (*qr2_init_socketA_t)(void*, int, int, const char*, const char*, int, int, void*, void*, void*, void*, void*, void*, void*);
static const qr2_init_socketA_t qr2_init_socketA_Real = (qr2_init_socketA_t)kmRuntimeAddr(0x8010ecac);

static int HookQr2Init(void* q, int s, int port, const char* game, const char* secret, int isPublic, int nat,
                       void* serverCb, void* playerCb, void* teamCb, void* keyListCb, void* countCb, void* errorCb, void* userdata) {
    OriginalKeyListCallback = (KeyListCallback)keyListCb;
    return qr2_init_socketA_Real(q, s, port, game, secret, isPublic, nat,
        (void*)RatingServerKeyCallback, playerCb, teamCb, (void*)RatingKeyListCallback, countCb, errorCb, userdata);
}
kmCall(0x800d4f28, HookQr2Init);

} // namespace PointRating
} // namespace Pulsar

