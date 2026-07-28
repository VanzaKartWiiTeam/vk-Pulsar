#include <Network/Rating/PlayerRating.hpp>
#include <core/rvl/DWC/DWCGHTTP.hpp>
#include <include/c_string.h>

namespace Pulsar {
namespace PointRating {

static const char* MULTIPLIER_URL = "http://sitodaking.it:8000/VanzaKart/multiplierBeta.txt";
static bool sRequested = false;
static bool sValid = false;
static float sMultiplier = 1.0f;

static const char* SkipSpace(const char* p) {
    while (p != nullptr && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) ++p;
    return p;
}

static bool ParseMultiplier(const void* body, int length, float& value) {
    if (body == nullptr || length <= 0) return false;
    const int count = length < 31 ? length : 31;
    char text[32]; memcpy(text, body, count); text[count] = '\0';
    const char* p = SkipSpace(text);
    bool digit = false; float result = 0.0f;
    while (*p >= '0' && *p <= '9') { digit = true; result = result * 10.0f + (*p++ - '0'); }
    if (*p == '.') {
        ++p; float scale = 0.1f;
        while (*p >= '0' && *p <= '9') { digit = true; result += (*p++ - '0') * scale; scale *= 0.1f; }
    }
    p = SkipSpace(p);
    if (!digit || *p != '\0' || result < 0.0f || result > 100.0f) return false;
    value = result; return true;
}

static void OnMultiplierDownloaded(const void* body, int length, DWC::GHTTPResult result, void*) {
    if (result != DWC::DWCGHTTPSuccess) return;
    float value = 1.0f;
    if (ParseMultiplier(body, length, value)) { sMultiplier = value; sValid = true; }
}

void TryDownloadMultiplier() {
    if (sRequested) return;
    const int request = DWC::GetGHTTPDataEx(MULTIPLIER_URL, 32, true, nullptr, OnMultiplierDownloaded, nullptr);
    if (request >= 0) sRequested = true;
}

float GetMultiplier() { return sValid ? sMultiplier : 1.0f; }
bool IsWeekendMultiplierActive() { return false; }
bool IsWeekendMultiplierActiveForRegion(u8) { return false; }
bool IsItemRainEventActive() { return false; }

} // namespace PointRating
} // namespace Pulsar