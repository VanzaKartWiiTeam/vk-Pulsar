#include <Network/Rating/RatingStorage.hpp>
#include <Network/Rating/RatingCalculator.hpp>
#include <Network/Rating/RatingConfig.hpp>
#include <Network/Rating/RatingSync.hpp>
#include <IO/IO.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/System/Rating.hpp>

namespace Pulsar {
namespace PointRating {
namespace Storage {

static const u32 ENTRY_FLAG_VALID = 1;
static const u32 ENTRY_FLAG_RANK_SHIFT = 8;
static const u32 ENTRY_FLAG_RANK_MASK = 0x0f;

struct ProfileEntry {
    s32 profileId;
    RatingValue vr;
    RatingValue br;
    RankId rank;
    bool hasData;
    u32 totalRaces;
    u32 wins;
};

struct LicenseBackup {
    u16 originalVr;
    u16 originalBr;
    bool hasOriginal;
};

struct PackedHeader {
    u32 magic;
    u16 version;
    u16 count;
};

// v1 (Retro Rewind) and v2 (rank packed into flags) share this 16 byte layout.
struct PackedEntryLegacy {
    s32 profileId;
    float vr;
    float br;
    u32 flags;
};

// v3 keeps the legacy prefix byte-for-byte and appends room for statistics.
struct PackedEntryV3 {
    s32 profileId;  // 0x00
    float vr;  // 0x04
    float br;  // 0x08
    u32 flags;  // 0x0C  bit0 valid, bits 8-11 legacy rank mirror
    u8 rank;  // 0x10  authoritative
    u8 reserved0[3];
    u32 totalRaces;  // 0x14
    u32 wins;  // 0x18
    u32 reserved1;  // 0x1C
};

static ProfileEntry sProfiles[Config::MAX_PROFILES] = {};
static LicenseBackup sBackups[Config::MAX_LICENSES] = {};
static s32 sBoundProfileIds[Config::MAX_LICENSES] = {};
static u32 sReplaceIdx = 0;
static bool sLoaded = false;
static u32 sBatchDepth = 0;
static bool sDirty = false;
static char sPath[IOS::ipcMaxPath] __attribute__((aligned(32))) = {};

static void Flush();

/*
    A gsProfileId is usable as a storage key as soon as it is positive.

    This deliberately has no upper bound. Retro Rewind rejected anything at or above
    1000000000, treating that as a reserved range, but VanzaKart's server hands out ids
    right there (1000000134, 1000000211, ...). With that bound in place every single
    profile was rejected, so nothing was ever loaded or saved: the rating read back as
    the default 5000 forever, the licence mirror never ran, and the value that went over
    the wire was the vanilla VR, which the display then read as an internal value and
    rendered a hundred times too large.
*/
static bool IsUsableProfileId(s32 id) {
    return id > 0;
}

static const char* GetPath() {
    if (sPath[0] == '\0') {
        const System* sys = System::sInstance;
        if (sys == nullptr) return nullptr;
        snprintf(sPath, IOS::ipcMaxPath, "%s/%s", sys->GetModFolder(), Config::SAVE_FILE_NAME);
    }
    return sPath;
}

// Marks the in-memory state dirty and writes it out unless a batch is open.
static void Touch() {
    sDirty = true;
    if (sBatchDepth == 0) Flush();
}

void BeginBatch() {
    ++sBatchDepth;
}

void EndBatch() {
    if (sBatchDepth == 0) return;
    --sBatchDepth;
    if (sBatchDepth == 0 && sDirty) Flush();
}

static ProfileEntry* FindProfile(s32 id) {
    if (!IsUsableProfileId(id)) return nullptr;
    for (u32 i = 0; i < Config::MAX_PROFILES; ++i) {
        if (sProfiles[i].hasData && sProfiles[i].profileId == id) return &sProfiles[i];
    }
    return nullptr;
}

static bool IsBoundToALicense(s32 id) {
    for (u32 i = 0; i < Config::MAX_LICENSES; ++i) {
        if (sBoundProfileIds[i] == id) return true;
    }
    return false;
}

static ProfileEntry* AllocProfile(s32 id) {
    for (u32 i = 0; i < Config::MAX_PROFILES; ++i) {
        if (!sProfiles[i].hasData) {
            sProfiles[i].profileId = id;
            sProfiles[i].rank = 0;
            sProfiles[i].totalRaces = 0;
            sProfiles[i].wins = 0;
            return &sProfiles[i];
        }
    }

    // Table full: evict round-robin, but never a profile bound to a local licence,
    // or the player currently playing would lose their own rating.
    for (u32 attempt = 0; attempt < Config::MAX_PROFILES; ++attempt) {
        ProfileEntry& candidate = sProfiles[sReplaceIdx];
        sReplaceIdx = (sReplaceIdx + 1) % Config::MAX_PROFILES;
        if (IsBoundToALicense(candidate.profileId)) continue;

        candidate.hasData = false;
        candidate.vr = 0.0f;
        candidate.br = 0.0f;
        candidate.rank = 0;
        candidate.totalRaces = 0;
        candidate.wins = 0;
        candidate.profileId = id;
        return &candidate;
    }
    return nullptr;
}

static ProfileEntry* GetProfile(s32 id, bool create) {
    if (!IsUsableProfileId(id)) return nullptr;
    ProfileEntry* e = FindProfile(id);
    if (e != nullptr) return e;
    if (!create) return nullptr;
    return AllocProfile(id);
}

static s32 ResolveProfileIdForLicense(u32 licenseId) {
    if (licenseId >= Config::MAX_LICENSES) return 0;

    RKSYS::Mgr* mgr = RKSYS::Mgr::sInstance;
    if (mgr != nullptr) {
        const s32 liveProfileId = mgr->licenses[licenseId].dwcAccUserData.gsProfileId;
        if (IsUsableProfileId(liveProfileId)) {
            sBoundProfileIds[licenseId] = liveProfileId;
            return liveProfileId;
        }
    }

    const s32 boundProfileId = sBoundProfileIds[licenseId];
    return IsUsableProfileId(boundProfileId) ? boundProfileId : 0;
}

static ProfileEntry* GetProfileForLicense(u32 licenseId, bool create) {
    return GetProfile(ResolveProfileIdForLicense(licenseId), create);
}

static void AdoptLegacyEntry(ProfileEntry& dest, const PackedEntryLegacy& src, u16 version) {
    dest.profileId = src.profileId;
    dest.vr = src.vr;
    dest.br = src.br;
    dest.hasData = true;
    dest.totalRaces = 0;
    dest.wins = 0;
    // v1 predates ranks entirely, so the rank is rebuilt from the rating itself.
    dest.rank = (version >= 2) ? (RankId)((src.flags >> ENTRY_FLAG_RANK_SHIFT) & ENTRY_FLAG_RANK_MASK)
                               : 0;
}

void Load() {
    if (sLoaded) return;
    sLoaded = true;

    IO* io = IO::sInstance;
    const char* path = GetPath();
    if (io == nullptr || path == nullptr || !io->OpenFile(path, FILE_MODE_READ)) return;

    union {
        PackedHeader h;
        u8 pad[32];
    } hBuf __attribute__((aligned(32))) = {};

    if (io->Read(sizeof(PackedHeader), &hBuf.h) != (s32)sizeof(PackedHeader)) {
        io->Close();
        return;
    }
    const u16 version = hBuf.h.version;
    if (hBuf.h.magic != Config::MAGIC || version < 1 || version > Config::VERSION) {
        io->Close();
        return;
    }

    const u16 count = (hBuf.h.count < Config::MAX_PROFILES) ? hBuf.h.count : (u16)Config::MAX_PROFILES;
    u32 slot = 0;
    for (u16 i = 0; i < count && slot < Config::MAX_PROFILES; ++i) {
        union {
            PackedEntryV3 v3;
            PackedEntryLegacy legacy;
            u8 pad[32];
        } eBuf __attribute__((aligned(32))) = {};

        const u32 entrySize = (version >= 3) ? sizeof(PackedEntryV3) : sizeof(PackedEntryLegacy);
        if (io->Read(entrySize, &eBuf) != (s32)entrySize) break;

        if (version >= 3) {
            if (!(eBuf.v3.flags & ENTRY_FLAG_VALID) || !IsUsableProfileId(eBuf.v3.profileId)) continue;
            ProfileEntry& dest = sProfiles[slot++];
            dest.profileId = eBuf.v3.profileId;
            dest.vr = eBuf.v3.vr;
            dest.br = eBuf.v3.br;
            dest.rank = eBuf.v3.rank;
            dest.hasData = true;
            dest.totalRaces = eBuf.v3.totalRaces;
            dest.wins = eBuf.v3.wins;
        } else {
            if (!(eBuf.legacy.flags & ENTRY_FLAG_VALID) || !IsUsableProfileId(eBuf.legacy.profileId)) continue;
            AdoptLegacyEntry(sProfiles[slot++], eBuf.legacy, version);
        }
    }
    io->Close();
}

static void Flush() {
    IO* io = IO::sInstance;
    const char* path = GetPath();
    if (io == nullptr || path == nullptr) return;

    struct FileImage {
        PackedHeader h;
        PackedEntryV3 e[Config::MAX_PROFILES];
        u8 pad[32];
    };
    static FileImage file __attribute__((aligned(32)));
    memset(&file, 0, sizeof(file));

    file.h.magic = Config::MAGIC;
    file.h.version = Config::VERSION;
    file.h.count = (u16)Config::MAX_PROFILES;

    for (u32 i = 0; i < Config::MAX_PROFILES; ++i) {
        const ProfileEntry& src = sProfiles[i];
        PackedEntryV3& dest = file.e[i];
        dest.profileId = src.profileId;
        dest.vr = src.vr;
        dest.br = src.br;
        dest.rank = src.rank;
        dest.totalRaces = src.totalRaces;
        dest.wins = src.wins;
        // The legacy rank mirror keeps an older build able to read this file.
        dest.flags = (src.hasData ? ENTRY_FLAG_VALID : 0u) |
                     (((u32)src.rank & ENTRY_FLAG_RANK_MASK) << ENTRY_FLAG_RANK_SHIFT);
    }

    if (!io->OpenFile(path, FILE_MODE_WRITE) && !io->CreateAndOpen(path, FILE_MODE_WRITE)) return;
    io->Overwrite(sizeof(FileImage), &file);
    io->Close();
    sDirty = false;
}

static u16 ClampU16(float v) {
    const float clamped = Calculator::ClampToLimits(v);
    return (u16)clamped;
}

RatingValue GetVR(u32 licenseId) {
    Load();
    const ProfileEntry* e = GetProfileForLicense(licenseId, false);
    return (e != nullptr && e->hasData) ? e->vr : Config::DEFAULT_RATING;
}

RatingValue GetBR(u32 licenseId) {
    Load();
    const ProfileEntry* e = GetProfileForLicense(licenseId, false);
    return (e != nullptr && e->hasData) ? e->br : Config::DEFAULT_RATING;
}

void SetVR(u32 licenseId, RatingValue vr) {
    Load();
    ProfileEntry* e = GetProfileForLicense(licenseId, true);
    if (e == nullptr) return;
    e->vr = Calculator::ClampToLimits(vr);
    e->hasData = true;
    Touch();
    ReportCurrentRatings(licenseId);
}

void SetBR(u32 licenseId, RatingValue br) {
    Load();
    ProfileEntry* e = GetProfileForLicense(licenseId, true);
    if (e == nullptr) return;
    e->br = Calculator::ClampToLimits(br);
    e->hasData = true;
    Touch();
    ReportCurrentRatings(licenseId);
}

RankId GetRank(u32 licenseId) {
    Load();
    const ProfileEntry* e = GetProfileForLicense(licenseId, false);
    if (e == nullptr || !e->hasData) return 0;
    return (e->rank <= Config::MAX_RANK) ? e->rank : 0;
}

void SetRank(u32 licenseId, RankId rank) {
    Load();
    ProfileEntry* e = GetProfileForLicense(licenseId, true);
    if (e == nullptr) return;
    e->rank = (rank <= Config::MAX_RANK) ? rank : Config::MAX_RANK;
    e->hasData = true;
    Touch();
}

void SaveProfileVR(s32 profileId, RatingValue vr) {
    Load();
    ProfileEntry* e = GetProfile(profileId, true);
    if (e == nullptr) return;
    e->vr = Calculator::ClampToLimits(vr);
    e->hasData = true;
    Touch();
}

void SaveProfileBR(s32 profileId, RatingValue br) {
    Load();
    ProfileEntry* e = GetProfile(profileId, true);
    if (e == nullptr) return;
    e->br = Calculator::ClampToLimits(br);
    e->hasData = true;
    Touch();
}

void BindLicenseProfileId(u32 licenseId, s32 profileId) {
    if (licenseId >= Config::MAX_LICENSES) return;
    if (IsUsableProfileId(profileId)) sBoundProfileIds[licenseId] = profileId;
}

bool GetOriginalLicenseRatings(u32 idx, u16& vr, u16& br) {
    if (idx >= Config::MAX_LICENSES || !sBackups[idx].hasOriginal) return false;
    vr = sBackups[idx].originalVr;
    br = sBackups[idx].originalBr;
    return true;
}

// Shared by both licence hooks: remember the vanilla values once, seed the profile
// from them on first contact, then mirror our values into the live licence.
static void SyncLicense(u32 idx, RKSYS::LicenseMgr& lic, bool refreshBackup) {
    Load();
    if (idx >= Config::MAX_LICENSES) return;

    if (refreshBackup || !sBackups[idx].hasOriginal) {
        sBackups[idx].originalVr = ClampU16((float)lic.vr.points);
        sBackups[idx].originalBr = ClampU16((float)lic.br.points);
        sBackups[idx].hasOriginal = true;
    }

    ProfileEntry* e = GetProfile(ResolveProfileIdForLicense(idx), true);
    if (e == nullptr) return;

    if (!e->hasData) {
        // First import: adopt the vanilla rating, which is stored times 100.
        e->vr = (float)sBackups[idx].originalVr / 100.0f;
        e->br = (float)sBackups[idx].originalBr / 100.0f;
        e->hasData = true;
        Touch();
    }

    lic.vr.points = ClampU16(e->vr);
    lic.br.points = ClampU16(e->br);
}

void ApplyToLicense(u32 idx, RKSYS::LicenseMgr& lic) {
    SyncLicense(idx, lic, true);
}

void StoreFromLicense(u32 idx, RKSYS::LicenseMgr& lic) {
    SyncLicense(idx, lic, false);
}

}  // namespace Storage
}  // namespace PointRating
}  // namespace Pulsar
