#include <Network/Rating/MultiplierManager.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <PulsarSystem.hpp>
#include <core/System/SystemManager.hpp>
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

// ------------------------------------------------------------------ calendar
// The console RTC, which SystemManager keeps as the last two digits of the year.

struct Date {
    u16 year;
    u8 month;
    u8 day;
    bool isValid;
};

static Date GetToday() {
    Date date;
    date.year = 0;
    date.month = 0;
    date.day = 0;
    date.isValid = false;

    const SystemManager* system = SystemManager::sInstance;
    if (system != nullptr && system->isValidDate) {
        date.year = (u16)system->year + 2000;
        date.month = system->month;
        date.day = system->day;
        date.isValid = true;
    }
    return date;
}

// Zeller's congruence, shifted so that 0 is Sunday.
static u8 GetDayOfWeek(u16 year, u8 month, u8 day) {
    int m = month;
    int y = year;
    if (m < 3) {
        m += 12;
        y -= 1;
    }
    const int k = y % 100;
    const int j = y / 100;
    const int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    return (u8)((h + 6) % 7);
}

static bool IsLeapYear(u16 year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Whole weeks elapsed since 1 January of WEEK_EPOCH_YEAR.
static u32 GetWeekNumber(const Date& date) {
    if (!date.isValid || date.year < Config::WEEK_EPOCH_YEAR) return 0;

    s32 days = 0;
    for (u16 year = Config::WEEK_EPOCH_YEAR; year < date.year; ++year) {
        days += IsLeapYear(year) ? 366 : 365;
    }
    static const u8 daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (u8 month = 1; month < date.month; ++month) {
        days += daysInMonth[month - 1];
        if (month == 2 && IsLeapYear(date.year)) days += 1;
    }
    days += date.day - 1;
    return (u32)(days / 7);
}

static bool IsEventDay(u8 month, u8 day) {
    return (month == 12 && day >= 23) ||  // Christmas
           (month == 1 && day <= 3) ||  // New Year
           (month == 10 && day >= 25) ||  // Halloween
           (month == 6 && day >= 5 && day <= 8) ||  // Start of summer
           (month == 3 && day >= 13 && day <= 17) ||  // St. Patrick's Day
           (month == 4 && day >= 10 && day <= 14) ||  // MKWii's birthday
           (month == 8 && day >= 23 && day <= 29);  // End of summer
}

// -------------------------------------------------------------------- layers

float GetEventLayer() {
    const Date today = GetToday();
    if (!today.isValid) return 1.0f;
    return IsEventDay(today.month, today.day) ? Config::EVENT_MULTIPLIER : 1.0f;
}

bool IsWeekendActive() {
    const Date today = GetToday();
    if (!today.isValid) return false;

    const u8 dayOfWeek = GetDayOfWeek(today.year, today.month, today.day);
    if (dayOfWeek != 0 && dayOfWeek != 6) return false;  // Sunday or Saturday only

    return (GetWeekNumber(today) % 2) == 1;
}

bool IsWeekendActiveForRegion(u8 region) {
    if (!IsWeekendActive()) return false;
    const u32 week = GetWeekNumber(GetToday());
    return Config::WEEKEND_REGIONS[(week / 2) % 3] == region;
}

float GetWeekendLayer(u8 region) {
    const Date today = GetToday();
    const bool aprilFools = today.isValid && today.month == 4 && today.day == 1;
    return (IsWeekendActiveForRegion(region) || aprilFools) ? Config::WEEKEND_MULTIPLIER : 1.0f;
}

// Retro Rewind ties an item rain window to a date here. VanzaKart drives item rain from
// the room settings instead, so no date ever turns it on by itself.
bool IsItemRainEventActive() {
    return false;
}

float Get() {
    u8 region = 0xFF;
    if (System::sInstance != nullptr) region = (u8)System::sInstance->GetInfo().GetWiimmfiRegion();

    float multiplier = GetEventLayer() * GetWeekendLayer(region) * GetRemoteLayer();
#ifdef BETA
    multiplier *= Config::BETA_MULTIPLIER;
#endif
#ifdef PROD
    multiplier *= Config::MULTIPLIER;
#endif
    return multiplier;
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
