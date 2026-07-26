#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Scene/GameScene.hpp>
#include <PulsarSystem.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {

//Adds a 3rd UI (menu or race) and a 3rd common to the relevant archive holders, which will contain custom pulsar assets
kmWrite32(0x8052a108, 0x38800003); //Add one archive to CommonArchiveHolder
kmWrite32(0x8052a188, 0x38800003); //Add one archive to UIArchiveHolder
void LoadAssetsFile(ArchiveFile* file, const char* path, EGG::Heap* decompressedHeap, bool isCompressed, s32 allocDirection,
    EGG::Heap* archiveHeap, EGG::Archive::FileInfo* info) {
    const ArchiveMgr* archiveMgr = ArchiveMgr::sInstance;
    if(file == &archiveMgr->archivesHolders[ARCHIVE_HOLDER_UI]->archives[2]) {
        const bool isItalian =
            Settings::Mgr::Get().GetUserSettingValue(
                static_cast<Settings::UserType>(Settings::SETTINGSTYPE_LANGUAGE),
                SCROLLER_LANGUAGE) == LANGUAGE_ITALIAN;
        const char* fileType = GameScene::GetCurrent()->id == SCENE_ID_RACE ? "Race" : "UI";
        const char* languageSuffix = isItalian ? "_I" : "";
        char newPath[0x20];
        snprintf(newPath, sizeof(newPath), "%sAssets%s.szs", fileType, languageSuffix);
        path = newPath;
    }
    else if(file == &archiveMgr->archivesHolders[ARCHIVE_HOLDER_COMMON]->archives[2]) path = System::CommonAssets;
    file->Load(path, decompressedHeap, isCompressed, allocDirection, archiveHeap, info);
}
kmCall(0x8052aa2c, LoadAssetsFile);

}//namespace Pulsar