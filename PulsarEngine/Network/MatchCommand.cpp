#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/UI/Ctrl/UIControl.hpp>
#include <Network/MatchCommand.hpp>
#include <PulsarSystem.hpp>
#include <UI/RoomKick/RoomKickPage.hpp>
#include <core/rvl/os/OS.hpp>

namespace Pulsar {
namespace Network {

ResvPacket::ResvPacket(const DWC::Reservation& src) {
    memcpy(this, &src, sizeof(DWC::Reservation));

    const System* system = System::sInstance;
    const Mgr& mgr = system->netMgr;
    pulInfo.statusData = mgr.ownStatusData;
    pulInfo.roomKey = system->GetInfo().GetKey();
    strncpy(pulInfo.modFolderName, system->GetModFolder(), IOS::ipcMaxFileName);
}

asmFunc MoveSize() { //needed to get datasize later
    ASM(
        nofralloc;
    mr r25, r28;
    li r28, 255;
    blr;
        )
}
kmCall(0x800dc3bc, MoveSize);

DWC::MatchCommand Process(DWC::MatchCommand type, const void* data, u32 dataSize, u32 pid) {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    const bool isCustom = roomType == RKNet::ROOMTYPE_FROOM_NONHOST || roomType == RKNet::ROOMTYPE_FROOM_HOST
        || roomType == RKNet::ROOMTYPE_VS_REGIONAL || roomType == RKNet::ROOMTYPE_JOINING_REGIONAL;

    Pulsar::System* system = Pulsar::System::sInstance;
    Mgr& mgr = system->netMgr;
    DenyType denyType = DENY_TYPE_NORMAL;

    if(type == DWC::MATCH_COMMAND_RESV_OK && isCustom) {
        const ResvPacket* packet = reinterpret_cast<const ResvPacket*>(data);
        const bool badSize = dataSize != (sizeof(ResvPacket) / sizeof(u32));
        const bool badKey = badSize || packet->pulInfo.roomKey != system->GetInfo().GetKey();
        const bool badFolder = badSize || strcmp(packet->pulInfo.modFolderName, system->GetModFolder()) != 0;
        const bool badUser = badSize || !system->CheckUserInfo(packet->pulInfo.userInfo);
        if(badSize || badKey || badFolder || badUser) {

            //Host-side reason for the "Reservation was denied" the joining console reports.
            OS::Report("[VK RESV] deny pid=%u roomType=%d size=%u/%u key=0x%08X/0x%08X folder='%s'/'%s'"
                       " badSize=%d badKey=%d badFolder=%d badUser=%d\n",
                       pid, roomType, dataSize, (u32)(sizeof(ResvPacket) / sizeof(u32)),
                       badSize ? 0u : packet->pulInfo.roomKey, system->GetInfo().GetKey(),
                       badSize ? "?" : packet->pulInfo.modFolderName, system->GetModFolder(),
                       (int)badSize, (int)badKey, (int)badFolder, (int)badUser);

            denyType = DENY_TYPE_BAD_PACK;
            if(roomType == RKNet::ROOMTYPE_VS_REGIONAL) mgr.deniesCount++;
            type = DWC::MATCH_COMMAND_RESV_DENY;
        }
        else if(roomType == RKNet::ROOMTYPE_VS_REGIONAL) {
            if(packet->pulInfo.statusData != mgr.ownStatusData) {
                           pid, ((int)packet->pulInfo.statusData, (int)mgr.ownStatusData);
                denyType = DENY_TYPE_OTT;
                type = DWC::MATCH_COMMAND_RESV_DENY;
            }
        }

        if (SectionMgr::sInstance && SectionMgr::sInstance->curSection) {
            UI::ExpSection* expSection = reinterpret_cast<UI::ExpSection*>(SectionMgr::sInstance->curSection);
            UI::RoomKickPage* roomKick = expSection->GetPulPage<UI::RoomKickPage>();
            if (roomKick) {
                u32 bannedCount = 0;
                u32* bannedPIDs = roomKick->GetKickHistory(bannedCount);
                for (u32 i = 0; i < bannedCount; i++) {
                    if (bannedPIDs[i] == pid) {
                        OS::Report("[VK RESV] deny pid=%u reason=KICK (kickedCount=%u)\n", pid, bannedCount);
                        denyType = DENY_TYPE_KICK;
                        type = DWC::MATCH_COMMAND_RESV_DENY;
                        break;
                    }
                }
            }
        }

        //Logged unconditionally: if the joining console reports a denied reservation and this
        //line says OK, the deny came from vanilla DWC/RKNet, not from any Pulsar check.
        OS::Report("[VK RESV] result pid=%u roomType=%d -> %s\n", pid, roomType,
                   type == DWC::MATCH_COMMAND_RESV_DENY ? "DENY" : "OK");
    }
    mgr.denyType = denyType;
    return type;
}


static int GetSuspendType(int r3, const char* string) {
    DWC::Printf(r3, string);
    const u32 errorType = 0x120000000 + Pulsar::System::sInstance->netMgr.denyType << 28;
    return errorType;
}
kmCall(0x800dc9e8, GetSuspendType);
kmWrite32(0x800dc9f4, 0x906100d8);

static void HasBeenPulsarDenied(u32 level, const char* string) {
    register u32 error;
    asm(mr error, r0);
    DenyType type = DENY_TYPE_NORMAL;
    Mgr& mgr = Pulsar::System::sInstance->netMgr;
    if(error != 0x12) {
        type = static_cast<DenyType>(error & 0xf);
        if(RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL && type == DENY_TYPE_BAD_PACK) mgr.deniesCount++;
    }
    //Joining side: which reason the host encoded in the RESV_DENY (0=normal, 1=pack, 2=OTT, 3=kick).
    OS::Report("[VK RESV] denied by host: error=0x%X -> denyType=%d\n", error, (int)type);
    Pulsar::System::sInstance->netMgr.denyType = type;
    DWC::Printf(level, string);
}
kmCall(0x800dd054, HasBeenPulsarDenied);
kmWrite32(0x800dd044, 0x60000000);

asmFunc ProcessWrapper() {
    ASM(
        nofralloc;
    mr r4, r31;
    mr r5, r25;
    mr r6, r24;
    mflr r31;
    bl Process;
    mtlr r31;
    rlwinm r0, r3, 0, 24, 31;
    blr;
        )
}
kmCall(0x800dc4a0, ProcessWrapper);

void Send(DWC::MatchCommand type, u32 pid, u32 ip, u16 port, void* data, u32 dataSize) {
    const RKNet::RoomType roomType = RKNet::Controller::sInstance->roomType;
    const bool isCustom = roomType == RKNet::ROOMTYPE_FROOM_NONHOST || roomType == RKNet::ROOMTYPE_FROOM_HOST
        || roomType == RKNet::ROOMTYPE_VS_REGIONAL || roomType == RKNet::ROOMTYPE_JOINING_REGIONAL;
    if(type == DWC::MATCH_COMMAND_RESERVATION && isCustom) {
        ResvPacket packet(*reinterpret_cast<const DWC::Reservation*>(data));
        System::sInstance->SetUserInfo(packet.pulInfo.userInfo);
        data = &packet;
        dataSize = sizeof(ResvPacket) / sizeof(u32);
    }
    SendMatchCommand(type, pid, ip, port, data, dataSize);
}
kmCall(0x800df078, Send);

static void ResetDenyCounter(UIControl* control, u32 soundId, u32 r5) {
    control->PlaySound(soundId, r5);
    if(RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_VS_REGIONAL) {
        Pulsar::System::sInstance->netMgr.deniesCount = 0;
    }
}
kmCall(0x80609110, ResetDenyCounter);

}
}


