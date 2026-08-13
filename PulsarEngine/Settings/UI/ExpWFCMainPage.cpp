#include <MarioKartWii/UI/Page/Other/GlobeSearch.hpp>
#include <core/rvl/os/OS.hpp>
#include <MarioKartWii/RKSYS/RKSYSMgr.hpp>
#include <Settings/UI/ExpWFCMainPage.hpp>
#include <Settings/UI/SettingsPageSelect.hpp>
#include <UI/UI.hpp>
#include <UI/PlayerCount.hpp>
#include <Network/Rating/PlayerRating.hpp>
#include <Network/Rating/RankManager.hpp>

namespace Pulsar {
namespace Network {
    extern u32 REGIONID;
}
namespace UI {
//EXPANDED WFC, keeping WW button and just hiding it in case it is ever needed...

kmWrite32(0x8064b984, 0x60000000); //nop the InitControl call in the init func
kmWrite24(0x80899a36, 'PUL'); //8064ba38
kmWrite24(0x80899a5B, 'PUL'); //8064ba90

kmWrite32(0x8064c284, 0x38800001); // change control group size parameter to 1
kmCall(0x8064c294, ExpWFCModeSel::InitButton); // call InitButton to set up vrButton

ExpWFCMain::ExpWFCMain() {
    this->onSettingsClick.subject = this;
    this->onSettingsClick.ptmf = &ExpWFCMain::OnSettingsButtonClick;
    this->onButtonSelectHandler.ptmf = &ExpWFCMain::ExtOnButtonSelect;
}

void ExpWFCMain::OnInit() {
    PointRating::ResetRemotePrestigeRanks();
    Network::REGIONID = System::sInstance->GetInfo().GetWiimmfiRegion();
    this->InitControlGroup(8); //5 controls usually + settings button (5) + playerCount (6) + rankInfo (7)
    WFCMainMenu::OnInit();
    this->AddControl(5, settingsButton, 0);

    this->settingsButton.Load(UI::buttonFolder, "Settings1P", "Settings", 1, 0, false);
    this->settingsButton.buttonId = 5;
    this->settingsButton.SetOnClickHandler(this->onSettingsClick, 0);
    this->settingsButton.SetOnSelectHandler(this->onButtonSelectHandler);

    /*
        PlayerButton.brctr and RankButton.brctr already sit side by side, at x +60 and
        -60 of the shared VRButton layout. Forcing the player count to x 0 was only
        right while the rank button was hidden and it had to look centred on its own;
        now that both are shown, their own positions are what keeps them apart.
    */
    this->AddControl(6, playerCount, 0);
    ControlLoader loader(&this->playerCount);
    loader.Load(UI::buttonFolder, "PlayerButton", "VRButton", nullptr);
    this->playerCount.SetPosition(0.0f);

    this->AddControl(7, rankInfo, 0);
    ControlLoader rankLoader(&this->rankInfo);
    /*
        The layout name must stay "VRButton". UIAssets does ship a RankButton.brlyt, but
        every one of these brctr embeds "VRButton" as its own layout, so pointing the
        loader at RankButton.brlyt builds the control against panes it was not made for
        and GetPaneByName returns null -> DSI exception at null+0xE in BeforeControlUpdate.
    */
    rankLoader.Load(UI::buttonFolder, "RankButton", "VRButton", nullptr);
    this->rankInfo.SetPosition(0.0f);
    this->rankInfo.isHidden = false;

      this->topSettingsPage = SettingsPageSelect::id;  // Navigate to page selection first
}

void ExpWFCMain::BeforeControlUpdate() {
    WFCMainMenu::BeforeControlUpdate();

    int nTotal = 0;
    PlayerCount::GetNumbersTotal(nTotal);

    Text::Info info;
    info.intToPass[0] = nTotal;
    this->playerCount.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);

    /*
        The rank of the active licence. Rank::FormatLabel decides between the badge glyph
        and the plain digit, so this button and the name prefixes elsewhere always agree;
        see RATING_BADGE_USES_GLYPH for what the glyph form needs from the font. Unranked
        reads 0 instead of being blank, so the button never looks broken.

        Composed by hand rather than with swprintf: the label is one wide character and
        this toolchain's swprintf has no dependable way to splice one in.
    */
    const RKSYS::Mgr* rksys = RKSYS::Mgr::sInstance;
    const PointRating::RankId rank =
        (rksys != nullptr && rksys->curLicenseId < 4) ? PointRating::Rank::GetLocal(rksys->curLicenseId) : 0;

    static const wchar_t rankPrefix[] = L"Rank: ";
    wchar_t rankText[16];
    u32 len = 0;
    while (rankPrefix[len] != L'\0') {
        rankText[len] = rankPrefix[len];
        ++len;
    }
    len += PointRating::Rank::FormatLabel(rank, &rankText[len], 16 - len);
    rankText[len] = L'\0';

