/*
 * Per-license button remapping for VanzaKart
 * Copyright (c) 2026 ErSossiega
 *
 * SPDX-License-Identifier: MIT
 * Released under the MIT License (https://opensource.org/licenses/MIT): this file can be
 * used, modified and redistributed freely, as long as this copyright notice and the
 * license are kept in every copy or substantial portion of it.
 */

#include <kamek.hpp>
#include <core/rvl/pad.hpp>
#include <core/rvl/wpad.hpp>
#include <MarioKartWii/Input/Controller.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <MarioKartWii/Race/Racedata.hpp>
#include <Settings/Settings.hpp>
#include <Race/ButtonRemap.hpp>

namespace Pulsar {
namespace ButtonRemap {

enum ActionNameId {
    NAME_ACCELERATE,
    NAME_BRAKE,
    NAME_DRIFT,
    NAME_BRAKEDRIFT,
    NAME_ITEM,
    NAME_LOOKBACK,
    NAME_TRICK,
    NAME_COUNT
};

//State::buttonActions
enum ActionBits {
    BIT_ACCELERATE = 0x1,
    BIT_BRAKE = 0x2,
    BIT_ITEM = 0x4,
    BIT_DRIFT = 0x8,
    BIT_LOOKBACK = 0x20
};

//State::motionControlFlick
enum Flick {
    FLICK_NONE,
    FLICK_UP,
    FLICK_DOWN,
    FLICK_LEFT,
    FLICK_RIGHT
};

//Half pressing a GCN trigger counts as pressing it, like the game does for items and drifts
static const u8 analogTriggerThreshold = 100;

static const wchar_t* actionNames[NAME_COUNT][2] = {
    { L"Accelerate", L"Accelera" },
    { L"Brake", L"Freno" },
    { L"Drift", L"Derapata" },
    { L"Brake / Drift", L"Freno / Derapata" },
    { L"Use item", L"Oggetto" },
    { L"Look behind", L"Guarda dietro" },
    { L"Trick", L"Trick" }
};

static const ButtonDef gcnButtons[] = {
    { PAD::PAD_BUTTON_A, L"A" },
    { PAD::PAD_BUTTON_B, L"B" },
    { PAD::PAD_BUTTON_X, L"X" },
    { PAD::PAD_BUTTON_Y, L"Y" },
    { PAD::PAD_BUTTON_Z, L"Z" },
    { PAD::PAD_BUTTON_L, L"L" },
    { PAD::PAD_BUTTON_R, L"R" },
    { PAD::PAD_BUTTON_UP, L"Pad Up", L"Croce Su" },
    { PAD::PAD_BUTTON_DOWN, L"Pad Down", L"Croce Gi\x00F9" },
    { PAD::PAD_BUTTON_LEFT, L"Pad Left", L"Croce Sx" },
    { PAD::PAD_BUTTON_RIGHT, L"Pad Right", L"Croce Dx" }
};
static const u16 gcnDpad = PAD::PAD_BUTTON_UP | PAD::PAD_BUTTON_DOWN | PAD::PAD_BUTTON_LEFT | PAD::PAD_BUTTON_RIGHT;
static const ActionDef gcnActions[] = {
    { NAME_ACCELERATE, PAD::PAD_BUTTON_A, PAD::PAD_BUTTON_A, BIT_ACCELERATE },
    { NAME_BRAKE, PAD::PAD_BUTTON_B, PAD::PAD_BUTTON_B, BIT_BRAKE },
    { NAME_DRIFT, PAD::PAD_BUTTON_R, PAD::PAD_BUTTON_R, BIT_DRIFT },
    { NAME_ITEM, PAD::PAD_BUTTON_L, PAD::PAD_BUTTON_L, BIT_ITEM },
    { NAME_LOOKBACK, PAD::PAD_BUTTON_X | PAD::PAD_BUTTON_Z, PAD::PAD_BUTTON_X, BIT_LOOKBACK },
    { NAME_TRICK, gcnDpad, PAD::PAD_BUTTON_UP, 0 }
};

static const ButtonDef classicButtons[] = {
    { WPAD::WPAD_CL_BUTTON_A, L"A" },
    { WPAD::WPAD_CL_BUTTON_B, L"B" },
    { WPAD::WPAD_CL_BUTTON_X, L"X" },
    { WPAD::WPAD_CL_BUTTON_Y, L"Y" },
    { WPAD::WPAD_CL_TRIGGER_L, L"L" },
    { WPAD::WPAD_CL_TRIGGER_R, L"R" },
    { WPAD::WPAD_CL_TRIGGER_ZL, L"ZL" },
    { WPAD::WPAD_CL_TRIGGER_ZR, L"ZR" },
    { WPAD::WPAD_CL_BUTTON_MINUS, L"-" },
    { WPAD::WPAD_CL_BUTTON_UP, L"Pad Up", L"Croce Su" },
    { WPAD::WPAD_CL_BUTTON_DOWN, L"Pad Down", L"Croce Gi\x00F9" },
    { WPAD::WPAD_CL_BUTTON_LEFT, L"Pad Left", L"Croce Sx" },
    { WPAD::WPAD_CL_BUTTON_RIGHT, L"Pad Right", L"Croce Dx" }
};
static const u16 classicDpad = WPAD::WPAD_CL_BUTTON_UP | WPAD::WPAD_CL_BUTTON_DOWN | WPAD::WPAD_CL_BUTTON_LEFT | WPAD::WPAD_CL_BUTTON_RIGHT;
static const ActionDef classicActions[] = {
    { NAME_ACCELERATE, WPAD::WPAD_CL_BUTTON_A, WPAD::WPAD_CL_BUTTON_A, BIT_ACCELERATE },
    { NAME_BRAKE, WPAD::WPAD_CL_BUTTON_B, WPAD::WPAD_CL_BUTTON_B, BIT_BRAKE },
    { NAME_DRIFT, WPAD::WPAD_CL_TRIGGER_R, WPAD::WPAD_CL_TRIGGER_R, BIT_DRIFT },
    { NAME_ITEM, WPAD::WPAD_CL_TRIGGER_L, WPAD::WPAD_CL_TRIGGER_L, BIT_ITEM },
    { NAME_LOOKBACK, WPAD::WPAD_CL_BUTTON_X | WPAD::WPAD_CL_TRIGGER_ZR, WPAD::WPAD_CL_BUTTON_X, BIT_LOOKBACK },
    { NAME_TRICK, classicDpad, WPAD::WPAD_CL_BUTTON_UP, 0 }
};

static const ButtonDef nunchuckButtons[] = {
    { WPAD::WPAD_BUTTON_A, L"A" },
    { WPAD::WPAD_BUTTON_B, L"B" },
    { WPAD::WPAD_BUTTON_1, L"1" },
    { WPAD::WPAD_BUTTON_2, L"2" },
    { WPAD::WPAD_BUTTON_MINUS, L"-" },
    { WPAD::WPAD_BUTTON_C, L"C" },
    { WPAD::WPAD_BUTTON_Z, L"Z" },
    { WPAD::WPAD_BUTTON_UP, L"Pad Up", L"Croce Su" },
    { WPAD::WPAD_BUTTON_DOWN, L"Pad Down", L"Croce Gi\x00F9" },
    { WPAD::WPAD_BUTTON_LEFT, L"Pad Left", L"Croce Sx" },
    { WPAD::WPAD_BUTTON_RIGHT, L"Pad Right", L"Croce Dx" }
};
//Tricks are done by shaking, so there is no trick action on the Wii Remote
static const ActionDef nunchuckActions[] = {
    { NAME_ACCELERATE, WPAD::WPAD_BUTTON_A, WPAD::WPAD_BUTTON_A, BIT_ACCELERATE },
    { NAME_BRAKEDRIFT, WPAD::WPAD_BUTTON_B, WPAD::WPAD_BUTTON_B, BIT_BRAKE | BIT_DRIFT },
    { NAME_ITEM, WPAD::WPAD_BUTTON_Z, WPAD::WPAD_BUTTON_Z, BIT_ITEM },
    { NAME_LOOKBACK, WPAD::WPAD_BUTTON_C, WPAD::WPAD_BUTTON_C, BIT_LOOKBACK }
};

//The remote is held sideways, so the D-Pad names follow how the player sees it, not the raw bits
static const ButtonDef wheelButtons[] = {
    { WPAD::WPAD_BUTTON_A, L"A" },
    { WPAD::WPAD_BUTTON_B, L"B" },
    { WPAD::WPAD_BUTTON_1, L"1" },
    { WPAD::WPAD_BUTTON_2, L"2" },
    { WPAD::WPAD_BUTTON_MINUS, L"-" },
    { WPAD::WPAD_BUTTON_RIGHT, L"Pad Up", L"Croce Su" },
    { WPAD::WPAD_BUTTON_LEFT, L"Pad Down", L"Croce Gi\x00F9" },
    { WPAD::WPAD_BUTTON_UP, L"Pad Left", L"Croce Sx" },
    { WPAD::WPAD_BUTTON_DOWN, L"Pad Right", L"Croce Dx" }
};
static const u16 wiimoteDpad = WPAD::WPAD_BUTTON_UP | WPAD::WPAD_BUTTON_DOWN | WPAD::WPAD_BUTTON_LEFT | WPAD::WPAD_BUTTON_RIGHT;
static const ActionDef wheelActions[] = {
    { NAME_ACCELERATE, WPAD::WPAD_BUTTON_2, WPAD::WPAD_BUTTON_2, BIT_ACCELERATE },
    { NAME_BRAKE, WPAD::WPAD_BUTTON_1, WPAD::WPAD_BUTTON_1, BIT_BRAKE },
    { NAME_DRIFT, WPAD::WPAD_BUTTON_B, WPAD::WPAD_BUTTON_B, BIT_DRIFT },
    { NAME_ITEM, wiimoteDpad, WPAD::WPAD_BUTTON_RIGHT, BIT_ITEM },
    { NAME_LOOKBACK, WPAD::WPAD_BUTTON_A, WPAD::WPAD_BUTTON_A, BIT_LOOKBACK }
};

#define PUL_ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
static const ControllerDef controllerDefs[typeCount] = {
    { wheelActions, PUL_ARRAY_COUNT(wheelActions), wheelButtons, PUL_ARRAY_COUNT(wheelButtons), wiimoteDpad }, //WHEEL
    { nunchuckActions, PUL_ARRAY_COUNT(nunchuckActions), nunchuckButtons, PUL_ARRAY_COUNT(nunchuckButtons), wiimoteDpad }, //NUNCHUCK
    { classicActions, PUL_ARRAY_COUNT(classicActions), classicButtons, PUL_ARRAY_COUNT(classicButtons), classicDpad }, //CLASSIC
    { gcnActions, PUL_ARRAY_COUNT(gcnActions), gcnButtons, PUL_ARRAY_COUNT(gcnButtons), gcnDpad } //GCN
};
#undef PUL_ARRAY_COUNT

bool IsItalian() {
    return Settings::Mgr::Get().GetUserSettingValue(static_cast<Settings::UserType>(Settings::SETTINGSTYPE_LANGUAGE),
        SCROLLER_LANGUAGE) == LANGUAGE_ITALIAN;
}

bool IsRemappable(ControllerType type) {
    return type >= WHEEL && type <= GCN;
}

const ControllerDef& GetControllerDef(ControllerType type) {
    if(!IsRemappable(type)) type = GCN;
    return controllerDefs[type];
}

const wchar_t* GetActionName(const ActionDef& action) {
    return actionNames[action.nameId][IsItalian() ? 1 : 0];
}

const wchar_t* GetButtonName(const ButtonDef& button) {
    if(button.nameItalian != nullptr && IsItalian()) return button.nameItalian;
    return button.name;
}

u32 GetCurrentLicense() {
    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    if(rksys == nullptr || rksys->curLicenseId >= licenseCount) return noLicense;
    return rksys->curLicenseId;
}

u32 GetProfile(u32 license, ControllerType type) {
    if(license >= licenseCount || !IsRemappable(type)) return 0;
    return Settings::Mgr::GetButtonRemap(license, type);
}

static inline u32 SetAssignment(u32 profile, u32 actionIdx, u32 value) {
    const u32 shift = actionIdx * 4;
    return (profile & ~(0xF << shift)) | ((value & 0xF) << shift);
}

static inline u16 GetAssignedBit(u32 profile, const ControllerDef& def, u32 actionIdx) {
    const u32 assignment = GetAssignment(profile, actionIdx);
    if(assignment == 0 || assignment > def.buttonCount) return 0;
    return def.buttons[assignment - 1].bit;
}

u16 GetActionButtons(u32 profile, const ControllerDef& def, u32 actionIdx) {
    const u16 assigned = GetAssignedBit(profile, def, actionIdx);
    if(assigned != 0) return assigned;
    u16 taken = 0;
    for(u32 i = 0; i < def.actionCount; ++i) if(i != actionIdx) taken |= GetAssignedBit(profile, def, i);
    return def.actions[actionIdx].defaultMask & ~taken;
}

static u32 GetButtonIdx(const ControllerDef& def, u16 bit) {
    for(u32 i = 0; i < def.buttonCount; ++i) if(def.buttons[i].bit == bit) return i;
    return def.buttonCount;
}

static u32 Normalize(const ControllerDef& def, u32 actionIdx, u32 buttonIdx) {
    //One of the action's own default buttons: store 0 so it gets its whole default group back
    if(def.actions[actionIdx].defaultMask & def.buttons[buttonIdx].bit) return 0;
    return buttonIdx + 1;
}

u32 AssignButton(u32 profile, const ControllerDef& def, u32 actionIdx, u32 buttonIdx) {
    if(actionIdx >= def.actionCount || buttonIdx >= def.buttonCount) return profile;
    const u16 newBit = def.buttons[buttonIdx].bit;
    const u16 oldButtons = GetActionButtons(profile, def, actionIdx) & ~newBit;
    profile = SetAssignment(profile, actionIdx, Normalize(def, actionIdx, buttonIdx));

    for(u32 i = 0; i < def.actionCount; ++i) {
        if(i == actionIdx) continue;
        //Another action had that exact button, or it was all it had left: swap, it takes the first button actionIdx just left
        if(GetAssignedBit(profile, def, i) != newBit && GetActionButtons(profile, def, i) != 0) continue;
        u32 value = 0;
        for(u32 b = 0; b < def.buttonCount; ++b) {
            if((oldButtons & def.buttons[b].bit) == 0) continue;
            value = Normalize(def, i, b);
            break;
        }
        profile = SetAssignment(profile, i, value);
    }
    return profile;
}

//Race input remapping
static u16 physicalButtons = 0;
static ControllerType physicalType = CONTROLLER_TYPE_NONE;
static u16 prevVirtualButtons = 0;

u16 GetPhysicalButtons() { return physicalButtons; }
ControllerType GetPhysicalType() { return physicalType; }

static u8 GetDpadFlick(ControllerType type, u16 presses) {
    u16 up = PAD::PAD_BUTTON_UP, down = PAD::PAD_BUTTON_DOWN, left = PAD::PAD_BUTTON_LEFT, right = PAD::PAD_BUTTON_RIGHT;
    if(type == CLASSIC) {
        up = WPAD::WPAD_CL_BUTTON_UP;
        down = WPAD::WPAD_CL_BUTTON_DOWN;
        left = WPAD::WPAD_CL_BUTTON_LEFT;
        right = WPAD::WPAD_CL_BUTTON_RIGHT;
    }
    if(presses & up) return FLICK_UP;
    if(presses & down) return FLICK_DOWN;
    if(presses & left) return FLICK_LEFT;
    if(presses & right) return FLICK_RIGHT;
    return FLICK_NONE;
}

//A trick moved off the D-Pad has a single button, so the direction comes from the stick (same rule as mkw-sp)
static u8 GetStickFlick(const Input::State& state) {
    const s32 x = static_cast<s32>(state.quantisedStickX) - 7;
    const s32 y = static_cast<s32>(state.quantisedStickY) - 7;
    const s32 absX = x < 0 ? -x : x;
    const s32 absY = y < 0 ? -y : y;
    if(absX <= 2 * absY) return y < 0 ? FLICK_DOWN : FLICK_UP;
    return x < 0 ? FLICK_LEFT : FLICK_RIGHT;
}

static void SetFlick(Input::State& state, u8 flick) {
    state.motionControlFlickUnmirrored = flick;
    const Racedata* racedata = Racedata::sInstance;
    const bool isMirror = racedata != nullptr && (racedata->racesScenario.settings.modeFlags & 1) != 0;
    if(isMirror && flick == FLICK_LEFT) flick = FLICK_RIGHT;
    else if(isMirror && flick == FLICK_RIGHT) flick = FLICK_LEFT;
    state.motionControlFlick = flick;
}

static void Apply(const Input::Controller& controller, Input::State& state, u16 analogButtons) {
    const SectionMgr* sectionMgr = SectionMgr::sInstance;
    if(sectionMgr == nullptr) return;
    const Input::RealControllerHolder* holder = sectionMgr->pad.padInfos[0].controllerHolder;
    if(holder == nullptr || holder->curController != &controller) return; //only player 1 has a license

    const ControllerType type = controller.GetType();
    const u16 raw = state.buttonRaw | analogButtons;
    physicalButtons = raw;
    physicalType = type;

    const u32 profile = GetProfile(GetCurrentLicense(), type);
    if(profile == 0) {
        prevVirtualButtons = state.buttonRaw;
        return;
    }

    const ControllerDef& def = controllerDefs[type];
    u16 assigned[maxActions];
    u16 taken = 0;
    u16 freed = 0;
    for(u32 i = 0; i < def.actionCount; ++i) {
        assigned[i] = GetAssignedBit(profile, def, i);
        if(assigned[i] != 0) {
            taken |= assigned[i];
            freed |= def.actions[i].defaultMask;
        }
    }

    //Rebuild the raw buttons as if the player had pressed the default ones: whatever reads buttonRaw
    //afterwards (200cc brake drifting, OTT toggles...) follows the remap too
    u16 virtualButtons = raw & ~(taken | freed);
    for(u32 i = 0; i < def.actionCount; ++i) {
        if(assigned[i] != 0 && (raw & assigned[i]) != 0) virtualButtons |= def.actions[i].primary;
    }

    const u16 touched = taken | freed;
    for(u32 i = 0; i < def.actionCount; ++i) {
        const ActionDef& action = def.actions[i];
        if(assigned[i] == 0 && (action.defaultMask & touched) == 0) continue; //untouched, keep what the game computed (analog triggers included)

        if(action.actionBits != 0) {
            state.buttonActions &= ~action.actionBits;
            if(virtualButtons & action.defaultMask) state.buttonActions |= action.actionBits;
        }
        else {
            //Trick: the game only sets the flick on the first frame of a D-Pad press
            const u16 presses = virtualButtons & ~prevVirtualButtons & action.defaultMask;
            u8 flick = FLICK_NONE;
            if(presses != 0) flick = assigned[i] != 0 ? GetStickFlick(state) : GetDpadFlick(type, presses);
            SetFlick(state, flick);
        }
    }
    state.buttonRaw = virtualButtons;
    prevVirtualButtons = virtualButtons;
}

static void GCNUpdateImpl(Input::GCNController* controller, Input::State& state, Input::UIState& uiState) {
    controller->Input::GCNController::UpdateImpl(state, uiState);
    u16 analogButtons = 0;
    if(controller->padStatus.triggerL >= analogTriggerThreshold) analogButtons |= PAD::PAD_BUTTON_L;
    if(controller->padStatus.triggerR >= analogTriggerThreshold) analogButtons |= PAD::PAD_BUTTON_R;
    Apply(*controller, state, analogButtons);
}
kmWritePointer(0x808b2e54, GCNUpdateImpl); //GCNController vtable, UpdateImpl

static void WiiUpdateImpl(Input::WiiController* controller, Input::State& state, Input::UIState& uiState) {
    controller->Input::WiiController::UpdateImpl(state, uiState);
    Apply(*controller, state, 0);
}
kmWritePointer(0x808b2e9c, WiiUpdateImpl); //WiiController vtable, UpdateImpl

}//namespace ButtonRemap
}//namespace Pulsar
