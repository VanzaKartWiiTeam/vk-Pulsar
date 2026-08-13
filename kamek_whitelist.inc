// Kamek Whitelist for Anti-Cheat System
// This file contains all legitimate Kamek patch addresses used by vk-Pulsar
// These addresses should be whitelisted to prevent false positives

static const u32 kWhitelistedKamekAddresses[] = {
    // Gecko Handler Base Area (used by Kamek)
    0x80001800, 0x80001804, 0x80001808, 0x8000180C,
    0x80001810, 0x80001814, 0x80001818, 0x8000181C,
    
    // VanzaKart System
    0x800017CC, // PUL2 Magic string
    0x800017D0, // pack_id
    0x800017D4, // pack_version
    0x80007880, // PatchPADRead
    0x8000785c, // Controller on strap
    0x800079b0, // CheckControllerStrap
    
    // Codes.cpp - Various game patches
    0x80258184, // Remove Background Blur
    0x805a906c, // Disable Camera Shaking from Bombs
    0x806335B0, // Show Position in Live View
    0x8068E494, // CPUs/Online High Detail Particles
    0x8068DD68, // No Shell Tail Dissolve
    0x807BA077, // Mushroom Glitch Fix
    0x80643BC4, // Instant Voting Roulette Decide
    0x80643C2C, // Instant Voting Roulette Decide
    0x80549898, // Force player not penalized
    0x8054989c, // Force player not penalized
    0x80856a28, // Allow Pausing Before Race Starts
    0x8057C3F8, // Fix star offroad glitch after cannon
    0x80655578, // No Disconnect on Countdown
    0x80654b00, // Prevent Lag Abuse
    0x8053F478, // Disable 6 minute time limit Online
    0x8053511C, // Ultra Uncut
    0x800EE3A0, // Allow WFC on Wiimmfi
    0x800ECAAC, // Allow WFC on Wiimmfi
    0x807eb774, // Fix Unfocused Small Mii Icon Border
    0x805DD85C, // Cancel Friend Room Joining by Pressing B
    0x8052D286, // VR Limit patches
    0x8052D28E,
    0x8064F6DA,
    0x8064F6E6,
    0x8085654E,
    0x80856556,
    0x8085C23E,
    0x8085C246,
    0x8064F76A,
    0x8064F776,
    0x808565BA,
    0x808565C2,
    0x8085C322,
    0x8085C32A,
    0x8064B982, // Remove WW Button patches
    0x8064BA10,
    0x8064BA38,
    0x8064BA50,
    0x8064BA5C,
    0x8064BC12,
    0x8064BC3E,
    0x8064BC4E,
    0x8064BCB6,
    0x8064BCC2,
    0x80544928, // Disable Data Save Reset for Region ID Change
    
    // AutoVote.cpp
    0x80622088, // BuildCustomSection
    0x8062213c, // AddCustomLayers
    
    // Lex.cpp (LECODE)
    0x80512820, // LoadLEXAndKMP
    0x808D3EE0, // ApplyHIPT Vtable
    0x805850b8, // ApplyCANN
    0x805850bc, // ApplyCANN nop
    0x8053f3b8, // ApplySET1TimeLimit
    0x8079b568, // ModifyItemPos
    0x8079b860, // ModifyItemPos exit
    0x8079c994, // FillOrExtractShoot
    0x8079c998, // FillOrExtractShoot branch
    
    // HybridDrift.cpp
    0x8057930C, // GetHybridDrift1
    0x80578DCC, // GetHybridDrift1
    0x8057DFA8, // GetHybridDrift3
    0x8057E018, // GetHybridDrift4
    0x8057E108, // GetHybridDrift6
    0x80594AA8, // GetHybridDrift7
    0x8059450E, // HybridDrift patches
    0x80594A60,
    0x805A35BC,
    0x80745AB0,
    0x808CB70A,
    
    // WUP028.cpp (Cosmos)
    0x80523910, // PatchPADRead
    0x805237c4,
    0x80217464,
    
    // ChooseNextTrack.cpp
    0x8062f020, 0x8062f08c, 0x8062f0f8, 0x8062f164,
    0x8062f1d0, 0x8062f23c, 0x8062f2a8, 0x8062f314,
    
    // ExpFroomMessages.cpp
    0x805dc480, // OnStartButtonFroomMsgActivate
    0x805dd32c, // OnBackPress
    0x805dd314, // OnBackButtonClick
    0x805dcb74, // CorrectModeButtonsBMG
    0x805e4df4, // CorrectRoomStartButton
    0x805dce38, // RemapAndStoreSentMessage
    
    // GhostManager.cpp
    0x806394f0, // InsertCustomGroupToList
    0x80856fec, // PlayCorrectFinishAnim
    0x8052fe78, // IncreaseRacedataSize
    0x80531f44, // Racedata buffer patch
    0x8052f5c8, // RacedataCheckCorrectRKG
    0x8052f5e4, // GhostHeaderGetCorrectRKG
    0x805e158c, // LoadCorrectMainGhost
    0x805e13ac, 0x805e13e4, 0x805e141c, // ExtendSetupGhostRace
    0x805e149c, 0x805e14c8, 0x805e14f4,
    0x805e144c, 0x805e1518, // ExtendSetupGhostReplay
    0x805c7b6c, // SetCorrectGhostRaceSlot
    
    // BootIntoSection.cpp
    0x80634f20, // BootIntoSection
    0x805243f4, // SetUpCorrectController
    0x805243e4, // mr r5, r27
    0x8061af98, // silent controller changing
    
    // VanzaKart.cpp
    0x80625E1C, // HideChannelButton
    
    // ColorHUD.cpp
    0x805f03dc, 0x805f0440, // GetHUDColor
    0x805f04d8, // GetHUDBaseColor
    0x807ec1dc, // GetHUDRaceColor
    
    // BRSARPatches.cpp
    0x800a66f4, // LoadBRSTMVolumeAndFixTrackCount
    0x800a66f0,
    0x80091354, // BRSAREntrySizePatch
    
    // ChangeCombo.cpp
    0x806508d4, // Add VR screen
    0x8064a61c, // nop initControlGroup
    0x808998b3, // WifiMemberConfirmButton rename
    0x8062e09c, 0x8062e7e0, 0x8062e870, // AddChangeComboPages P1
    0x8062e0e4, 0x8062e900, 0x8062e990,
    0x8062e708, 0x8062e798, 0x8062ea68, // AddChangeComboPages P2
    0x8062eaf8, 0x8062eb88, 0x8062ec18,
    0x808D9DF8, // DriftSelectBeforeControlUpdate
    0x808D9C10, // MultiDriftSelectBeforeControlUpdate
    0x806509d0, // AddCharSelectLayer
    0x8084e670, // LoadCorrectPageAfterDrift
    
    // BRSTMExpansion.cpp
    0x8009e0dc, // Music slot expansion
    0x8009e0e4, // MusicSlotsExpand
    
    // PulsarSystem.cpp
    0x80543bb4, // CreateSystem
    0x8051ac40, // OnSceneEnter
    0x80549974, // Unlock Everything
    0x80604094, // Skip ESRB page
    
    // BRSTMSpeedUp.cpp
    0x8070b2f8, // MusicSpeedup
    0x8070b2c0,
    0x8070b2d4,
    
    // ConditionalChannel.cpp
    0x806fab78, // ConditionalChannelSwitch
    
    // FriendRoomRegional.cpp
    0x80635a3c, // SetNextSectionRegionalHook
    
    // MiscSound.cpp
    0x80711fcc, 0x80711df4, 0x80712198, // DisableRaceMusic
    0x80711074, 0x8064a398, 0x8064a340,
    0x806f8eb4, // PreventPrepareRaceMusic
    0x806fa64c, // DisableMenuMusic
    0x806fa664, // DisableAndChangeBGMusic
    0x8085a674, // no TT jingle prep
    0x806fa2fc, // PatchPrepareStreamsBG
    0x80857860, // CheckFanfare
    0x80711FE8, 0x80712024, 0x8071207C, 0x807120B8, // disable TF music delay
    0x80719920, // disable tf music reset
    0x806fa738, // disable THP and demo
    
    // COOB.cpp
    0x80571870, // COOB
    
    // SlotProperties.cpp
    0x8068dfc8, 0x8068dfcc, // PreventRSLCrash
    0x8068e2b0,
    0x80827a34, 0x80827a3c, // PreventRSBCrash
    0x808279ac, 0x808279b4, // PreventMHCrash
    0x8078e1ec, 0x8078e1f0, // PreventMHMatCrash
    0x8082c140, 0x807f92ac, // GetCommonBinary
    0x807ea670, // SecondaryKTPT
    
    // MiscUI.cpp
    0x8054913C, 0x80855f48, // No ghost saving on RKSYS
    0x8007B37C, // BMG size patch
    0x8085A6A8, // CenterTopMenuWifiWaku
    0x806497F8, 0x8064A600, // No Wii logo in froom
    0x80644904, 0x8084C96C, // Character rendering patches
    0x808BC804, 0x808BB804,
    0x808BAC04, 0x808BC874,
    0x80643970, 0x80643908, // VS Custom page
    0x80660E64, 0x8065FE00, // No Battle Mode
    0x80622F04, // No GP mode
    0x80856904, // AddLapsLayer
    0x8083FD38, // Lap count patches
    
    // Network patches
    0x80662a48, 0x80662ab0, // FindAvailableAidFriend
    0x80545a94, 0x80545a9c, // SaveFile patches
    0x80545c4c, 0x80545c54,
    0x80545b58, 0x80545b60,
    0x80655698, 0x80662950, // RandomMiis
    0x80661cc0, 0x80661cc8, // FriendMatching
    0x80662990, 0x80662998,
    0x8065f0b0, 0x8065ef5c, // CustomMatchingAreaChange
    0x8065ee74, 0x8065ee80,
    0x80660074, 0x80660168,
    0x8065eff0,
    
    // Timer and race patches
    0x80856870, 0x80856814, // Timer patches
    0x8085670c,
    0x8052c93c, 0x8052ca48, // Speedometer
    0x8082c568, 0x8082d338, // CorrectObjFlowBMG
    0x807e8068, 0x807e80c8, // MinimapRenderAll
    0x807e85e8,
    
    // Item and effect patches
    0x807f3cdc, 0x807f3d80, // GeoHitEntry
    0x807f3dc0,
    0x807d7c30, 0x807d7fec, // EffectSetupHook
    0x8068f3a0, 0x8068f3ac, // GetCorrectParticleName
    
    // Kart and physics patches
    0x8059213c, // Kart::GetLink
    0x80591E34, 0x80591F9C,
    0x807ea1d0, // MinimapKartIcon
    0x80574cd4, 0x805750d8, // Bike/Kart patches
    0x80831654, 0x8083168c,
    0x8082e9a4, 0x8082e9c0,
    
    // UI Control patches
    0x8063bbb0, 0x8063d100, // Delay Online Friend page load
    0x80645a58, // Room patches
    0x80647ad8, 0x80647b50,
    0x80647bcc,
    0x8064c0f0, 0x8064bd6c, // VoteNextTrack
    0x8064bfb4, 0x8064be5c,
    0x8064a7d4, 0x8064a8a0, // TeamSelect
    0x8064a83c, 0x8064a908,
    0x8083f618, // OnlineTeamChange
    
    // CourseID and Track patches
    0x80854e28, 0x80854ea8, // Track images
    0x80831700, 0x80831758,
    0x807e7e10, // Minimap patches
    0x8085a410, 0x8085a71c, // CountDownTimer
    0x8085a480, 0x8085a544,
    0x808554a8, // WifiVSNextPageId
    0x80852aa8, 0x80852adc, // WifiVS
    0x8083fef8, 0x8084006c, // RaceMenu
    0x8083fe70,
    0x80852938, 0x80852998, // WifiFriend
    0x80643a50, 0x80643aa4, // PauseMenu
    0x80643a24,
    0x8063faec, // NewMiiCharSelectPhase
    0x808d9c10, 0x808d9df8, // Multi/DriftSelect
    
    // Additional system patches
    0x80553540, // Scenario GetWinningCourse
    0x805512d0, 0x805512a8, // Scenario GetCourse
    0x8055113c, 0x80550e64,
    0x805515f8, 0x805515e0,
    0x80551710, 0x805516ec,
    
    // End marker (used for array size calculation)
    0xFFFFFFFF
};
