#include <hooks.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <MarioKartWii/Item/Obj/ObjProperties.hpp>
#include <MarioKartWii/Item/Obj/Gesso.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <MarioKartWii/Item/ItemPlayer.hpp>
#include <kamek.hpp>
#include <Settings/Settings.hpp>


namespace Pulsar {
namespace Race {
static bool isGivenItem;
void restartItem(){
    isGivenItem = false;
    OS::Report("[TEST LOG DIO CANE]PulsarEngine: removing item from player 1 for TT\n", 0);
}
void itemOnTT(){
    bool isTT = DriverMgr::isTT;
    if(isTT==true && isGivenItem == false){
        if(Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_RADIO_TT) == RACESETTING_ITEM_DISABLED){
            Item::Manager::sInstance->players[0].inventory.SetItem(TRIPLE_MUSHROOM, true);
            isGivenItem = true;
            OS::Report("[TEST LOG DIO CANE]PulsarEngine: Giving player 1 a Triple Mushroom for TT\n", 0);
        }
        else if(Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_RADIO_TT) == RACESETTING_ITEM_STAR){
            Item::Manager::sInstance->players[0].inventory.SetItem(STAR, true);
            isGivenItem = true;
            OS::Report("[TEST LOG DIO CANE]PulsarEngine: Giving player 1 a Star for TT\n", 0);
        }
        else if(Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_RACE, SETTINGRACE_RADIO_TT) == RACESETTING_ITEM_MEGA){
            Item::Manager::sInstance->players[0].inventory.SetItem(MEGA_MUSHROOM, true);
            isGivenItem = true;
            OS::Report("[TEST LOG DIO CANE]PulsarEngine: Giving player 1 a Mega Mushroom for TT\n", 0);
        }
    }
}
RaceLoadHook megaRestart(restartItem);
RaceFrameHook mega(itemOnTT);
}
}