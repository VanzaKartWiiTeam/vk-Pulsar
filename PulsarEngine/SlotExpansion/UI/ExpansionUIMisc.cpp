#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CupSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <MarioKartWii/UI/Page/Other/GhostSelect.hpp>
#include <MarioKartWii/UI/Page/Other/Votes.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpCupSelect.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <Network/PacketExpansion.hpp>


namespace Pulsar {
namespace UI {
static bool IsGroupedTrack(PulsarId id) {
    if (CupsConfig::IsReg(id)) return false;
    const u32 idx = id - 0x100;
    switch (idx) {
        case 6:
        case 9:
        case 27:
        case 29:
        case 31:
        case 32:
        case 37:
        case 51:
        case 57:
        case 61:
        case 63:
        case 67:
        case 73:
        case 76:
        case 77:
        case 85:
            return true;
        default:
            if (idx >= 88 && idx <= 103) return true;
            return false;
    }
}

bool IsTrackBlocked(PulsarId id) {
    System* system = System::sInstance;
    if (!system || !system->IsContext(PULSAR_CT)) return false;

    const u32 blockingCount = system->GetInfo().GetTrackBlocking();
    if (blockingCount == 0 || system->netMgr.lastTracks == nullptr) return false;

    for (u32 i = 0; i < blockingCount; ++i) {
        if (system->netMgr.lastTracks[i] == id) return true;
    }

    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (controller != nullptr &&
        (controller->roomType == RKNet::ROOMTYPE_JOINING_REGIONAL ||
         controller->roomType == RKNet::ROOMTYPE_VS_REGIONAL)) {
        if (IsGroupedTrack(id) && system->netMgr.lastGroupedTrackPlayed) {
            return true;
        }
    }

    return false;
}

static void RemoveAllEscapeSequences(wchar_t* dest, const wchar_t* src) {
    while (*src != L'\0') {
        if (src[0] == 0x001A) {
            const u8* escapeBytes = reinterpret_cast<const u8*>(src);
            u8 escapeLength = escapeBytes[2];
            src = reinterpret_cast<const wchar_t*>(escapeBytes + escapeLength);
        } else {
            *dest++ = *src++;
        }
    }
    *dest = L'\0';
}

static void BuildRainbowTrackName(wchar_t* dest, const wchar_t* src, u32 maxLen) {
    wchar_t cleanSrc[0x100];
    RemoveAllEscapeSequences(cleanSrc, src);

    static const u16 colors[] = {
        0x0017, // Red
        0x0013, // Orange
        0x0010, // Yellow
        0x0033, // Green
        0x0021, // Cyan
        0x0031, // Blue
        0x0040  // Pink/Purple
    };
    const u32 numColors = sizeof(colors) / sizeof(colors[0]);
    u32 colorIdx = 0;

    u32 destIdx = 0;
    u32 srcIdx = 0;

    while (cleanSrc[srcIdx] != L'\0' && destIdx < maxLen - 5) {
        wchar_t curChar = cleanSrc[srcIdx++];
        if (curChar != L' ') {
            dest[destIdx++] = 0x001A;
            dest[destIdx++] = 0x0800;
            dest[destIdx++] = 0x0001;
            dest[destIdx++] = colors[colorIdx];
            colorIdx = (colorIdx + 1) % numColors;
        }
        dest[destIdx++] = curChar;
    }
    dest[destIdx] = L'\0';
}

//Change brctr names
kmWrite24(0x808a85ef, 'PUL'); //used by 807e5754

static void LoadCtrlMenuCourseSelectCupBRCTR(ControlLoader& loader, const char* folderName, const char* ctrName,
    const char* variant, const char** animNames) {
    loader.Load(UI::buttonFolder, "PULrseSelectCup", variant, animNames); //Move to button to avoid duplication of cup icon tpls
}
kmCall(0x807e4538, LoadCtrlMenuCourseSelectCupBRCTR);

static void LoadCorrectTrackListBox(ControlLoader& loader, const char* folder, const char* normalBrctr, const char* variant, const char** anims) {
    loader.Load(folder, "PULSelectNULL", variant, anims);
}
kmCall(0x807e5f24, LoadCorrectTrackListBox);

//BMG
int GetTrackBMGId(PulsarId pulsarId, bool useCommonName) {
    u32 bmgId;
    u32 realId = CupsConfig::ConvertTrack_PulsarIdToRealId(pulsarId);
    if (CupsConfig::IsReg(pulsarId)) bmgId = realId > 32 ? BMG_BATTLE : BMG_REGS;
    else {
        bmgId = BMG_TRACKS;
        const CupsConfig* cupsConfig = CupsConfig::sInstance;
        u8 variantIdx;
        if (useCommonName) {
            if (cupsConfig->GetTrack(pulsarId).variantCount > 0) variantIdx = 8;
            else variantIdx = 0;
        }
        else variantIdx = cupsConfig->GetCurVariantIdx();
        realId += variantIdx << 12;
    }
    return bmgId + realId;
}

int GetTrackBMGByRowIdx(u32 cupTrackIdx) {
    const Pages::CupSelect* cup = SectionMgr::sInstance->curSection->Get<Pages::CupSelect>();
    PulsarCupId curCupId;
    if (cup == nullptr) curCupId = PULSARCUPID_FIRSTREG;
    else curCupId = static_cast<PulsarCupId>(cup->ctrlMenuCupSelectCup.curCupID);
    return GetTrackBMGId(CupsConfig::sInstance->ConvertTrack_PulsarCupToTrack(curCupId, cupTrackIdx), true);
}
kmWrite32(0x807e6184, 0x7FA3EB78);
kmCall(0x807e6188, &GetTrackBMGByRowIdx);
kmWrite32(0x807e6088, 0x7F63DB78);
kmCall(0x807e608c, GetTrackBMGByRowIdx);

static wchar_t s_blockedCupPreviewBuffer[4][0x100];

static void SetCupPreviewTrackMessageImpl(LayoutUIControl* control, u32 bmgId, const Text::Info* info, u32 trackIdx) {
    const Pages::CupSelect* cup = SectionMgr::sInstance->curSection->Get<Pages::CupSelect>();
    PulsarCupId curCupId;
    if (cup == nullptr) curCupId = PULSARCUPID_FIRSTREG;
    else curCupId = static_cast<PulsarCupId>(cup->ctrlMenuCupSelectCup.curCupID);

    PulsarId trackId = CupsConfig::sInstance->ConvertTrack_PulsarCupToTrack(curCupId, trackIdx);
    if (IsTrackBlocked(trackId) && trackIdx < 4) {
        const wchar_t* originalText = GetCustomMsg(bmgId);
        if (originalText != nullptr) {
            BuildRainbowTrackName(s_blockedCupPreviewBuffer[trackIdx], originalText, 0x100);
            Text::Info blockedInfo;
            blockedInfo.strings[0] = s_blockedCupPreviewBuffer[trackIdx];
            control->SetMessage(BMG_TEXT, &blockedInfo);
            return;
        }
    }
    control->SetMessage(bmgId, info);
}

static void SetCupPreviewTrackMessage_R27(LayoutUIControl* control, u32 bmgId, const Text::Info* info) {
    register u32 trackIdx;
    asm(mr trackIdx, r27;);
    SetCupPreviewTrackMessageImpl(control, bmgId, info, trackIdx);
}
kmCall(0x807e609c, SetCupPreviewTrackMessage_R27);

static void SetCupPreviewTrackMessage_R29(LayoutUIControl* control, u32 bmgId, const Text::Info* info) {
    register u32 trackIdx;
    asm(mr trackIdx, r29;);
    SetCupPreviewTrackMessageImpl(control, bmgId, info, trackIdx);
}
kmCall(0x807e6198, SetCupPreviewTrackMessage_R29);

int GetCurTrackBMG() {
    return GetTrackBMGId(CupsConfig::sInstance->GetWinning(), false);
}

static void SetVSIntroBmgId(LayoutUIControl* trackName) {
    u32 bmgId = GetCurTrackBMG();
    Text::Info info;
    info.bmgToPass[0] = bmgId;
    u32 authorId;
    if (bmgId < BMG_TRACKS) authorId = BMG_NINTENDO;
    else authorId = bmgId + BMG_AUTHORS - BMG_TRACKS;
    info.bmgToPass[1] = authorId;
    trackName->SetMessage(BMG_INFO_DISPLAY, &info);
}
kmCall(0x808552cc, SetVSIntroBmgId);

static void SetAwardsResultCupInfo(LayoutUIControl& awardType, const char* textBoxName, u32 bmgId, Text::Info& info) {
    PulsarCupId id = CupsConfig::sInstance->lastSelectedCup;
    if (!CupsConfig::IsRegCup(id)) {
        awardType.layout.GetPaneByName("cup_icon")->flag &= ~1;
        u32 realCupId = CupsConfig::ConvertCup_PulsarIdToRealId(id);
        u32 cupBmgId;
        u16 iconCount = System::sInstance->GetInfo().GetCupIconCount();
        if (realCupId > iconCount - 1) {
            wchar_t cupName[0x20];
            swprintf(cupName, 0x20, L"Cup %d", realCupId);
            info.strings[0] = cupName;
            cupBmgId = BMG_TEXT;
        }
        else cupBmgId = BMG_CUPS + realCupId;
        info.bmgToPass[1] = cupBmgId;
    }
    awardType.SetTextBoxMessage(textBoxName, bmgId, &info);
}
kmCall(0x805bcb88, SetAwardsResultCupInfo);

static void SetGPIntroInfo(LayoutUIControl& titleText, u32 bmgId, Text::Info& info) {

    PulsarCupId id = CupsConfig::sInstance->lastSelectedCup;
    if (!CupsConfig::IsRegCup(id)) {
        titleText.layout.GetPaneByName("cup_icon")->flag &= ~1;
        u32 realCupId = CupsConfig::ConvertCup_PulsarIdToRealId(id);
        u32 cupBmgId;
        u16 iconCount = System::sInstance->GetInfo().GetCupIconCount();
        if (realCupId > iconCount - 1) {
            wchar_t cupName[0x20];
            swprintf(cupName, 0x20, L"Cup %d", realCupId);
            info.strings[0] = cupName;
            cupBmgId = BMG_TEXT;
        }
        else cupBmgId = BMG_CUPS + realCupId;
        info.bmgToPass[1] = cupBmgId;

    }
    titleText.SetMessage(bmgId, &info);
}
kmCall(0x808553b4, SetGPIntroInfo);

static void SetGPBottomText(CtrlMenuInstructionText& bottomText, u32 bmgId, Text::Info& info) {
    register ExpCupSelect* cupPage;
    asm(mr cupPage, r31;);
    PulsarCupId id = static_cast<PulsarCupId>(cupPage->ctrlMenuCupSelectCup.curCupID);

    if (!CupsConfig::IsRegCup(id)) {
        u32 realCupId = CupsConfig::ConvertCup_PulsarIdToRealId(id);
        register u32 cc;
        asm(mr cc, r28;);
        u8 status = Settings::Mgr::GetGPStatus(realCupId, cc);
        u32 trophyBmg;
        u32 rankBmg;
        if (status == 0xFF) {
            trophyBmg = BMG_GP_BLANK;
            rankBmg= BMG_GP_BLANK;
        }
        else {
            trophyBmg = BMG_GP_GOLD_TROPHY + Settings::Mgr::ComputeTrophyFromStatus(status);
            rankBmg = BMG_GP_RANK_3STARS + Settings::Mgr::ComputeRankFromStatus(status);
        }
        info.bmgToPass[1] = trophyBmg;
        info.bmgToPass[2] = rankBmg;
    }
    bottomText.SetMessage(bmgId, &info);
}
kmCall(0x80841720, SetGPBottomText);

static void SetGhostInfoTrackBMG(GhostInfoControl* control, const char* textBoxName) {
    control->SetTextBoxMessage(textBoxName, GetCurTrackBMG());
}
kmCall(0x805e2a4c, SetGhostInfoTrackBMG);

kmWrite32(0x808406e8, 0x388000ff); //store 0xFF on timeout instead of -1
kmWrite32(0x808415ac, 0x388000ff);
kmWrite32(0x80643004, 0x3be000ff);
kmWrite32(0x808394e8, 0x388000ff);
kmWrite32(0x80644104, 0x3b5b0000);
static wchar_t s_blockedVoteNameBuffer[12][0x100];

static bool IsVoteTrackBlocked(PulsarId courseVote) {
    const Network::ExpSELECTHandler& handler = Network::ExpSELECTHandler::Get();
    if (handler.toSendPacket.pulWinningTrack != 0xFF && handler.toSendPacket.pulWinningTrack == courseVote) return false;
    return IsTrackBlocked(courseVote);
}

void SetVoteControlMessage(VoteControl& vote, u32 bmgId, PulsarId courseVote, u32 playerId) {
    if (IsVoteTrackBlocked(courseVote) && playerId < 12) {
        const wchar_t* originalText = GetCustomMsg(bmgId);
        if (originalText != nullptr) {
            BuildRainbowTrackName(s_blockedVoteNameBuffer[playerId], originalText, 0x100);
            Text::Info info;
            info.strings[0] = s_blockedVoteNameBuffer[playerId];
            vote.SetMessage(BMG_TEXT, &info);
            return;
        }
    }
}

static void CourseVoteBMG(VoteControl* vote, bool isCourseIdInvalid, PulsarId courseVote, MiiGroup& miiGroup, u32 playerId, bool isLocalPlayer, u32 team) {
    u32 bmgId = courseVote;
    if (bmgId != 0x1101 && bmgId < 0x2498) bmgId = GetTrackBMGId(courseVote, true);
    vote->Fill(isCourseIdInvalid, bmgId, miiGroup, playerId, isLocalPlayer, team);
    SetVoteControlMessage(*vote, bmgId, courseVote, playerId);
}
kmCall(0x806441b8, CourseVoteBMG);

static bool BattleArenaBMGFix(SectionId sectionId) {
    register PulsarId id;
    asm(mr id, r28;);
    CupsConfig::sInstance->SetWinning(id, 0);
    return IsOnlineSection(sectionId);
}
kmCall(0x8083d02c, BattleArenaBMGFix);


//kmWrite32(0x80644340, 0x7F64DB78);
static void WinningTrackBMG(PulsarId winningCourse) {
    register Pages::Vote* vote;
    asm(mr vote, r27;);
    vote->trackBmgId = GetTrackBMGId(winningCourse, true);
}
kmCall(0x80644344, WinningTrackBMG);

//Rewrote InitSelf to start with correct TPLs
static void ExtCupSelectCupInitSelf(CtrlMenuCupSelectCup* cups) {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    PulsarCupId selCup = cupsConfig->lastSelectedCup;
    cups->curCupID = selCup;
    PushButton** buttons = reinterpret_cast<PushButton**>(cups->childrenGroup.controlArray);

    for (int i = 0; i < 8; ++i) {
        const PulsarCupId id = cupsConfig->GetNextCupId(selCup, i - cupsConfig->lastSelectedCupButtonIdx);
        buttons[i]->buttonId = id;
        ExpCupSelect::UpdateCupData(id, *buttons[i]);
        buttons[i]->SetOnClickHandler(cups->onCupButtonClickHandler, 0);
        buttons[i]->SetOnSelectHandler(cups->onCupButtonSelectHandler);
        buttons[i]->SetPlayerBitfield(SectionMgr::sInstance->curSection->Get<Pages::CupSelect>()->GetPlayerBitfield());
    }
    buttons[cupsConfig->lastSelectedCupButtonIdx]->SelectInitial(0);
};
kmWritePointer(0x808d324c, ExtCupSelectCupInitSelf); //807e5894

static void ExtCourseSelectCupInitSelf(CtrlMenuCourseSelectCup* courseCups) {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    for (int i = 0; i < 8; ++i) {
        CtrlMenuCourseSelectCupSub& cur = courseCups->cupIcons[i];
        const PulsarCupId id = cupsConfig->GetNextCupId(cupsConfig->lastSelectedCup, i - cupsConfig->lastSelectedCupButtonIdx);
        ExpCupSelect::UpdateCupData(id, cur);
        cur.animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
        const bool clicked = cupsConfig->lastSelectedCupButtonIdx == i ? true : false;
        cur.animator.GetAnimationGroupById(1).PlayAnimationAtFrame(!clicked, 0.0f);
        cur.animator.GetAnimationGroupById(2).PlayAnimationAtFrame(!clicked, 0.0f);
        cur.animator.GetAnimationGroupById(3).PlayAnimationAtFrame(clicked, 0.0f);
        cur.selected = clicked;
        cur.SetRelativePosition(courseCups->positionAndscale[1]);
    }
    const Section* curSection = SectionMgr::sInstance->curSection;
    Pages::CupSelect* cup = curSection->Get<Pages::CupSelect>();
    NoteModelControl* positionArray = cup->modelPosition;

    switch (cup->extraControlNumber) {
    case(2):
        positionArray[0].positionAndscale[1].position.x = -52.0f;
        positionArray[0].positionAndscale[1].position.y = -8.0f;
        positionArray[0].positionAndscale[1].scale.x = 0.875f;
        positionArray[0].positionAndscale[1].scale.z = 0.875f;
        positionArray[1].positionAndscale[1].position.x = -52.0f;
        positionArray[1].positionAndscale[1].position.y = -13.0f;
        positionArray[1].positionAndscale[1].scale.x = 0.875f;
        positionArray[1].positionAndscale[1].scale.z = 0.875f;
        break;
    case(1):
        positionArray[0].positionAndscale[1].position.x = -32.0f;
        positionArray[0].positionAndscale[1].position.y = -32.0f;
        positionArray = curSection->Get<Pages::CourseSelect>()->modelPosition;
        positionArray[0].positionAndscale[1].position.x = -32.0f;
        positionArray[0].positionAndscale[1].position.y = -32.0f;
        break;
    case(4):
        positionArray[3].positionAndscale[1].position.x = 64.0f;
        positionArray[3].positionAndscale[1].position.y = -55.25f;
        positionArray[3].positionAndscale[1].scale.x = 0.6875f;
        positionArray[3].positionAndscale[1].scale.z = 0.6875f;
    case(3):
        positionArray[0].positionAndscale[1].position.x = 64.0f;
        positionArray[0].positionAndscale[1].position.y = -64.0f;
        positionArray[0].positionAndscale[1].scale.x = 0.6875f;
        positionArray[0].positionAndscale[1].scale.z = 0.6875f;
        positionArray[1].positionAndscale[1].position.x = 64.0f;
        positionArray[1].positionAndscale[1].position.y = -64.0f;
        positionArray[1].positionAndscale[1].scale.x = 0.6875f;
        positionArray[1].positionAndscale[1].scale.z = 0.6875f;
        positionArray[2].positionAndscale[1].position.x = 64.0f;
        positionArray[2].positionAndscale[1].position.y = -55.25f;
        positionArray[2].positionAndscale[1].scale.x = 0.6875f;
        positionArray[2].positionAndscale[1].scale.z = 0.6875f;
        break;
    }
};
kmWritePointer(0x808d3190, ExtCourseSelectCupInitSelf); //807e45c0

static wchar_t s_blockedTrackNameBuffer[4][0x100];

static void ExtCourseSelectCourseInitSelf(CtrlMenuCourseSelectCourse* course) {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const Section* curSection = SectionMgr::sInstance->curSection;
    const Pages::CupSelect* cupPage = curSection->Get<Pages::CupSelect>();
    Pages::CourseSelect* coursePage = curSection->Get<Pages::CourseSelect>();
    //channel ldb stuff ignored
    const u32 cupId = cupPage->clickedCupId;

    PushButton* toSelect = &course->courseButtons[0];
    for (int i = 0; i < 4; ++i) {
        PushButton& curButton = course->courseButtons[i];
        curButton.buttonId = i;
        const u32 bmgId = GetTrackBMGByRowIdx(i);
        
        PulsarId trackId = cupsConfig->ConvertTrack_PulsarCupToTrack(cupsConfig->lastSelectedCup, i);
        if (IsTrackBlocked(trackId)) {
            const wchar_t* originalText = GetCustomMsg(bmgId);
            if (originalText != nullptr) {
                BuildRainbowTrackName(s_blockedTrackNameBuffer[i], originalText, 0x100);
                Text::Info info;
                info.strings[0] = s_blockedTrackNameBuffer[i];
                curButton.SetMessage(BMG_TEXT, &info);
            }
            else {
                curButton.SetMessage(bmgId);
            }
        }
        else {
            curButton.SetMessage(bmgId);
        }

        if (trackId == cupsConfig->GetSelected()) {
            toSelect = &curButton;
        }
    };
    coursePage->SelectButton(*toSelect);
};
kmWritePointer(0x808d30d8, ExtCourseSelectCourseInitSelf); //807e5118

//Multiplayer Fix
kmWrite32(0x807e56d4, 0x60000000);
kmWrite32(0x807e5f04, 0x60000000);

//TPL
//CupSelectCup patch, disable picture panes
kmWrite32(0x807e57a4, 0x60000000);
kmWrite32(0x807e57bc, 0x60000000);
kmWrite32(0x807e57d4, 0x60000000);

//CourseSelectCup patch, disable picture panes
kmWrite32(0x807e4550, 0x60000000);
kmWrite32(0x807e4568, 0x60000000);
kmWrite32(0x807e4580, 0x60000000);
}//namespace UI
}//namespace Pulsar