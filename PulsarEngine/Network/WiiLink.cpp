/*
    Based on wfc-patcher-wii
    Written by mkwcat

    Copyright (c) 2023-2025 WiiLink
    SPDX-License-Identifier: gpl-2.0-or-later
*/

#include <Network/RSA.hpp>
#include <Network/SHA256.hpp>
#include <Network/WiiLink.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/rvl/DWC/DWC.hpp>
#include <core/rvl/DWC/NHTTP.hpp>
#include <core/rvl/ipc/ipc.hpp>
#include <kamek.hpp>

#ifndef _WIILINK_
#define _WIILINK_

static void* s_payloadStorage = nullptr;
static void *s_payload = nullptr;
static bool s_payloadReady = false;
static u8 s_saltHash[SHA256_DIGEST_SIZE];

extern "C" void OSReport(const char *format, ...);
struct WwfcServer {
    const char *domain;
    const unsigned char *publicKey;
};

static const WwfcServer s_servers[] = {
    {WWFC_DOMAIN, PayloadPublicKey},
#ifdef WWFC_FALLBACK_DOMAIN
    {WWFC_FALLBACK_DOMAIN, PayloadFallbackPublicKey},
#endif
};
static const u32 s_serverCount = sizeof(s_servers) / sizeof(s_servers[0]);

static u32 s_serverIndex = 0;
static u32 s_attemptsOnServer = 0;
static u32 s_totalAttempts = 0;

static const char *s_activeServer = nullptr;

static const u32 ATTEMPTS_PER_SERVER = 2;

static const u32 MAX_TOTAL_ATTEMPTS = 6;

#ifdef WWFC_FALLBACK_FLAG_URL

enum FallbackPermission {
    FALLBACK_UNKNOWN,
    FALLBACK_ALLOWED,
    FALLBACK_DENIED,
};
static FallbackPermission s_fallbackPermission = FALLBACK_UNKNOWN;

enum RequestKind {
    REQUEST_PAYLOAD,
    REQUEST_FALLBACK_FLAG,
};
static RequestKind s_requestKind = REQUEST_PAYLOAD;

static const u32 FLAG_BUFFER_SIZE = 0x20;
static u8 s_flagBuffer[FLAG_BUFFER_SIZE] __attribute__((aligned(0x20)));

#endif

extern "C" {
void Real_DWCi_Auth_SendRequest(
    int param_1, int param_2, int param_3, int param_4, int param_5, int param_6);

extern s32 *s_auth_work;
extern s32 s_auth_error;
}

static asm void DWCi_Auth_SendRequest(
    int param_1, int param_2, int param_3, int param_4, int param_5, int param_6) {
    // clang-format off
    nofralloc

    stwu r1, -0x1B0(r1)
    b Real_DWCi_Auth_SendRequest
    // clang-format on
}

static bool EnsurePayloadBuffer() {
    if (s_payload != nullptr) return true;

    if (s_payloadStorage == nullptr) {
        EGG::Heap* heap = RKSystem::mInstance.EGGSystem;
        if (heap == nullptr) return false;

        // Keep the large payload buffer out of Kamek's boot-time BSS reservation.
        s_payloadStorage = EGG::Heap::alloc(PAYLOAD_BLOCK_SIZE + 0x20, 0x20, heap);
        if (s_payloadStorage == nullptr) return false;
    }

    s_payload = reinterpret_cast<void*>((u32(s_payloadStorage) + 31) & ~31);
    return true;
}

