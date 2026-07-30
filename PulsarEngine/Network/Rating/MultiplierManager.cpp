#include <Network/Rating/MultiplierManager.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <core/rvl/DWC/DWCGHTTP.hpp>
#include <include/c_string.h>

namespace Pulsar {
namespace PointRating {
namespace Multiplier {

static bool sRequested = false;
static bool sValid = false;
static float sRemote = Config::MULTIPLIER_DEFAULT;

static const char* SkipSpace(const char* p) {
    while (p != nullptr && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) ++p;
    return p;
}

// Hand-rolled because the toolchain has no strtof; the file is a bare decimal.
static bool ParseMultiplier(const void* body, int length, float& value) {
    if (body == nullptr || length <= 0) return false;

    const int count = (length < 31) ? length : 31;
    char text[32];
    memcpy(text, body, count);
    text[count] = '\0';

    const char* p = SkipSpace(text);
    bool hasDigit = false;
    float result = 0.0f;
    while (*p >= '0' && *p <= '9') {
        hasDigit = true;
        result = result * 10.0f + (float)(*p - '0');
        ++p;
    }
    if (*p == '.') {
        ++p;
        float scale = 0.1f;
        while (*p >= '0' && *p <= '9') {
            hasDigit = true;
            result += (float)(*p - '0') * scale;
            scale *= 0.1f;
            ++p;
        }
    }
    p = SkipSpace(p);

    if (!hasDigit || *p != '\0') return false;
    if (result < Config::MULTIPLIER_MIN || result > Config::MULTIPLIER_MAX) return false;

    value = result;
    return true;
}

static void OnDownloaded(const void* body, int length, DWC::GHTTPResult result, void*) {
    if (result != DWC::DWCGHTTPSuccess) return;
    float value = Config::MULTIPLIER_DEFAULT;
    if (ParseMultiplier(body, length, value)) {
        sRemote = value;
        sValid = true;
    }
}

void TryDownload() {
    if (sRequested) return;
    const int request = DWC::GetGHTTPDataEx(Config::MULTIPLIER_URL, 32, true, nullptr, OnDownloaded, nullptr);
    if (request >= 0) sRequested = true;
}

float GetRemoteLayer() {
    return sValid ? sRemote : Config::MULTIPLIER_DEFAULT;
}

// Seasonal events are not wired up in VanzaKart yet; adding them means returning
// something other than 1.0 here, and nothing else changes.
float GetEventLayer() {
    return 1.0f;
}

float GetWeekendLayer(u8 /*region*/) {
    return 1.0f;
}

bool IsWeekendActive() {
    return false;
}

bool IsWeekendActiveForRegion(u8 /*region*/) {
    return false;
}

bool IsItemRainEventActive() {
    return false;
}

float Get() {
    return GetEventLayer() * GetWeekendLayer(0) * GetRemoteLayer();
}

}  // namespace Multiplier

// ------------------------------------------------------------- public facade
void TryDownloadMultiplier() {
    Multiplier::TryDownload();
}
float GetMultiplier() {
    return Multiplier::Get();
}
bool IsWeekendMultiplierActive() {
    return Multiplier::IsWeekendActive();
}
bool IsWeekendMultiplierActiveForRegion(u8 region) {
    return Multiplier::IsWeekendActiveForRegion(region);
}
bool IsItemRainEventActive() {
    return Multiplier::IsItemRainEventActive();
}

}  // namespace PointRating
}  // namespace Pulsar
