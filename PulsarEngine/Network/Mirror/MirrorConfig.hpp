#ifndef _PULSAR_MIRROR_CONFIG_HPP_
#define _PULSAR_MIRROR_CONFIG_HPP_

#include <kamek.hpp>

/*
    Dual send.

    Every ranked race is reported a second time, to the VanzaKart server, on top of
    whatever the server the client is actually connected to records on its own. It exists
    because the leaderboard data on the other side is closed: nothing comes back from
    there, so the only way VanzaKart keeps its own leaderboard alive is to have the
    console tell it directly.

    Two constraints shaped this:

    Plain HTTP on a grey (DNS-only) hostname is deliberate, not an oversight. The Wii's
    GHTTP stack predates modern TLS and cannot go through the Cloudflare proxy - the same
    reason nas, naswii and *.gs have to stay grey. Rate limiting therefore has to live on
    nginx, because Cloudflare never sees this traffic.

    Nothing is ever sent while a race is running. The network stack is shared with the
    peer-to-peer race traffic, and an HTTP request mid-race buys lag or a desync. The
    result is captured into memory at the finish and flushed on the next section load.
*/

namespace Pulsar {
namespace Mirror {
namespace Config {

// Master switch. Stays 0 until the endpoint is actually live and rate limited; with it
// off nothing is captured and nothing touches the network.
#define MIRROR_ENABLED 0

static const char* const RACE_REPORT_URL = "http://mirror.vanzakart.net:8000/api/race";

// The wire format, bumped whenever the JSON below changes shape, so the server can keep
// accepting reports from builds that are still out in the wild.
static const u32 REPORT_FORMAT_VERSION = 1;

// A full 12 player race serialises to roughly 700 bytes. The margin covers the header
// fields and leaves room for a player count that grows later.
static const u32 REPORT_BUFFER_SIZE = 1536;

// Replies are acknowledgements, not data; anything longer is a misconfigured endpoint.
static const u32 REPORT_RESPONSE_MAX = 128;

// A capture older than this is dropped rather than sent: the player has moved on and a
// stale result is worse than a missing one.
static const u32 CAPTURE_MAX_AGE_SECTIONS = 4;

}  // namespace Config
}  // namespace Mirror
}  // namespace Pulsar

#endif
