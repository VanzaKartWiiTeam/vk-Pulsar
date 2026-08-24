#ifndef _PUL_NETWORK_
#define _PUL_NETWORK_

#include <Config.hpp>

namespace Pulsar {
namespace Network {

enum DenyType {
    DENY_TYPE_NORMAL,
    DENY_TYPE_BAD_PACK,
    DENY_TYPE_OTT,
    DENY_TYPE_KICK,
};


static const u32 MAX_TRACK_BLOCKING = 12;

inline bool IsGroupedTrack(PulsarId id) {
    if(id < PULSARID_FIRSTCT || id == PULSARID_NONE) return false;
    switch(id - PULSARID_FIRSTCT) {
        default: return false; //no group defined for this pack
    }
}
#ifdef BETA
#  define VK_REGION_OFFSET 100
#else
#  define VK_REGION_OFFSET 0
#endif

enum ModeRegion {
    REGION_VS = 104 + VK_REGION_OFFSET,        // plain VS worldwide, same value as the pack's base region
    REGION_OTT = 105 + VK_REGION_OFFSET,       // Online TT worldwide
    REGION_200CC = 112 + VK_REGION_OFFSET,     // 200cc worldwide
    REGION_ITEMRAIN = 113 + VK_REGION_OFFSET,  // Item Rain worldwide
};

extern u32 REGIONID;

// True for the regions that carry a mode of their own, i.e. everything but plain VS.
// These must survive a section load: they are the only record of which mode was picked.
inline bool IsModeRegion(u32 region) {
    return region == REGION_OTT || region == REGION_200CC || region == REGION_ITEMRAIN;
}

class Mgr { //Manages network related stuff within Pulsar
public:
    Mgr() : racesPerGP(3), curBlockingArrayIdx(0), lastGroupedTrackPlayed(false) {}
    u32 hostContext;
    DenyType denyType;
    u8 deniesCount;
    u8 ownStatusData;
    u8 statusDatas[30];
    u8 curBlockingArrayIdx;
    u8 racesPerGP;
    bool lastGroupedTrackPlayed;
    u8 padding[1];
    PulsarId* lastTracks;
};

}//namespace Network
}//namespace Pulsar

#endif