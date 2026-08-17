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

static const u32 MAX_TRACK_BLOCKING = 12; // Maximum number of blocked tracks synced via packets

/*
    Each VanzaKart worldwide mode matchmakes in its own DWC region. The region is what ends up
    in the "rk" key of the QR2 record ("vs_105" and so on), and that key is the only thing that
    keeps two public rooms apart: two players whose rk differs can never be matched together,
    two players whose rk matches always can, no matter which mode they picked in the menu.

    Everything that talks to the server (the rk string, the RKNet controller region, the player
    counters on the WFC pages) must therefore read REGIONID, which follows the mode the player
    chose. Info::GetWiimmfiRegion is the pack's base region: it is a constant, it is what the
    login region is built from, and it is only correct for plain VS worldwide.
*/
enum ModeRegion {
    REGION_VS = 0x68,        // plain VS worldwide, same value as the pack's base region
    REGION_OTT = 0x69,       // Online TT worldwide
    REGION_200CC = 0x70,     // 200cc worldwide
    REGION_ITEMRAIN = 0x71,  // Item Rain worldwide
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