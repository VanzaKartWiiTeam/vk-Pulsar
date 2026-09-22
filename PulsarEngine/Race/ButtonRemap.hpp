#ifndef _PUL_BUTTONREMAP_
#define _PUL_BUTTONREMAP_
#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

//Per-license button remapping for race inputs. Only the race State is remapped, the UIState is left alone
//so menus always keep the original buttons and a bad remap can never lock a player out of them.

namespace Pulsar {
namespace ButtonRemap {

//A profile is one u32 per (license, controller type): one nibble per action, 0 = the game's default buttons,
//n = the n-th entry of that controller's button list. Start/+ and Home are never in the list.
static const u32 licenseCount = 4;
static const u32 typeCount = 4; //WHEEL, NUNCHUCK, CLASSIC, GCN
static const u32 maxActions = 6;
static const u32 noLicense = licenseCount;

struct ButtonDef {
    u16 bit; //in State::buttonRaw
    const wchar_t* name;
    const wchar_t* nameItalian; //nullptr when it is the same
};

struct ActionDef {
    u8 nameId;
    u16 defaultMask; //raw buttons the game uses for this action
    u16 primary; //raw button fed to the game when a remapped button triggers the action
    u16 actionBits; //State::buttonActions bits driven by the action, 0 for the trick
};

struct ControllerDef {
    const ActionDef* actions;
    u32 actionCount;
    const ButtonDef* buttons;
    u32 buttonCount;
    u16 dpadMask; //shown as a single "D-Pad" when an action has all of it
};

bool IsRemappable(ControllerType type);
const ControllerDef& GetControllerDef(ControllerType type);
const wchar_t* GetActionName(const ActionDef& action);
const wchar_t* GetButtonName(const ButtonDef& button);
bool IsItalian();

u32 GetCurrentLicense(); //noLicense if none is loaded
u32 GetProfile(u32 license, ControllerType type);
inline u32 GetAssignment(u32 profile, u32 actionIdx) { return (profile >> (actionIdx * 4)) & 0xF; }
u16 GetActionButtons(u32 profile, const ControllerDef& def, u32 actionIdx); //buttons that currently trigger the action
//Returns the new profile. If the button was the only one left to another action, that action takes the old button.
u32 AssignButton(u32 profile, const ControllerDef& def, u32 actionIdx, u32 buttonIdx);

//Raw buttons of player 1's controller as last read, before any remapping (for the "press a button" prompt)
u16 GetPhysicalButtons();
ControllerType GetPhysicalType();

}//namespace ButtonRemap
}//namespace Pulsar

#endif