bool GenerateRandomSalt(u8 *out) {
    // Generate cryptographic random with ES_Sign
    s32 fd = IOS::Open("/dev/es", IOS::MODE_NONE);
    if (fd < 0) {
        return false;
    }

    u8 dummy[0x20] __attribute((aligned(0x40)));
    dummy[0] = 0x7a;
    u8 eccCert[0x180] __attribute((aligned(0x40)));
    u8 eccSignature[0x3C] __attribute((aligned(0x40)));

    IOS::IOCtlvRequest vec[3] __attribute((aligned(0x40)));
    vec[0].address = &dummy;
    vec[0].size = 1;
    vec[1].address = eccSignature;
    vec[1].size = 0x3C;
    vec[2].address = eccCert;
    vec[2].size = 0x180;

    // ES_Sign
    s32 ret = IOS::IOCtlv(fd, IOS::IOCtlType(0x30), 1, 2, vec);
    IOS::Close(fd);

    if (ret < 0) {
        return false;
    }

    SHA256Context ctx;
    SHA256Init(&ctx);
    SHA256Update(&ctx, eccSignature, 0x3C);
    SHA256Update(&ctx, eccCert, 0x180);
    memcpy(out, SHA256Final(&ctx), SHA256_DIGEST_SIZE);
    return true;
}

s32 HandleResponse(u8 *block) {
    register wwfc_payload *__restrict payload =
        reinterpret_cast<wwfc_payload *>(block);

    if (*reinterpret_cast<u32 *>(payload) != 0x57574643 /* WWFC */) {
        return WL_ERROR_PAYLOAD_STAGE1_HEADER_CHECK;
    }

    if (payload->header.total_size < sizeof(wwfc_payload) ||
        payload->header.total_size > PAYLOAD_BLOCK_SIZE) {
        return WL_ERROR_PAYLOAD_STAGE1_LENGTH_ERROR;
    }

    if (memcmp(payload->salt, s_saltHash, SHA256_DIGEST_SIZE) != 0) {
        return WL_ERROR_PAYLOAD_STAGE1_SALT_MISMATCH;
    }

    SHA256Context ctx;
    SHA256Init(&ctx);
    SHA256Update(
        &ctx, reinterpret_cast<u8 *>(payload) + sizeof(wwfc_payload_header),
        payload->header.total_size - sizeof(wwfc_payload_header));
    u8 *hash = SHA256Final(&ctx);

    if (!RSAVerify(
            reinterpret_cast<const RSAPublicKey *>(s_servers[s_serverIndex].publicKey),
            payload->header.signature, hash)) {
        return WL_ERROR_PAYLOAD_STAGE1_SIGNATURE_INVALID;
    }

    // Flush data cache and invalidate instruction cache
    for (register u32 i = 0; i < 0x20000; i += 0x20) {
        asm(dcbf i, payload; sync; icbi i, payload; isync;);
    }
    s32 (*entryFunction)(wwfc_payload *) =
        reinterpret_cast<s32 (*)(wwfc_payload *)>(
            reinterpret_cast<u8 *>(payload) + payload->info.entry_point);

    return entryFunction(payload);
}

static void ResetServerRotation() {
    s_serverIndex = 0;
    s_attemptsOnServer = 0;
    s_totalAttempts = 0;
#ifdef WWFC_FALLBACK_FLAG_URL
    s_fallbackPermission = FALLBACK_UNKNOWN;
    s_requestKind = REQUEST_PAYLOAD;
#endif
}

