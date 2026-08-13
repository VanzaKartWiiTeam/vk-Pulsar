#ifndef _PUL_SETTINGSPAGESELECT_
#define _PUL_SETTINGSPAGESELECT_
#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <Settings/SettingsParam.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

class SettingsPageSelect : public Pages::MenuInteractable {
public:
    static const PulPageId id = PULPAGE_SETTINGSPAGESELECT;

    SettingsPageSelect();
    ~SettingsPageSelect() override {}

    void OnInit() override;
    void OnActivate() override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    int GetActivePlayerBitfield() const override;
    int GetPlayerBitfield() const override;
    ManipulatorManager& GetManipulatorManager() override;
    UIControl* CreateExternalControl(u32 id) override { return nullptr; }
    UIControl* CreateControl(u32 id) override;
    void SetButtonHandlers(PushButton& button) override;
    void BeforeControlUpdate() override;

    void OnBackPress(u32 hudSlotId);
    void OnBackButtonClick(PushButton& button, u32 hudSlotId);

private:
    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonSelect(PushButton& button, u32 hudSlotId);
    void OnButtonDeselect(PushButton& button, u32 hudSlotId) {}

    PtmfHolder_2A<SettingsPageSelect, void, PushButton&, u32> onBackButtonClickHandler;

    PushButton pageButtons[Settings::Params::pageCount];
};

}
}
#endif
