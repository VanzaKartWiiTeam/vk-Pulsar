/*
 * Per-license button remapping for VanzaKart
 * Copyright (c) 2026 ErSossiega
 *
 * SPDX-License-Identifier: MIT
 * Released under the MIT License (https://opensource.org/licenses/MIT): this file can be
 * used, modified and redistributed freely, as long as this copyright notice and the
 * license are kept in every copy or substantial portion of it.
 */

#include <UI/ButtonRemap/ButtonRemapPage.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/UI/Page/Menu/VSSettings.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuText.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <core/nw4r/lyt/Pane.hpp>

namespace Pulsar {
namespace UI {

static const u32 textCapacity = 96;
static const u32 captureTimeout = 60 * 5; //frames
static const char* textPanes[2] = { "text", "text_light_01" };
static const float rowFitChars = 14.0f; //what a SettingsPageSelect button holds at the layout's text size

static void AppendText(wchar_t* dest, const wchar_t* src) {
    u32 len = 0;
    while(dest[len] != L'\0') ++len;
    while(*src != L'\0' && len < textCapacity - 1) dest[len++] = *src++;
    dest[len] = L'\0';
}

static const wchar_t* Localize(const wchar_t* english, const wchar_t* italian) {
    return ButtonRemap::IsItalian() ? italian : english;
}

static const wchar_t* GetControllerName(ControllerType type) {
    switch(type) {
        case WHEEL: return Localize(L"Wii Remote", L"Wii Remote");
        case NUNCHUCK: return Localize(L"Wii Remote + Nunchuk", L"Wii Remote + Nunchuk");
        case CLASSIC: return Localize(L"Classic Controller", L"Controller Classic");
        default: return Localize(L"GameCube Controller", L"Controller GameCube");
    }
}

static ControllerType GetPlayerControllerType() {
    const Input::RealControllerHolder* holder = SectionMgr::sInstance->pad.padInfos[0].controllerHolder;
    if(holder == nullptr || holder->curController == nullptr) return CONTROLLER_TYPE_NONE;
    return holder->curController->GetType();
}

ButtonRemapPage::ButtonRemapPage() {
    externControlCount = 0;
    internControlCount = rowCount;
    hasBackButton = true;
    nextPageId = PAGE_NONE;
    prevPageId = static_cast<PageId>(SettingsPageSelect::id);
    titleBmg = BMG_TEXT;
    activePlayerBitfield = 1;
    movieStartFrame = -1;
    extraControlNumber = 0;
    isLocked = false;
    controlCount = 0;
    nextSection = SECTION_NONE;
    controlSources = 2;

    type = GCN;
    license = ButtonRemap::noLicense;
    profile = 0;
    state = STATE_IDLE;
    captureAction = 0;
    stateFrames = 0;
    prevButtons = 0;
    for(u32 i = 0; i < rowCount; ++i) {
        textScaleX[i] = 1.0f;
        textScaleY[i] = 1.0f;
    }

    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &ButtonRemapPage::OnButtonClick;
    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &ButtonRemapPage::OnButtonSelect;
    onButtonDeselectHandler.subject = this;
    onButtonDeselectHandler.ptmf = &ButtonRemapPage::OnButtonDeselect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &ButtonRemapPage::OnBackPress;
    onBackButtonClickHandler.subject = this;
    onBackButtonClickHandler.ptmf = &ButtonRemapPage::OnBackButtonClick;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

void ButtonRemapPage::OnInit() {
    MenuInteractable::OnInit();
    this->SetTransitionSound(0, 0);
    this->backButton.SetOnClickHandler(this->onBackButtonClickHandler, 0);
}

//Reuses the SettingsPageSelect buttons: Page0 to Page6 already exist in its brctr
UIControl* ButtonRemapPage::CreateControl(u32 id) {
    if(id >= rowCount) return nullptr;
    PushButton& button = this->rows[id];
    this->AddControl(this->controlCount++, button, 0);

    char variant[16];
    snprintf(variant, 16, "Page%d", id);
    button.Load(UI::buttonFolder, "SettingsPageSelect", variant, this->activePlayerBitfield, 0, false);
    button.buttonId = id;
    this->SetButtonHandlers(button);

    const lyt::Pane* textPane = button.layout.GetPaneByName(textPanes[0]);
    this->textScaleX[id] = textPane != nullptr ? textPane->scale.x : 1.0f;
    this->textScaleY[id] = textPane != nullptr ? textPane->scale.z : 1.0f;
    return &button;
}

void ButtonRemapPage::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void ButtonRemapPage::OnActivate() {
    this->state = STATE_IDLE;
    this->controlsManipulatorManager.inaccessible = false;

    this->type = GetPlayerControllerType();
    if(!ButtonRemap::IsRemappable(this->type)) this->type = GCN;
    this->license = ButtonRemap::GetCurrentLicense();
    this->profile = ButtonRemap::GetProfile(this->license, this->type);

    this->Refresh();
    this->rows[0].Select(0);
    MenuInteractable::OnActivate();
    this->SetTitle();
    this->SetBottomText(0);
}

void ButtonRemapPage::OnDeactivate() {
    MenuInteractable::OnDeactivate();
    this->controlsManipulatorManager.inaccessible = false;
    this->state = STATE_IDLE;
}

const ut::detail::RuntimeTypeInfo* ButtonRemapPage::GetRuntimeTypeInfo() const {
    return Pages::VSSettings::typeInfo;
}

void ButtonRemapPage::SetTitle() {
    if(this->titleText == nullptr) return;
    wchar_t title[textCapacity];
    title[0] = L'\0';
    AppendText(title, Localize(L"Controls - ", L"Comandi - "));
    AppendText(title, GetControllerName(this->type));
    Text::Info info;
    info.strings[0] = title;
    this->titleText->SetMessage(BMG_TEXT, &info);
}

void ButtonRemapPage::Refresh() {
    for(u32 row = 0; row < rowCount; ++row) this->RefreshRow(row);
}

void ButtonRemapPage::RefreshRow(u32 row) {
    const ButtonRemap::ControllerDef& def = ButtonRemap::GetControllerDef(this->type);
    PushButton& button = this->rows[row];
    const bool isHidden = row > def.actionCount;
    button.isHidden = isHidden;
    button.manipulator.inaccessible = isHidden;
    if(isHidden) return;

    wchar_t text[textCapacity];
    text[0] = L'\0';
    if(row == def.actionCount) AppendText(text, Localize(L"Reset to default", L"Ripristina predefiniti"));
    else {
        AppendText(text, ButtonRemap::GetActionName(def.actions[row]));
        AppendText(text, L": ");
        const bool isWaiting = this->state == STATE_WAIT_RELEASE || this->state == STATE_CAPTURE;
        if(isWaiting && this->captureAction == row) AppendText(text, L"...");
        else {
            u16 buttons = ButtonRemap::GetActionButtons(this->profile, def, row);
            bool isFirst = true;
            if((buttons & def.dpadMask) == def.dpadMask) {
                AppendText(text, Localize(L"D-Pad", L"Croce"));
                buttons &= ~def.dpadMask;
                isFirst = false;
            }
            for(u32 i = 0; i < def.buttonCount; ++i) {
                if((buttons & def.buttons[i].bit) == 0) continue;
                if(!isFirst) AppendText(text, L" / ");
                AppendText(text, ButtonRemap::GetButtonName(def.buttons[i]));
                isFirst = false;
            }
            if(isFirst) AppendText(text, L"-");
        }
    }
    Text::Info info;
    info.strings[0] = text;
    button.SetMessage(BMG_TEXT, &info);

    //The buttons are sized for short page names: shrink the text of the longer rows so it stays inside
    u32 len = 0;
    while(text[len] != L'\0') ++len;
    const float factor = len > rowFitChars ? rowFitChars / static_cast<float>(len) : 1.0f;
    for(u32 i = 0; i < 2; ++i) {
        lyt::Pane* pane = button.layout.GetPaneByName(textPanes[i]);
        if(pane == nullptr) continue;
        pane->scale.x = this->textScaleX[row] * factor;
        pane->scale.z = this->textScaleY[row] * factor;
    }
}

void ButtonRemapPage::SetBottomText(u32 row) {
    if(this->bottomText == nullptr) return;
    const ButtonRemap::ControllerDef& def = ButtonRemap::GetControllerDef(this->type);
    const wchar_t* message;
    if(this->state == STATE_WAIT_RELEASE || this->state == STATE_CAPTURE) {
        message = Localize(L"Press the button to assign. START/+ to cancel.",
            L"Premi il tasto da assegnare. START/+ per annullare.");
    }
    else if(this->license == ButtonRemap::noLicense) {
        message = Localize(L"Load a license to change the controls.", L"Carica una licenza per cambiare i comandi.");
    }
    else if(row == def.actionCount) {
        message = Localize(L"Restore the original controls of this controller.",
            L"Ripristina i comandi originali di questo controller.");
    }
    else {
        message = Localize(L"Select an action to change its button. Menus keep the original buttons.",
            L"Scegli un'azione per cambiarne il tasto. I menu usano sempre i tasti originali.");
    }
    wchar_t text[textCapacity];
    text[0] = L'\0';
    AppendText(text, message);
    Text::Info info;
    info.strings[0] = text;
    this->bottomText->SetMessage(BMG_TEXT, &info);
}

void ButtonRemapPage::OnButtonClick(PushButton& button, u32 hudSlotId) {
    if(this->state != STATE_IDLE || this->license == ButtonRemap::noLicense) return;
    const ButtonRemap::ControllerDef& def = ButtonRemap::GetControllerDef(this->type);
    const u32 row = button.buttonId;
    if(row < def.actionCount) this->StartCapture(row);
    else if(row == def.actionCount && this->profile != 0) {
        this->profile = 0;
        Settings::Mgr::SetButtonRemap(this->license, this->type, 0);
        Settings::Mgr::SaveButtonRemap();
        this->Refresh();
    }
}

void ButtonRemapPage::OnButtonSelect(PushButton& button, u32 hudSlotId) {
    this->SetBottomText(button.buttonId);
}

void ButtonRemapPage::OnBackPress(u32 hudSlotId) {
    if(this->state != STATE_IDLE) return;
    this->backButton.SelectFocus();
    this->nextPageId = static_cast<PageId>(SettingsPageSelect::id);
    this->EndStateAnimated(0, this->backButton.GetAnimationFrameSize());
}

void ButtonRemapPage::OnBackButtonClick(PushButton& button, u32 hudSlotId) {
    this->OnBackPress(hudSlotId);
}

u16 ButtonRemapPage::GetAssignableMask() const {
    const ButtonRemap::ControllerDef& def = ButtonRemap::GetControllerDef(this->type);
    u16 mask = 0;
    for(u32 i = 0; i < def.buttonCount; ++i) mask |= def.buttons[i].bit;
    return mask;
}

void ButtonRemapPage::SetMenuLocked(bool locked) {
    this->controlsManipulatorManager.inaccessible = locked;
}

void ButtonRemapPage::StartCapture(u32 actionIdx) {
    this->state = STATE_WAIT_RELEASE;
    this->captureAction = actionIdx;
    this->stateFrames = 0;
    this->prevButtons = ButtonRemap::GetPhysicalButtons();
    this->SetMenuLocked(true);
    this->RefreshRow(actionIdx);
    this->SetBottomText(actionIdx);
}

void ButtonRemapPage::EndCapture() {
    const u32 row = this->captureAction;
    this->state = STATE_WAIT_RELEASE_DONE;
    this->stateFrames = 0;
    this->RefreshRow(row);
    this->SetBottomText(row);
}

void ButtonRemapPage::BeforeControlUpdate() {
    const ControllerType curType = GetPlayerControllerType();
    if(this->state == STATE_IDLE) {
        //The player picked up another controller: show that one's controls
        if(curType != this->type && ButtonRemap::IsRemappable(curType)) {
            this->type = curType;
            this->profile = ButtonRemap::GetProfile(this->license, this->type);
            this->Refresh();
            this->SetTitle();
            this->rows[0].Select(0);
        }
        return;
    }

    //The player switched controller mid-prompt: it no longer matches what they hold
    const u16 buttons = ButtonRemap::GetPhysicalType() == this->type ? ButtonRemap::GetPhysicalButtons() : 0;
    const u16 assignable = this->GetAssignableMask();
    ++this->stateFrames;

    switch(this->state) {
        case STATE_WAIT_RELEASE:
            if(curType != this->type || this->stateFrames > captureTimeout) this->EndCapture();
            else if((buttons & assignable) == 0) {
                this->state = STATE_CAPTURE;
                this->stateFrames = 0;
            }
            break;
        case STATE_CAPTURE: {
            const u16 presses = buttons & ~this->prevButtons;
            if(curType != this->type || this->stateFrames > captureTimeout || (presses & ~assignable) != 0) {
                this->EndCapture(); //START/+ (or any button that can't be assigned) cancels
                break;
            }
            const ButtonRemap::ControllerDef& def = ButtonRemap::GetControllerDef(this->type);
            for(u32 i = 0; i < def.buttonCount; ++i) {
                if((presses & def.buttons[i].bit) == 0) continue;
                this->profile = ButtonRemap::AssignButton(this->profile, def, this->captureAction, i);
                Settings::Mgr::SetButtonRemap(this->license, this->type, this->profile);
                Settings::Mgr::SaveButtonRemap();
                this->EndCapture();
                this->Refresh(); //a swap can change another row too
                break;
            }
            break;
        }
        case STATE_WAIT_RELEASE_DONE:
            if((buttons & assignable) == 0 || this->stateFrames > captureTimeout) {
                this->state = STATE_IDLE;
                this->SetMenuLocked(false);
                this->RefreshRow(this->captureAction);
                this->SetBottomText(this->captureAction);
            }
            break;
        default:
            break;
    }
    this->prevButtons = buttons;
}

}//namespace UI
}//namespace Pulsar
