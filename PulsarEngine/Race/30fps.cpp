#include <kamek.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace Race {

kmWrite32(0x80554224, 0x3C808000);
kmWrite32(0x80554228, 0x88841200);
kmWrite32(0x8055422C, 0x48000044);

kmRuntimeUse(0x80001200);

void ApplyFPSSetting() {
    const Racedata* racedata = Racedata::sInstance;
    if(racedata == nullptr) return;
    
    const GameMode mode = racedata->racesScenario.settings.gamemode;
    
    // always 60 fps in time trial
    if(mode == MODE_TIME_TRIAL || mode == MODE_GHOST_RACE) {
        *reinterpret_cast<u8*>(kmRuntimeAddr(0x80001200)) = 0;
        return;
    }
    
    const Settings::Mgr& settings = Settings::Mgr::Get();
    u8 fpsSetting = settings.GetUserSettingValue(Settings::SETTINGSTYPE_RACE1, SETTINGRACE1_RADIO_FPS);
    
    *reinterpret_cast<u8*>(kmRuntimeAddr(0x80001200)) = (fpsSetting == RACE1SETTING_FPS_30) ? 1 : 0;
}

static SectionLoadHook ApplyFPS(ApplyFPSSetting);
static RaceLoadHook ApplyFPSRace(ApplyFPSSetting);

} //namespace Race
} //namespace Pulsar