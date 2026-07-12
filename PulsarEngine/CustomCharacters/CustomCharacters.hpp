#ifndef _PULSAR_CUSTOM_CHARACTERS_STUB_
#define _PULSAR_CUSTOM_CHARACTERS_STUB_

#include <kamek.hpp>

class LayoutUIControl;

namespace Pulsar {
namespace CustomCharacters {

inline bool FindLooseSoundEffectPath(u32 fileId, const char* extension, char* path, u32 pathSize, u32* outFileSize = nullptr) {
    return false;
}

inline const char* GetLooseVoicePostfixForGroup(u32 groupId, const char*& groupSuffix, const char*& voiceName) {
    groupSuffix = nullptr;
    voiceName = nullptr;
    return nullptr;
}

inline bool SetRaceNameTextIfCustom(LayoutUIControl& control, const char* paneName, u8 playerId) {
    return false;
}

} // namespace CustomCharacters
} // namespace Pulsar

#endif // _PULSAR_CUSTOM_CHARACTERS_STUB_
