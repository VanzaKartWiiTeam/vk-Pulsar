#ifndef _PUL_ROOMCONTEXT_
#define _PUL_ROOMCONTEXT_

#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Race/RaceData.hpp>

/*
Small shared predicates for "where are we playing right now". Several features need the same
three answers - offline, friend room, mogi - and each one used to reimplement them slightly
differently, which is how a feature ends up enabled in a room it was never meant for.
*/

namespace VanzaKart {
namespace RoomContext {

inline bool IsOnline() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr) return false;
    return controller->connectionState != RKNet::CONNECTIONSTATE_SHUTDOWN
        && controller->roomType != RKNet::ROOMTYPE_NONE;
}

inline bool IsFriendRoom() {
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr) return false;
    const RKNet::RoomType roomType = controller->roomType;
    return roomType == RKNet::ROOMTYPE_FROOM_HOST || roomType == RKNet::ROOMTYPE_FROOM_NONHOST;
}

//Everything online that is not a friend room: worldwides and regionals, VS and battle.
inline bool IsWorldwide() {
    return IsOnline() && !IsFriendRoom();
}

inline bool IsMogi() {
    const Pulsar::System* system = Pulsar::System::sInstance;
    return system != nullptr && system->IsContext(Pulsar::PULSAR_STARTMOGI);
}

/*
Where the transmission system is allowed to act: offline and in friend rooms only. Worldwides stay
vanilla because everyone there has to be racing the same vehicle physics, and mogi is excluded
outright even when hosted from a friend room.
*/
inline bool IsTransmissionAllowed() {
    if(IsMogi()) return false;
    if(!IsOnline()) return true;
    return IsFriendRoom();
}

//Players actually in the race, humans and CPUs alike.
inline u8 GetRacePlayerCount() {
    const Racedata* racedata = Racedata::sInstance;
    if(racedata == nullptr) return 0;
    return racedata->racesScenario.playerCount;
}

}  // namespace RoomContext
}  // namespace VanzaKart

#endif
