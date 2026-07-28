#include <Network/Rating/RatingSync.hpp>

namespace Pulsar {
namespace PointRating {

// Vanza keeps ratings authoritative on SD. Server pull is intentionally disabled
// until its endpoint is implemented; this also avoids RR's per-frame HTTP lifetime bug.
void SetSyncReportingSuppressed(bool) {}
void ReportCurrentRatings(u32) {}
void StartLoginRatingDownload(s32, u32) {}

} // namespace PointRating
} // namespace Pulsar

