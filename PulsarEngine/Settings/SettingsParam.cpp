#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <Config.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar {

namespace Settings {

u8 Params::radioCount[Params::pageCount] ={
    5, 6, 5, 5, 2, 2, 0//menu, race, host, OTT, KO, Extended Teams
    
};
u8 Params::scrollerCount[Params::pageCount] ={ 2, 2, 1, 0, 2, 0, 1}; //menu, race (SOM + item mode), host, OTT, KO, Extended Teams

u8 Params::buttonsPerPagePerRow[Params::pageCount][Params::maxRadioCount] = //first row is PulsarSettingsType, 2nd is rowIdx of radio
{
    { 2, 2, 3, 2, 2, 0, 0, 0 }, //Menu: FastMenus (2), Layout (2), Music (3), InputViewer (2), BRSAR (2)
    { 2, 2, 2, 2, 3, 4, 0, 0 }, //Race: Mii (2), Speedup (2), Battle (2), FPS (2), SOM (3), TTItems (4)
    { 2, 2, 4, 2, 2, 0, 0, 0 }, //Host: last row is the ranked friend room toggle
    { 3, 3, 2, 2, 2, 0, 0, 0 }, //OTT
    { 2, 2, 0, 0, 0, 0, 0, 0 }, //KO
    { 2, 2, 0, 0, 0, 0, 0, 0 }, //Extended Teams
    { 0, 0, 0, 0, 0, 0, 0, 0 }, //Language
};

u8 Params::optionsPerPagePerScroller[Params::pageCount][Params::maxScrollerCount] =
{
    { 5, 13, 0, 0, 0, 0, 0, 0}, //Menu: Boot (5), HUD Color (13)
    { 4, 3, 0, 0, 0, 0, 0, 0}, //Race: SOM (4), Race mode (3: normal, item rain, countdown)
    { 7, 0, 0, 0, 0, 0, 0, 0}, //Host: GP (7)
    { 0, 0, 0, 0, 0, 0, 0, 0}, //OTT
    { 4, 4, 0, 0, 0, 0, 0, 0}, //KO
    { 0, 0, 0, 0, 0, 0, 0, 0}, //Extended Teams
    { 2, 0, 0, 0, 0, 0, 0, 0}, //Language: English, Italian
};

}//namespace Settings
}//namespace Pulsar



