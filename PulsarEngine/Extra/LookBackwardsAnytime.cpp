#include <kamek.hpp>
#include <PulsarSystem.hpp>

namespace VanzaKart {
namespace Race {

// Allow Looking Backwards Anytime [Ro, Gaberboo]
kmWrite32(0x805A228C, 0x60000000);
kmWrite32(0x805A225C, 0x38800001);

} // namespace Race
} // namespace VanzaKart
