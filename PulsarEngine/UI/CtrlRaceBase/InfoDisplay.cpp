#include <PulsarSystem.hpp>
#include <UI/CtrlRaceBase/InfoDisplay.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <SlotExpansion/CupsConfig.hpp>


namespace Pulsar {
namespace UI {
//So that it is only done once in TTs
u32 CtrlRaceTrackInfoDisplay::lastCourse = -1;

u32 CtrlRaceTrackInfoDisplay::Count() {
    const u32 gamemode = Racedata::sInstance->racesScenario.settings.gamemode;
    if((gamemode == MODE_GRAND_PRIX) || (gamemode == MODE_VS_RACE) || (gamemode == MODE_PUBLIC_VS) || (gamemode == MODE_PRIVATE_VS)) return 1;
    const PulsarId winning = CupsConfig::sInstance->GetWinning();
    if(gamemode == MODE_TIME_TRIAL && winning != lastCourse) {
        lastCourse = winning;
        return 1;
    }
    return 0;
}
void CtrlRaceTrackInfoDisplay::Create(Page& page, u32 index, u32) {
    CtrlRaceTrackInfoDisplay* info = new(CtrlRaceTrackInfoDisplay);
    page.AddControl(index, *info, 0);
    info->Load();
}
static CustomCtrlBuilder INFODISPLAYPANEL(CtrlRaceTrackInfoDisplay::Count, CtrlRaceTrackInfoDisplay::Create);


void CtrlRaceTrackInfoDisplay::Load() {
    this->hudSlotId = 0;

    ControlLoader loader(this);
    loader.Load("game_image", "CTInfo", "CTInfo", nullptr);
    this->textBox_00 = this->layout.GetPaneByName("TextBox_00");
    Text::Info info;
    info.bmgToPass[0] = GetCurTrackBMG();
    info.bmgToPass[1] = GetCurTrackAuthorBMG();
    this->SetMessage(BMG_INFO_DISPLAY, &info);
}

}//namespace UI
}//namespace Pulsar
