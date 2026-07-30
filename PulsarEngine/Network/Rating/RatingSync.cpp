#include <Network/Rating/RatingSync.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/RatingStorage.hpp>
#include <Network/GPReport.hpp>
#include <core/rvl/DWC/DWCGHTTP.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <include/c_stdio.h>
#include <include/c_stdlib.h>
#include <include/c_string.h>

namespace Pulsar {
namespace PointRating {

static bool sReportingSuppressed = false;

// Bumped on every new request; a reply carrying an old generation is dropped.
static u32 sGeneration = 0;
static float sRequestStartVr = 0.0f;
static float sRequestStartBr = 0.0f;
static char sRequestUrl[192];

struct RequestCtx {
    u32 generation;
    s32 profileId;
    u32 licenseId;
};
static RequestCtx sRequestCtx;

bool IsSyncEnabled() {
    return RATING_SYNC_ENABLED != 0;
}

void SetSyncReportingSuppressed(bool suppress) {
    sReportingSuppressed = suppress;
}

static int ScaleForSync(float rating) {
    int scaled = (int)(rating * 100.0f + 0.5f);
    if (scaled < Config::QR2_MIN) scaled = Config::QR2_MIN;
    if (scaled > Config::QR2_MAX) scaled = Config::QR2_MAX;
    return scaled;
}

void ReportCurrentRatings(u32 licenseId) {
    if (!IsSyncEnabled() || sReportingSuppressed) return;

    const int vrScaled = ScaleForSync(Storage::GetVR(licenseId));
    const int brScaled = ScaleForSync(Storage::GetBR(licenseId));

    char buffer[64];
    if (snprintf(buffer, sizeof(buffer), "vr=%d|br=%d", vrScaled, brScaled) < 0) return;
    Network::Report(Config::SYNC_REPORT_KEY, buffer);
}

static const char* SkipWhitespace(const char* p) {
    while (p != nullptr && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

static bool ParseJsonInt(const char* json, const char* key, int& out) {
    if (json == nullptr || key == nullptr) return false;

    const char* pos = strstr(json, key);
    if (pos == nullptr) return false;

    const char* colon = strchr(pos, ':');
    if (colon == nullptr) return false;

    char* end = nullptr;
    const long value = strtol(SkipWhitespace(colon + 1), &end, 10);
    if (end == nullptr || end == colon + 1) return false;

    out = (int)value;
    return true;
}

/*
    A reply is only applied if it still describes the situation that produced it: same
    generation, same profile still on the licence, and a rating that has not moved since
    the request went out. Otherwise a slow answer could silently undo a race result.
*/
static bool IsStillRelevant(const RequestCtx& ctx) {
    if (ctx.generation != sGeneration || ctx.profileId <= 0) return false;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr || ctx.licenseId >= Config::MAX_LICENSES) return false;
    if ((s32)rksys->licenses[ctx.licenseId].dwcAccUserData.gsProfileId != ctx.profileId) return false;

    return Storage::GetVR(ctx.licenseId) == sRequestStartVr &&
           Storage::GetBR(ctx.licenseId) == sRequestStartBr;
}

static void OnRatingsDownloaded(const void* body, int length, DWC::GHTTPResult result, void* param) {
    if (result != DWC::DWCGHTTPSuccess || body == nullptr || length <= 0) return;

    RequestCtx* ctx = reinterpret_cast<RequestCtx*>(param);
    if (ctx == nullptr || ctx->generation != sGeneration) return;

    const int maxLen = (length < (int)Config::SYNC_RESPONSE_MAX - 1) ? length
                                                                    : (int)Config::SYNC_RESPONSE_MAX - 1;
    char json[Config::SYNC_RESPONSE_MAX];
    memcpy(json, body, maxLen);
    json[maxLen] = '\0';

    int found = 0;
    if (!ParseJsonInt(json, "\"found\"", found) || found != 1) return;
    if (!IsStillRelevant(*ctx)) return;

    int vrScaled = 0;
    int brScaled = 0;
    if (!ParseJsonInt(json, "\"vr\"", vrScaled)) return;
    if (!ParseJsonInt(json, "\"br\"", brScaled)) return;

    // Writing the downloaded values must not echo back out as a fresh upload.
    SetSyncReportingSuppressed(true);
    Storage::BeginBatch();
    Storage::SaveProfileVR(ctx->profileId, (float)vrScaled / 100.0f);
    Storage::SaveProfileBR(ctx->profileId, (float)brScaled / 100.0f);
    Storage::EndBatch();
    SetSyncReportingSuppressed(false);
}

void StartLoginRatingDownload(s32 profileId, u32 licenseId) {
    if (profileId <= 0 || licenseId >= Config::MAX_LICENSES) return;

    // The binding is useful even with the sync off: it is what ties the licence to the
    // right entry of VKRating.pul.
    Storage::BindLicenseProfileId(licenseId, profileId);
    if (!IsSyncEnabled()) return;

    RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if (rksys == nullptr) return;

    ++sGeneration;
    sRequestStartVr = Storage::GetVR(licenseId);
    sRequestStartBr = Storage::GetBR(licenseId);

    sRequestCtx.generation = sGeneration;
    sRequestCtx.profileId = profileId;
    sRequestCtx.licenseId = licenseId;

    if (snprintf(sRequestUrl, sizeof(sRequestUrl), Config::SYNC_DOWNLOAD_URL_FORMAT, (long)profileId) < 0) {
        return;
    }

    const int request = DWC::GetGHTTPDataEx(sRequestUrl, Config::SYNC_RESPONSE_MAX, true, nullptr,
                                            OnRatingsDownloaded, &sRequestCtx);
    // A refused request must not leave a stale generation that a later reply could match.
    if (request < 0) ++sGeneration;
}

}  // namespace PointRating
}  // namespace Pulsar