static void FailCurrentServer(s32 error) {
    OSReport("[VK WFC] %s failed, error=%ld (attempt %lu on this server, %lu overall)\n",
             s_servers[s_serverIndex].domain, (long)error,
             (unsigned long)(s_attemptsOnServer + 1), (unsigned long)(s_totalAttempts + 1));

    if (++s_totalAttempts >= MAX_TOTAL_ATTEMPTS) {
        OSReport("[VK WFC] attempt budget spent, giving up with error=%ld\n", (long)error);
        ResetServerRotation();
        s_auth_error = error;
        return;
    }

    if (++s_attemptsOnServer < ATTEMPTS_PER_SERVER) {
        OSReport("[VK WFC] retrying %s\n", s_servers[s_serverIndex].domain);
        s_auth_error = -1;  // same server, one more go
        return;
    }

    s_attemptsOnServer = 0;
    if (s_serverIndex + 1 < s_serverCount) {
#ifdef WWFC_FALLBACK_FLAG_URL
        if (s_fallbackPermission == FALLBACK_UNKNOWN) {
            OSReport("[VK WFC] primary exhausted, asking the remote switch\n");
            s_requestKind = REQUEST_FALLBACK_FLAG;
            s_auth_error = -1;
            return;
        }

        if (s_fallbackPermission != FALLBACK_ALLOWED) {
            OSReport("[VK WFC] fallback is switched off remotely, not rotating\n");
            ResetServerRotation();
            s_auth_error = error;
            return;
        }
#endif
        ++s_serverIndex;
        OSReport("[VK WFC] *** FALLING BACK to %s ***\n", s_servers[s_serverIndex].domain);
        s_auth_error = -1;  // ask the next one
        return;
    }
    OSReport("[VK WFC] no server left to try, giving up with error=%ld\n", (long)error);
    ResetServerRotation();
    s_auth_error = error;
}

#ifdef WWFC_FALLBACK_FLAG_URL
static void OnFallbackFlagReceived(s32 result, void *response, void *userdata) {
    s_requestKind = REQUEST_PAYLOAD;

    bool allowed = false;
    if (response != nullptr) {
        if (result == 0) {
            // First character that is not whitespace decides; only '1' is a yes.
            for (u32 i = 0; i < FLAG_BUFFER_SIZE; ++i) {
                const u8 c = s_flagBuffer[i];
                if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
                allowed = c == '1';
                break;
            }
        }
        NHTTPDestroyResponse(response);
    }

    s_fallbackPermission = allowed ? FALLBACK_ALLOWED : FALLBACK_DENIED;
    OSReport("[VK WFC] remote switch: fallback %s\n", allowed ? "ENABLED" : "disabled");

    if (!allowed) {
        ResetServerRotation();
        s_auth_error = WL_ERROR_PAYLOAD_STAGE1_RESPONSE;
        return;
    }

    ++s_serverIndex;
    OSReport("[VK WFC] *** FALLING BACK to %s ***\n", s_servers[s_serverIndex].domain);
    s_auth_error = -1;
}

#endif

void OnPayloadReceived(s32 result, void *response, void *userdata) {
    if (response == nullptr) {
        FailCurrentServer(WL_ERROR_PAYLOAD_STAGE1_RESPONSE);
        return;
    }

    NHTTPDestroyResponse(response);

    if (result != 0) {
        FailCurrentServer(WL_ERROR_PAYLOAD_STAGE1_RESPONSE);
        return;
    }

    s32 error = HandleResponse(reinterpret_cast<u8 *>(s_payload));
    if (error != 0) {
        FailCurrentServer(error);
        return;
    }

    // Which server actually served the session is worth stating plainly: from here on
    // every URL in the game points at it, and on the fallback that means a different
    // player base and a different set of ratings.
    s_activeServer = s_servers[s_serverIndex].domain;
    OSReport("[VK WFC] payload accepted from %s%s\n", s_activeServer,
             s_serverIndex == 0 ? "" : "  <-- FALLBACK, not the primary server");

    s_payloadReady = true;
    s_auth_error = -1;  // This error code will retry auth
}

const char *GetActiveServer() {
    return s_activeServer;
}

bool IsOnFallbackServer() {
    return s_activeServer != nullptr && s_activeServer != s_servers[0].domain;
}

void WiiLinkInit() {
    // Prototipo per aggancio iniziale opzionale
}

