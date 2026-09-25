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

static u32 s_attemptsOnServer = 0;
static u32 s_totalAttempts = 0;

static const char *s_activeServer = nullptr;

/*
    True between handing a request to DWC and its callback firing.

    DWC polls one request handle, kept in s_auth_work, and it can call this hook again
    before the request in that slot has completed. Issuing a second one then overwrites
    the handle and both die: the Dolphin log showed it plainly - two "requesting payload"
    lines 60ms apart, then two -20912 a millisecond apart, far too fast to be the network.
*/
static bool s_requestInFlight = false;

static const u32 ATTEMPTS_PER_SERVER = 2;

static const u32 MAX_TOTAL_ATTEMPTS = 6;

/*
    The order the servers get tried in, as indices into s_servers. Kept separate from the
    table itself because the remote switch can put either one in front: the table is which
    servers exist, this is which one to ask first.
*/
static u32 s_order[sizeof(s_servers) / sizeof(s_servers[0])];
static u32 s_orderCount = 1;
static u32 s_orderPos = 0;

static const WwfcServer &CurrentServer() {
    return s_servers[s_order[s_orderPos]];
}

#ifdef WWFC_FALLBACK_FLAG_URL

/*
    What the remote switch last said. The file holds one character:

        '0'   primary only, no fallback at all          (what vanilla does)
        '1'   primary first, fallback behind it
        '2'   fallback first, primary behind it         (primary is known to be down)

    Anything else, and any failure to read it, counts as '0'.
*/
enum ServerPolicy {
    POLICY_UNKNOWN,
    POLICY_PRIMARY_ONLY,
    POLICY_PRIMARY_FIRST,
    POLICY_FALLBACK_FIRST,
};
static ServerPolicy s_policy = POLICY_UNKNOWN;

/*
    Choosing the order means asking before the first payload request rather than after a
    failure, so a healthy connection now pays one small round trip it did not pay before.
    That is cheap while the switch answers, and it is served by the machine that is up in
    exactly the situation this exists for.

    When it does not answer the cost is a timeout instead, and that would be paid on every
    connection. So a failed read is remembered for the whole boot and never retried: at
    worst one stall per game launch, not per connection attempt.
*/
static bool s_switchUnreachable = false;

static const u32 FLAG_BUFFER_SIZE = 0x20;
static u8 s_flagBuffer[FLAG_BUFFER_SIZE] __attribute__((aligned(0x20)));

#endif

// Rebuilds s_order from the policy. Without a fallback compiled in, or before the switch
// has answered, this is just the primary on its own.
static void ApplyPolicy() {
    s_orderPos = 0;
    s_order[0] = 0;
    s_orderCount = 1;

#ifdef WWFC_FALLBACK_FLAG_URL
    if (s_serverCount < 2) return;

    if (s_policy == POLICY_FALLBACK_FIRST) {
        s_order[0] = 1;
        s_order[1] = 0;
        s_orderCount = 2;
    } else if (s_policy == POLICY_PRIMARY_FIRST) {
        s_order[1] = 1;
        s_orderCount = 2;
    }
#endif
}

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
            reinterpret_cast<const RSAPublicKey *>(CurrentServer().publicKey),
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

/*
    End of a failed connection attempt, never called on success.

    The switch answer is dropped here so the next attempt asks again: it gets flipped
    while an outage is already under way, and a console that asked a minute too early
    has to be able to pick up the new order on the player's next try. The one thing kept
    is s_switchUnreachable, because re-asking a switch that timed out would put that
    stall in front of every single connection.
*/
static void ResetServerRotation() {
    s_attemptsOnServer = 0;
    s_totalAttempts = 0;
    // Belt and braces: the callbacks clear this themselves, but a request that somehow
    // never calls back would otherwise wedge every later attempt behind the guard.
    s_requestInFlight = false;
#ifdef WWFC_FALLBACK_FLAG_URL
    s_policy = POLICY_UNKNOWN;
#endif
    ApplyPolicy();
}

static void FailCurrentServer(s32 error) {
    OSReport("[VK WFC] %s failed, error=%ld (attempt %lu on this server, %lu overall)\n",
             CurrentServer().domain, (long)error,
             (unsigned long)(s_attemptsOnServer + 1), (unsigned long)(s_totalAttempts + 1));

    if (++s_totalAttempts >= MAX_TOTAL_ATTEMPTS) {
        OSReport("[VK WFC] attempt budget spent, giving up with error=%ld\n", (long)error);
        ResetServerRotation();
        s_auth_error = error;
        return;
    }

    if (++s_attemptsOnServer < ATTEMPTS_PER_SERVER) {
        OSReport("[VK WFC] retrying %s\n", CurrentServer().domain);
        s_auth_error = -1;  // same server, one more go
        return;
    }

    // No permission check left to make here: the order was already settled before the
    // first request went out, and a policy that forbids the other server simply leaves
    // it out of s_order.
    s_attemptsOnServer = 0;
    if (s_orderPos + 1 < s_orderCount) {
        ++s_orderPos;
        OSReport("[VK WFC] *** switching to %s ***\n", CurrentServer().domain);
        s_auth_error = -1;  // ask the next one
        return;
    }
    OSReport("[VK WFC] no server left to try, giving up with error=%ld\n", (long)error);
    ResetServerRotation();
    s_auth_error = error;
}