    // Guarded: writing to a pane the loaded layout does not have takes the whole page
    // down with a null dereference, and that is not worth risking for a label.
    if (this->rankInfo.layout.GetPaneByName("go") != nullptr) {
        Text::Info rankTextInfo;
        rankTextInfo.strings[0] = rankText;
        this->rankInfo.SetTextBoxMessage("go", UI::BMG_TEXT, &rankTextInfo);
        this->rankInfo.isHidden = false;
    } else {
        this->rankInfo.isHidden = true;
    }
}

void ExpWFCMain::OnSettingsButtonClick(PushButton& pushButton, u32 r5) {
    // Set up SettingsPageSelect's and SettingsPanel's previous page
    ExpSection::GetSection()->GetPulPage<SettingsPageSelect>()->prevPageId = PAGE_WFC_MAIN;
    ExpSection::GetSection()->GetPulPage<SettingsPanel>()->prevPageId = PAGE_WFC_MAIN;
    this->nextPageId = static_cast<PageId>(this->topSettingsPage);
    this->EndStateAnimated(0, pushButton.GetAnimationFrameSize());
}

void ExpWFCMain::ExtOnButtonSelect(PushButton& button, u32 hudSlotId) {
    if(button.buttonId == 5) {
        u32 bmgId = BMG_SETTINGS_BOTTOM + 1;
        if(this->topSettingsPage == PAGE_VS_TEAMS_VIEW) bmgId += 1;
        else if(this->topSettingsPage == PAGE_BATTLE_MODE_SELECT) bmgId += 2;
        this->bottomText.SetMessage(bmgId, 0);
    }
    else this->OnButtonSelect(button, hudSlotId);
}

//ExpWFCModeSel

ExpWFCModeSel::ExpWFCModeSel() : lastClickedButton(0), submenuState(STATE_MAIN) {
    this->onButtonSelectHandler.ptmf = &ExpWFCModeSel::OnModeButtonSelect;
    this->onModeButtonClickHandler.ptmf = &ExpWFCModeSel::OnModeButtonClick;
    this->onBackButtonClickHandler.ptmf = &ExpWFCModeSel::OnBackButtonClick;
    this->onBackPressHandler.ptmf = &ExpWFCModeSel::OnBackPress;
}

void ExpWFCModeSel::OnInit() {
    WFCModeSelect::OnInit();
}

void ExpWFCModeSel::InitButton(ExpWFCModeSel& self) {
    self.InitControlGroup(6); // 5 vanilla + vrButton
    
    self.AddControl(5, self.vrButton, 0);
    ControlLoader loader(&self.vrButton);
    loader.Load(UI::buttonFolder, "VRButton", "VRButton", nullptr);
    
    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    u32 vr = 0;
    u32 br = 5000;
    if (rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
        // The extended rating is stored divided by 100, so scale it back up to the
        // displayed VR the button expects.
        vr = static_cast<u32>(PointRating::GetUserVR(rksysMgr->curLicenseId) * 100.0f);
        br = license.br.points;
    }
    
    wchar_t buffer[64];
    swprintf(buffer, 64, L"%u VR", vr);
    Text::Info info;
    info.strings[0] = buffer;
    self.vrButton.SetTextBoxMessage("go", UI::BMG_TEXT, &info);
}

void ExpWFCModeSel::BeforeControlUpdate() {
    WFCModeSelect::BeforeControlUpdate();

    int nVS, n200cc, nOTT, nIR, nBattle;
    PlayerCount::GetNumbers(nVS, n200cc, nOTT, nIR, nBattle);

    Text::Info info;
    Pages::GlobeSearch* search = SectionMgr::sInstance->curSection->Get<Pages::GlobeSearch>();
    const u32 searchType = search != nullptr ? search->searchType : 0;

    if (searchType == 0) {
        // Worldwide search
        info.intToPass[0] = nVS;
        this->vsButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);

        info.intToPass[0] = nBattle;
        this->battleButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);
    } else {
        // VK Worldwide
        if (this->submenuState == STATE_MAIN) {
            info.intToPass[0] = nVS + n200cc;
            this->vsButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);

            info.intToPass[0] = nOTT + nIR;
            this->battleButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);
        } else if (this->submenuState == STATE_VS_WW) {
            info.intToPass[0] = nVS;
            this->vsButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);

            info.intToPass[0] = n200cc;
            this->battleButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);
        } else if (this->submenuState == STATE_OTHER_VS) {
            info.intToPass[0] = nOTT;
            this->vsButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);

            info.intToPass[0] = nIR;
            this->battleButton.SetTextBoxMessage("go", BMG_PLAYER_COUNT, &info);
        }
    }

    // VR/BR bottom bubble
    RKSYS::Mgr* rksysMgr = RKSYS::Mgr::sInstance;
    u32 vr = 0;
    u32 br = 5000;
    if (rksysMgr->curLicenseId >= 0) {
        RKSYS::LicenseMgr& license = rksysMgr->licenses[rksysMgr->curLicenseId];
        // The extended rating is stored divided by 100, so scale it back up to the
        // displayed VR the button expects.
        vr = static_cast<u32>(PointRating::GetUserVR(rksysMgr->curLicenseId) * 100.0f);
        br = license.br.points;
    }

    wchar_t buffer[64];
    bool showBR = false;
    if (searchType == 0) {
        if (this->battleButton.IsSelected()) {
            showBR = true;
        }
    }
    
    if (showBR) {
        swprintf(buffer, 64, L"%u BR", br);
    } else {
        swprintf(buffer, 64, L"%u VR", vr);
    }
    Text::Info vrInfo;
    vrInfo.strings[0] = buffer;
    this->vrButton.SetTextBoxMessage("go", UI::BMG_TEXT, &vrInfo);
}

