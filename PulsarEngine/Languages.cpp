#include <VanzaKart.hpp>

namespace Languages {

static bool IsItalian() {
    return Pulsar::Settings::Mgr::Get().GetUserSettingValue(
               static_cast<Pulsar::Settings::UserType>(Pulsar::Settings::SETTINGSTYPE_LANGUAGE),
               Pulsar::SCROLLER_LANGUAGE) == Pulsar::LANGUAGE_ITALIAN;
}

// These bytes are the language letters embedded in the game's archive paths.
// English keeps the PAL defaults; Italian selects the corresponding *_I archives.
void ApplyArchiveLanguage() {
    const bool italian = IsItalian();
    FontRename = 0x46;                     // F
    RaceRename = italian ? 0x49 : 0x53;    // I / S
    CommonRename = italian ? 0x49 : 0x52;  // I / R
    AwardRename = italian ? 0x49 : 0x53;   // I / S
}
BootHook LanguageArchiveHook(ApplyArchiveLanguage, 4);

} // namespace Languages

