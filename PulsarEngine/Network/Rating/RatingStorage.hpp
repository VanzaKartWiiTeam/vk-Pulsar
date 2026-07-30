#ifndef _PULSAR_RATING_STORAGE_HPP_
#define _PULSAR_RATING_STORAGE_HPP_

#include <kamek.hpp>
#include <Network/Rating/RatingTypes.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>

/*
    Persistence for VKRating.pul.

    Entries are keyed by the online gsProfileId rather than by licence slot, so two
    licences can never swap ratings with each other.  The licence's vanilla u16 VR/BR
    are only ever a display mirror: the original values are restored before rksys.dat
    is written back, so the vanilla save stays untouched.

    The reader accepts v1 (Retro Rewind, no rank), v2 (rank packed in flags) and v3;
    the writer only ever emits v3.  Migration happens on the first save.
*/

namespace Pulsar {
namespace PointRating {
namespace Storage {

// Reads the file once. Every accessor calls it, so there is no init ordering hazard.
void Load();

RatingValue GetVR(u32 licenseId);
RatingValue GetBR(u32 licenseId);
void SetVR(u32 licenseId, RatingValue vr);
void SetBR(u32 licenseId, RatingValue br);

RankId GetRank(u32 licenseId);
void SetRank(u32 licenseId, RankId rank);

// Direct by-profile writes, used when the server hands us authoritative values.
void SaveProfileVR(s32 profileId, RatingValue vr);
void SaveProfileBR(s32 profileId, RatingValue br);

// Associates a licence with its online profile, before or independently of login.
void BindLicenseProfileId(u32 licenseId, s32 profileId);

/*
    Coalesces the writes of one end-of-race commit into a single flush.  Without it a
    race costs two full rewrites of the file (one for the rating, one for the rank).
    Calls nest safely; the flush happens when the outermost EndBatch runs.
*/
void BeginBatch();
void EndBatch();

// Licence mirror, driven by the save-manager hooks.
void ApplyToLicense(u32 idx, RKSYS::LicenseMgr& lic);
void StoreFromLicense(u32 idx, RKSYS::LicenseMgr& lic);
bool GetOriginalLicenseRatings(u32 idx, u16& vr, u16& br);

}  // namespace Storage
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_RATING_STORAGE_HPP_