static void SetTextBoxMessageSafe(LayoutUIControl& control, const char* textBoxName, u32 bmgId, const Text::Info* textInfo = nullptr) {
    if (control.layout.GetPaneByName(textBoxName) != nullptr) {
        control.SetTextBoxMessage(textBoxName, bmgId, textInfo);
#ifdef ACDIAG
        OS::Report("[WFC DIAG] wrote pane '%s' bmg %04x\n", textBoxName, bmgId);
    } else {
        OS::Report("[WFC DIAG] MISSING pane '%s' (wanted bmg %04x) - label stays blank\n", textBoxName, bmgId);
#endif
    }
}

static void SetButtonLabel(LayoutUIControl& button, u32 bmgId) {
    SetTextBoxMessageSafe(button, "text", bmgId);
    SetTextBoxMessageSafe(button, "text_light_01", bmgId);
    SetTextBoxMessageSafe(button, "text_light_02", bmgId);
}

void ExpWFCModeSel::SetMenuTextAndRatings() {
    Pages::GlobeSearch* search = SectionMgr::sInstance->curSection->Get<Pages::GlobeSearch>();
    const u32 searchType = search != nullptr ? search->searchType : 0;

    if (searchType == 0) {
        SetTextBoxMessageSafe(this->vsButton, "text", BMG_VS_RESTORE); // VS
        SetTextBoxMessageSafe(this->vsButton, "text_light_01", BMG_VS_RESTORE);
        SetTextBoxMessageSafe(this->vsButton, "text_light_02", BMG_VS_RESTORE);
        SetTextBoxMessageSafe(this->battleButton, "text", BMG_BATTLE_RESTORE); // Battle
        SetTextBoxMessageSafe(this->battleButton, "text_light_01", BMG_BATTLE_RESTORE);
        SetTextBoxMessageSafe(this->battleButton, "text_light_02", BMG_BATTLE_RESTORE);
    } else {
        if (this->submenuState == STATE_MAIN) {
            SetButtonLabel(this->vsButton, BMG_WWMENU_MAIN_VS); // VS Worldwide
            SetButtonLabel(this->battleButton, BMG_WWMENU_MAIN_OTHER); // Other VS
        } else if (this->submenuState == STATE_VS_WW) {
            SetButtonLabel(this->vsButton, BMG_WWMENU_VS_VANZA); // Vanza VS
            SetButtonLabel(this->battleButton, BMG_WWMENU_VS_200CC); // 200cc Vanza VS
        } else if (this->submenuState == STATE_OTHER_VS) {
            SetButtonLabel(this->vsButton, BMG_WWMENU_OTHER_OTT); // Online TT
            SetButtonLabel(this->battleButton, BMG_WWMENU_OTHER_ITEMRAIN); // Item Rain WW
        }
    }
}

void ExpWFCModeSel::OnActivatePatch() {
    register ExpWFCModeSel* page;
    asm(mr page, r29;);
    register Pages::GlobeSearch* search;
    asm(mr search, r30;);

    page->submenuState = STATE_MAIN;

    if (search->searchType == 0) {
        Network::REGIONID = System::sInstance->GetInfo().GetWiimmfiRegion();
        System::sInstance->netMgr.ownStatusData = false;
        page->lastClickedButton = 1;
        page->WFCModeSelect::OnModeButtonClick(page->vsButton, 0);
        return;
    }

    // VK Worldwide (search->searchType == 1)
    page->vsButton.isHidden = false;
    page->vsButton.manipulator.inaccessible = false;
    page->battleButton.isHidden = false;
    page->battleButton.manipulator.inaccessible = false;

    page->nextPage = PAGE_NONE;

    page->SetMenuTextAndRatings();
    page->vsButton.SelectInitial(0);
}
kmCall(0x8064c5f0, ExpWFCModeSel::OnActivatePatch);

