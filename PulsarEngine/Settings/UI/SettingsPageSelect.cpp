#include <Settings/UI/SettingsPageSelect.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <UI/ButtonRemap/ButtonRemapPage.hpp>
#include <Race/ButtonRemap.hpp>
#include <Settings/Settings.hpp>
#include <MarioKartWii/UI/Page/Menu/VSSettings.hpp>

namespace Pulsar {
namespace UI {

SettingsPageSelect::SettingsPageSelect() {
    externControlCount = 0;
    internControlCount = Settings::Params::pageCount + 1;
    hasBackButton = true;
    nextPageId = PAGE_NONE;
    titleBmg = BMG_SETTINGS_TITLE;
    activePlayerBitfield = 1;
    movieStartFrame = -1;
    extraControlNumber = 0;
    isLocked = false;
    controlCount = 0;
    nextSection = SECTION_NONE;
    controlSources = 2;

    SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    if(sectionId == SECTION_OPTIONS)
        prevPageId = PAGE_OPTIONS;
    else if((sectionId == SECTION_P1_WIFI) || (sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE) ||
            (sectionId == SECTION_P1_WIFI_FROM_FIND_FRIEND) || (sectionId == SECTION_P2_WIFI) ||
            (sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE))
        prevPageId = PAGE_WFC_MAIN;
    else if(sectionId >= SECTION_LICENSE_SETTINGS_MENU && sectionId <= SECTION_SINGLE_P_LIST_RACE_GHOST)
        prevPageId = PAGE_SINGLE_PLAYER_MENU;
    else
        prevPageId = PAGE_FRIEND_ROOM;

    onButtonClickHandler.subject = this;
    onButtonClickHandler.ptmf = &SettingsPageSelect::OnButtonClick;
    onButtonSelectHandler.subject = this;
    onButtonSelectHandler.ptmf = &SettingsPageSelect::OnButtonSelect;
    onButtonDeselectHandler.subject = this;
    onButtonDeselectHandler.ptmf = &SettingsPageSelect::OnButtonDeselect;
    onBackPressHandler.subject = this;
    onBackPressHandler.ptmf = &SettingsPageSelect::OnBackPress;
    onBackButtonClickHandler.subject = this;
    onBackButtonClickHandler.ptmf = &SettingsPageSelect::OnBackButtonClick;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

void SettingsPageSelect::OnInit() {
    MenuInteractable::OnInit();
    this->SetTransitionSound(0, 0);
    this->backButton.SetOnClickHandler(this->onBackButtonClickHandler, 0);
}

UIControl* SettingsPageSelect::CreateControl(u32 id) {
    if(id < Settings::Params::pageCount) {
        PushButton& button = this->pageButtons[id];
        this->AddControl(this->controlCount++, button, 0);

        char variant[16];
        snprintf(variant, 16, "Page%d", id);

        button.Load(UI::buttonFolder, "SettingsPageSelect", variant, this->activePlayerBitfield, 0, false);
        button.buttonId = id;
        button.SetOnClickHandler(this->onButtonClickHandler, 0);
        button.SetOnSelectHandler(this->onButtonSelectHandler);
        button.SetOnDeselectHandler(this->onButtonDeselectHandler);

        u32 bmgOffset = 0;
        u32 pageIdx = id;
        if(id >= Settings::Params::pulsarPageCount) {
            bmgOffset = BMG_USERSETTINGSOFFSET;
            pageIdx = id - Settings::Params::pulsarPageCount;
        }
        button.SetMessage(bmgOffset + BMG_SETTINGS_PAGE + pageIdx);

        return &button;
    }
    if(id == remapButtonId) {
        PushButton& button = this->pageButtons[id];
        this->AddControl(this->controlCount++, button, 0);

        char variant[16];
        snprintf(variant, 16, "Page%d", id);

        button.Load(UI::buttonFolder, "SettingsPageSelect", variant, this->activePlayerBitfield, 0, false);
        button.buttonId = id;
        button.SetOnClickHandler(this->onButtonClickHandler, 0);
        button.SetOnSelectHandler(this->onButtonSelectHandler);
        button.SetOnDeselectHandler(this->onButtonDeselectHandler);

        wchar_t name[16];
        const wchar_t* src = ButtonRemap::IsItalian() ? L"Comandi" : L"Controls";
        u32 len = 0;
        for(; src[len] != L'\0' && len < 15; ++len) name[len] = src[len];
        name[len] = L'\0';
        Text::Info info;
        info.strings[0] = name;
        button.SetMessage(BMG_TEXT, &info);

        return &button;
    }
    return nullptr;
}

void SettingsPageSelect::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void SettingsPageSelect::OnActivate() {
    if(Settings::Params::pageCount > 0) {
        this->pageButtons[0].Select(0);
    }

    this->bottomText->SetMessage(BMG_SETTINGS_BOTTOM);

    SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    bool isVotingSection = (sectionId >= SECTION_P1_WIFI_FROOM_VS_VOTING && sectionId <= SECTION_P2_WIFI_FROOM_COIN_VOTING) ||
                           (sectionId == SECTION_P1_WIFI_VS_VOTING) || (sectionId == SECTION_P1_WIFI_BATTLE_VOTING);
    bool isOnlineSection = (sectionId == SECTION_P1_WIFI || sectionId == SECTION_P2_WIFI ||
                            sectionId == SECTION_P1_WIFI_FROM_FROOM_RACE || sectionId == SECTION_P2_WIFI_FROM_FROOM_RACE ||
                            sectionId == SECTION_P1_WIFI_FROM_FIND_FRIEND || sectionId == SECTION_P2_WIFI_FROM_FIND_FRIEND);

    for(int i = 0; i < Settings::Params::pageCount; ++i) {
        bool isHidden = false;

        if(isVotingSection) {
            if(i == Settings::SETTINGSTYPE_KO ||
               i == Settings::SETTINGSTYPE_OTT ||
               i == (Settings::SETTINGSTYPE_LANGUAGE + Settings::Params::pulsarPageCount)) {
                isHidden = true;
            }
        }

        if(isOnlineSection) {
            if(i == (Settings::SETTINGSTYPE_LANGUAGE + Settings::Params::pulsarPageCount)) {
                isHidden = true;
            }
        }

        this->pageButtons[i].isHidden = isHidden;
        this->pageButtons[i].manipulator.inaccessible = isHidden;
    }

    MenuInteractable::OnActivate();
}

const ut::detail::RuntimeTypeInfo* SettingsPageSelect::GetRuntimeTypeInfo() const {
    return Pages::VSSettings::typeInfo;
}

int SettingsPageSelect::GetActivePlayerBitfield() const {
    return this->activePlayerBitfield;
}

int SettingsPageSelect::GetPlayerBitfield() const {
    return this->playerBitfield;
}

ManipulatorManager& SettingsPageSelect::GetManipulatorManager() {
    return this->controlsManipulatorManager;
}

void SettingsPageSelect::OnBackPress(u32 hudSlotId) {
    this->backButton.SelectFocus();
    this->LoadPrevPage(this->backButton);
}

void SettingsPageSelect::OnBackButtonClick(PushButton& button, u32 hudSlotId) {
    this->OnBackPress(hudSlotId);
}

void SettingsPageSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    const u32 selectedPage = button.buttonId;
    if(selectedPage == remapButtonId) {
        this->nextPageId = static_cast<PageId>(ButtonRemapPage::id);
        this->EndStateAnimated(0, button.GetAnimationFrameSize());
        return;
    }

    SettingsPanel* settingsPanel = ExpSection::GetSection()->GetPulPage<SettingsPanel>();
    if(settingsPanel != nullptr) {
        settingsPanel->sheetIdx = selectedPage;
        if(selectedPage < Settings::Params::pulsarPageCount) {
            settingsPanel->catIdx = selectedPage;
            settingsPanel->bmgOffset = 0;
        }
        else {
            settingsPanel->catIdx = selectedPage - Settings::Params::pulsarPageCount;
            settingsPanel->bmgOffset = BMG_USERSETTINGSOFFSET;
        }

        this->nextPageId = static_cast<PageId>(SettingsPanel::id);
        this->EndStateAnimated(0, button.GetAnimationFrameSize());
    }
}

void SettingsPageSelect::OnButtonSelect(PushButton& button, u32 hudSlotId) {
    if(button.buttonId == remapButtonId) {
        wchar_t text[64];
        const wchar_t* src = ButtonRemap::IsItalian() ? L"Cambia i tasti usati in gara." : L"Change the buttons used in races.";
        u32 len = 0;
        for(; src[len] != L'\0' && len < 63; ++len) text[len] = src[len];
        text[len] = L'\0';
        Text::Info info;
        info.strings[0] = text;
        this->bottomText->SetMessage(BMG_TEXT, &info);
        return;
    }
    u32 bmgOffset = 0;
    u32 pageIdx = button.buttonId;
    if(button.buttonId >= Settings::Params::pulsarPageCount) {
        bmgOffset = BMG_USERSETTINGSOFFSET;
        pageIdx = button.buttonId - Settings::Params::pulsarPageCount;
    }
    this->bottomText->SetMessage(bmgOffset + BMG_SETTINGS_TITLE + pageIdx);
}

void SettingsPageSelect::BeforeControlUpdate() {
}

}
}
