#ifndef _PUL_BUTTONREMAPPAGE_
#define _PUL_BUTTONREMAPPAGE_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <Race/ButtonRemap.hpp>
#include <UI/UI.hpp>

//Lets player 1 remap the race buttons of the controller they are holding, for the loaded license.
//A on a row waits for the next button press, which is assigned to that action; START/+ cancels.

namespace Pulsar {
namespace UI {

class ButtonRemapPage : public Pages::MenuInteractable {
public:
    static const PulPageId id = PULPAGE_BUTTONREMAP;
    static const u32 rowCount = ButtonRemap::maxActions + 1; //actions + reset

    ButtonRemapPage();
    ~ButtonRemapPage() override {}

    void OnInit() override;
    void OnActivate() override;
    void OnDeactivate() override;
    void BeforeControlUpdate() override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    int GetActivePlayerBitfield() const override { return this->activePlayerBitfield; }
    int GetPlayerBitfield() const override { return this->playerBitfield; }
    ManipulatorManager& GetManipulatorManager() override { return this->controlsManipulatorManager; }
    UIControl* CreateExternalControl(u32 id) override { return nullptr; }
    UIControl* CreateControl(u32 id) override;
    void SetButtonHandlers(PushButton& button) override;

private:
    enum State {
        STATE_IDLE,
        STATE_WAIT_RELEASE, //the button used to open the prompt is still held
        STATE_CAPTURE, //waiting for the button to assign
        STATE_WAIT_RELEASE_DONE //assigned, waiting for it to be released before the menu takes inputs again
    };

    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonSelect(PushButton& button, u32 hudSlotId);
    void OnButtonDeselect(PushButton& button, u32 hudSlotId) {}
    void OnBackPress(u32 hudSlotId);
    void OnBackButtonClick(PushButton& button, u32 hudSlotId);

    void SetTitle();
    void Refresh();
    void RefreshRow(u32 row);
    void SetBottomText(u32 row);
    void StartCapture(u32 actionIdx);
    void EndCapture();
    void SetMenuLocked(bool locked);
    u16 GetAssignableMask() const;

    PtmfHolder_2A<ButtonRemapPage, void, PushButton&, u32> onBackButtonClickHandler;
    PushButton rows[rowCount];
    float textScaleX[rowCount]; //the layout's own text scale, shrunk from there when a row is too long
    float textScaleY[rowCount];

    ControllerType type;
    u32 license;
    u32 profile;

    State state;
    u32 captureAction;
    u32 stateFrames;
    u16 prevButtons;
};

}//namespace UI
}//namespace Pulsar
#endif
