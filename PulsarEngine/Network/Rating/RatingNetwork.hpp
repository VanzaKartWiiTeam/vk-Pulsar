#ifndef _PULSAR_RATING_NETWORK_HPP_
#define _PULSAR_RATING_NETWORK_HPP_

#include <kamek.hpp>
#include <Network/Rating/RatingTypes.hpp>

/*
    Everything the rating system puts on, or takes off, the wire:
      - the rank and VR decimals carried by PulSELECT,
      - the ev/eb values published through QR2,
      - binding the licence to its online profile at login.

    The transport itself stays in PulSELECT.cpp; this module only decides what the
    payload contains.
*/

namespace Pulsar {

namespace Network {
struct PulSELECT;
}

namespace PointRating {
namespace Net {

// Writes this console's rank and VR decimals into an outgoing SELECT packet.
void FillLocalPayload(Network::PulSELECT* packet);

// Zeroes the rating payload of a packet that arrived in vanilla (short) form.
void ClearPayload(Network::PulSELECT* packet);

// Caches the rank and decimals of a remote console after reception.
void CacheRemotePayload(u8 aid, const Network::PulSELECT* packet);

// Clamps a rating to the range QR2 publishes: rating * 100, within [1, 500000].
int ClampForQr2(RatingValue rating);

// Associates the active licence with its gsProfileId. Called just before DWC login.
void BindActiveLicenseProfile();

}  // namespace Net
}  // namespace PointRating
}  // namespace Pulsar

#endif  // _PULSAR_RATING_NETWORK_HPP_