void ExpWFCModeSel::OnModeButtonSelect(PushButton& modeButton, u32 hudSlotId) {
    Pages::GlobeSearch* search = SectionMgr::sInstance->curSection->Get<Pages::GlobeSearch>();
    const u32 searchType = search != nullptr ? search->searchType : 0;

    if (searchType == 0) {
        WFCModeSelect::OnModeButtonSelect(modeButton, hudSlotId);
    } else {
        if (this->submenuState == STATE_MAIN) {
            if (modeButton.buttonId == 1) {
                this->bottomText.SetMessage(BMG_WWMENU_MAIN_VS_DESC);
            } else {
                this->bottomText.SetMessage(BMG_WWMENU_MAIN_OTHER_DESC);
            }
        } else if (this->submenuState == STATE_VS_WW) {
            if (modeButton.buttonId == 1) {
                this->bottomText.SetMessage(BMG_WWMENU_VS_VANZA_DESC);
            } else {
                this->bottomText.SetMessage(BMG_WWMENU_VS_200CC_DESC);
            }
        } else if (this->submenuState == STATE_OTHER_VS) {
            if (modeButton.buttonId == 1) {
                this->bottomText.SetMessage(BMG_WWMENU_OTHER_OTT_DESC);
            } else {
                this->bottomText.SetMessage(BMG_WWMENU_OTHER_ITEMRAIN_DESC);
            }
        }
    }
}

void ExpWFCModeSel::OnModeButtonClick(PushButton& modeButton, u32 hudSlotId) {
    Pages::GlobeSearch* search = SectionMgr::sInstance->curSection->Get<Pages::GlobeSearch>();
    const u32 searchType = search != nullptr ? search->searchType : 0;

    if (searchType == 0) {
        WFCModeSelect::OnModeButtonClick(modeButton, hudSlotId);
        return;
    }

    const u32 clickedId = modeButton.buttonId;

    if (this->submenuState == STATE_MAIN) {
        if (clickedId == 1) {
            this->submenuState = STATE_VS_WW;
            this->SetMenuTextAndRatings();
            this->vsButton.SelectInitial(0);
        } else {
            this->submenuState = STATE_OTHER_VS;
            this->SetMenuTextAndRatings();
            this->vsButton.SelectInitial(0);
        }
    } else if (this->submenuState == STATE_VS_WW) {
        if (clickedId == 1) {
            Network::REGIONID = System::sInstance->GetInfo().GetWiimmfiRegion();
            System::sInstance->netMgr.ownStatusData = false;
            WFCModeSelect::OnModeButtonClick(modeButton, hudSlotId);
        } else {
            Network::REGIONID = 0x70;
            System::sInstance->netMgr.ownStatusData = false;
            modeButton.buttonId = 1;
            WFCModeSelect::OnModeButtonClick(modeButton, hudSlotId);
            modeButton.buttonId = clickedId;
        }
    } else if (this->submenuState == STATE_OTHER_VS) {
        if (clickedId == 1) {
            Network::REGIONID = 0x69;
            System::sInstance->netMgr.ownStatusData = true;
            modeButton.buttonId = 1;
            WFCModeSelect::OnModeButtonClick(modeButton, hudSlotId);
            modeButton.buttonId = clickedId;
        } else {
            Network::REGIONID = 0x71;
            System::sInstance->netMgr.ownStatusData = false;
            modeButton.buttonId = 1;
            WFCModeSelect::OnModeButtonClick(modeButton, hudSlotId);
            modeButton.buttonId = clickedId;
        }
    }
}

void ExpWFCModeSel::OnBackButtonClick(PushButton& backButton, u32 hudSlotId) {
    Pages::GlobeSearch* search = SectionMgr::sInstance->curSection->Get<Pages::GlobeSearch>();
    if (search != nullptr && search->searchType == 1 && this->submenuState != STATE_MAIN) {
        this->submenuState = STATE_MAIN;
        this->SetMenuTextAndRatings();
        this->vsButton.SelectInitial(0);
    } else {
        WFCModeSelect::OnBackButtonClick(static_cast<CtrlMenuBackButton&>(backButton), hudSlotId);
    }
}

void ExpWFCModeSel::OnBackPress(u32 hudSlotId) {
    Pages::GlobeSearch* search = SectionMgr::sInstance->curSection->Get<Pages::GlobeSearch>();
    if (search != nullptr && search->searchType == 1 && this->submenuState != STATE_MAIN) {
        this->submenuState = STATE_MAIN;
        this->SetMenuTextAndRatings();
        this->vsButton.SelectInitial(0);
    } else {
        WFCModeSelect::OnBackPress(hudSlotId);
    }
}

}//namespace UI
}//namespace Pulsar