/*
    Defined further down, next to the auth hook that is its other caller. Declared out
    here rather than beside the switch code, because the hook calls it in every build,
    including the ones compiled without a fallback.

    The switch callback sends the payload request itself rather than returning -1 and
    letting the auth retry do it. That retry is what killed the previous attempt: DWC
    starts a fresh auth pass by cancelling whatever sits in the request slot, so the
    payload request placed there came back with NHTTP_ERROR_CANCELED (8) about 26ms
    later. Handing the slot straight from one request to the next never gives DWC that
    window.
*/
static void SendPayloadRequest();

#ifdef WWFC_FALLBACK_FLAG_URL
static const char *PolicyName(ServerPolicy p) {
    if (p == POLICY_FALLBACK_FIRST) return "fallback first";
    if (p == POLICY_PRIMARY_FIRST) return "primary first, fallback behind";
    return "primary only";
}

static void OnSwitchReceived(s32 result, void *response, void *userdata) {
    s_requestInFlight = false;

    char answer = 0;
    if (response != nullptr) {
        if (result == 0) {
            // First character that is not whitespace decides.
            for (u32 i = 0; i < FLAG_BUFFER_SIZE; ++i) {
                const u8 c = s_flagBuffer[i];
                if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
                answer = (char)c;
                break;
            }
        }
        NHTTPDestroyResponse(response);
    }

    if (answer == 0) {
        // Nothing readable came back. Behave as if the switch were off, and do not ask
        // again this boot so the timeout is paid once rather than on every connection.
        s_switchUnreachable = true;
        s_policy = POLICY_PRIMARY_ONLY;
        OSReport("[VK WFC] remote switch unreachable, primary only until reboot\n");
    } else {
        s_policy = answer == '2' ? POLICY_FALLBACK_FIRST
                 : answer == '1' ? POLICY_PRIMARY_FIRST
                                 : POLICY_PRIMARY_ONLY;
        OSReport("[VK WFC] remote switch '%c': %s\n", answer, PolicyName(s_policy));
    }

    ApplyPolicy();
    OSReport("[VK WFC] order settled, first up is %s\n", CurrentServer().domain);

    // Straight on to the payload, without handing control back to the auth retry.
    SendPayloadRequest();
}

#endif

void OnPayloadReceived(s32 result, void *response, void *userdata) {
    s_requestInFlight = false;

    /*
        On the failure paths the raw NHTTP verdict is worth printing, because our own
        -20912 lumps together causes that are nothing alike. Codes are NHTTP::Error:
        4 DNS, 5 CONNECT, 8 CANCELED - that last one is what a request being pulled out
        from under us looks like, and it cost an evening to recognise.
    */
    if (response == nullptr) {
        OSReport("[VK WFC] no response object (NHTTP result=%ld)\n", (long)result);
        FailCurrentServer(WL_ERROR_PAYLOAD_STAGE1_RESPONSE);
        return;
    }

    NHTTPDestroyResponse(response);

    if (result != 0) {
        OSReport("[VK WFC] NHTTP refused the payload, result=%ld\n", (long)result);
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
    s_activeServer = CurrentServer().domain;
    OSReport("[VK WFC] payload accepted from %s%s\n", s_activeServer,
             s_order[s_orderPos] == 0 ? "" : "  <-- FALLBACK, not the primary server");

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

    // Something is already in the slot DWC is polling. Leave it alone and let its
    // callback drive what happens next.
    if (s_requestInFlight) return;

#ifdef WWFC_FALLBACK_FLAG_URL
    /*
        The switch is asked before anything else, because it decides which server the
        payload request below is even aimed at. It borrows the auth request slot for one
        round trip - DWC polls a single handle, so the two cannot be in flight together -
        and needs neither the payload buffer nor a salt, so it returns before any of that
        work happens.
    */
    if (s_policy == POLICY_UNKNOWN && !s_switchUnreachable) {
        memset(s_flagBuffer, 0, FLAG_BUFFER_SIZE);

        void *switchRequest = NHTTPCreateRequest(
            WWFC_FALLBACK_FLAG_URL, 0, s_flagBuffer, FLAG_BUFFER_SIZE,
            OnSwitchReceived, 0);

        if (switchRequest == nullptr) {
            // Cannot even form the request, which is a local failure rather than the
            // switch being down. Carry on against the primary instead of stalling.
            s_switchUnreachable = true;
            s_policy = POLICY_PRIMARY_ONLY;
            ApplyPolicy();
            OSReport("[VK WFC] could not request the remote switch, primary only\n");
        } else {
            s_requestInFlight = true;
            s_auth_work[0x59E0 / 4] = NHTTPSendRequestAsync(switchRequest);
            return;
        }
    }
#endif

    SendPayloadRequest();
}

static void SendPayloadRequest() {
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
        CurrentServer().domain, uri,
        s_saltHash[0], s_saltHash[1], s_saltHash[2], s_saltHash[3]);

    OSReport("[VK WFC] requesting payload from nas.%s (%lu of %lu in order)\n",
             CurrentServer().domain, (unsigned long)(s_orderPos + 1),
             (unsigned long)s_orderCount);

    void *request = NHTTPCreateRequest(
        url, 0, s_payload, PAYLOAD_BLOCK_SIZE, OnPayloadReceived, 0);

    if (request == nullptr) {
        s_auth_error = WL_ERROR_PAYLOAD_STAGE1_MAKE_REQUEST;
        return;
    }

    s_requestInFlight = true;
    s_auth_work[0x59E0 / 4] = NHTTPSendRequestAsync(request);
}

#endif