kmBranchDefCpp(
    0x800ed6e8, 0, void, int param_1, int param_2, int param_3, int param_4,
    int param_5, int param_6) {
    if (s_payloadReady) {
        DWCi_Auth_SendRequest(
            param_1, param_2, param_3, param_4, param_5, param_6);
        return;
    }

#ifdef WWFC_FALLBACK_FLAG_URL
    // The switch borrows the auth request slot for one round trip. No payload buffer and
    // no salt are needed for it, so it returns before any of that work happens.
    if (s_requestKind == REQUEST_FALLBACK_FLAG) {
        memset(s_flagBuffer, 0, FLAG_BUFFER_SIZE);

        void *flagRequest = NHTTPCreateRequest(
            WWFC_FALLBACK_FLAG_URL, 0, s_flagBuffer, FLAG_BUFFER_SIZE,
            OnFallbackFlagReceived, 0);

        if (flagRequest == nullptr) {
            // Cannot even ask, so the answer stays no rather than defaulting open.
            s_requestKind = REQUEST_PAYLOAD;
            s_fallbackPermission = FALLBACK_DENIED;
            OSReport("[VK WFC] could not request the remote switch, fallback stays off\n");
            ResetServerRotation();
            s_auth_error = WL_ERROR_PAYLOAD_STAGE1_MAKE_REQUEST;
            return;
        }

        s_auth_work[0x59E0 / 4] = NHTTPSendRequestAsync(flagRequest);
        return;
    }
#endif

    if (!EnsurePayloadBuffer()) {
        s_auth_error = WL_ERROR_PAYLOAD_STAGE1_ALLOC;
        return;
    }

    memset(s_payload, 0, PAYLOAD_BLOCK_SIZE);

    u8 salt[SHA256_DIGEST_SIZE];
    if (!GenerateRandomSalt(salt)) {
        s_auth_error = WL_ERROR_PAYLOAD_STAGE1_MAKE_REQUEST;
    }

    static const char *hexConv = "0123456789abcdef";
    char saltHex[SHA256_DIGEST_SIZE * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        saltHex[i * 2] = hexConv[salt[i] >> 4];
        saltHex[i * 2 + 1] = hexConv[salt[i] & 0xf];
    }
    saltHex[SHA256_DIGEST_SIZE * 2] = 0;


    // "Anticheat"
    // Check for the presence of the gecko codehandler, and halt online connections if it is found
    if (*(u32 *)0x80001920 != 0x0) {
        if (*(u32 *)0x80001920 != 0xFEE00090 && *(u32 *)0x80001920 != 0x3C841000) {
            s_auth_error = WL_ERROR_PAYLOAD_STAGE1_WAITING;
            return;
        }
        if (*(u32 *)0x802588F8 != 0x0 || *(u32 *)0x8000629C != 0x4E800020 || *(u32 *)0x80259198 == 0x9421FF98) {
            s_auth_error = WL_ERROR_PAYLOAD_STAGE1_WAITING;
            return;
        }
    }


    char uri[0x100];
    sprintf(uri, "payload?g=RMC%cD00&s=%s", *(char *)0x80000003, saltHex);

    // Generate salt hash
    SHA256Context ctx;
    SHA256Init(&ctx);
    SHA256Update(&ctx, uri, strlen(uri));
    memcpy(s_saltHash, SHA256Final(&ctx), SHA256_DIGEST_SIZE);

    char url[0x100];
    sprintf(
        url, "http://nas.%s/%s&h=%02x%02x%02x%02x",
        s_servers[s_serverIndex].domain, uri,
        s_saltHash[0], s_saltHash[1], s_saltHash[2], s_saltHash[3]);

    OSReport("[VK WFC] requesting payload from nas.%s (server %lu of %lu)\n",
             s_servers[s_serverIndex].domain, (unsigned long)(s_serverIndex + 1),
             (unsigned long)s_serverCount);

    void *request = NHTTPCreateRequest(
        url, 0, s_payload, PAYLOAD_BLOCK_SIZE, OnPayloadReceived, 0);

    if (request == nullptr) {
        s_auth_error = WL_ERROR_PAYLOAD_STAGE1_MAKE_REQUEST;
        return;
    }

    s_auth_work[0x59E0 / 4] = NHTTPSendRequestAsync(request);
}

#endif