#include <VanzaKart.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>

/*The MIT License (MIT)

Copyright (c) 2024 Brawlboxgaming

Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.*/

namespace VanzaKart {
namespace Race {

// Mission Mode Fix flag - set to 1 to disable transmission system
u16 U16_MISSION_MODE_FIX = 0;

static KartType GetCustomKartType(Kart::Link* link) {
    if(U16_MISSION_MODE_FIX != 1) {
        Racedata* raceData = Racedata::sInstance;
        u8 playerId = link->GetPlayerIdx();
        u8 hudSlotId = raceData->GetHudSlotId(playerId);
        if(hudSlotId > 3) hudSlotId = playerId;
        
        // Set target angle for proper handling
        link->pointers->values->statsAndBsp.stats->targetAngle = 45;
        
        // Get transmission setting for this player
        VanzaKart::Transmission chosenType = 
            static_cast<VanzaKart::System*>(Pulsar::System::sInstance)->transmissions[hudSlotId];
        
        if(chosenType == VanzaKart::TRANSMISSION_OUTSIDE) {
            if(link->pointers->values->statsAndBsp.stats->type == KART) {
                return KART;
            }
            return OUTSIDE_BIKE;
        }
        else if(chosenType == VanzaKart::TRANSMISSION_INSIDE) {
            return INSIDE_BIKE;
        }
        
        return link->pointers->values->statsAndBsp.stats->type;
    }
    return link->pointers->values->statsAndBsp.stats->type;
}
kmBranch(0x80590a10, GetCustomKartType);

static void SetCPUKartType() {
    Random random;
    u8 localPlayerCount = GetLocalPlayerCount();
    
    for(int i = localPlayerCount; i < 12; ++i) {
        VanzaKart::System* vkSystem = static_cast<VanzaKart::System*>(Pulsar::System::sInstance);
        PlayerType playerType = Racedata::sInstance->racesScenario.players[i].playerType;
        
        if(playerType == PLAYER_CPU && vkSystem->transmissions[i] == VanzaKart::TRANSMISSION_DEFAULT) {
            // Randomly assign transmission to CPU players (Outside or Inside)
            VanzaKart::Transmission type = 
                static_cast<VanzaKart::Transmission>(random.NextLimited(2) + 1);
            vkSystem->transmissions[i] = type;
        }
    }
}
static RaceLoadHook SetCPUKartTypeHook(SetCPUKartType);

static void ResetTransmissions() {
    VanzaKart::System* vkSystem = static_cast<VanzaKart::System*>(Pulsar::System::sInstance);
    for(int i = 0; i < 12; ++i) {
        vkSystem->transmissions[i] = VanzaKart::TRANSMISSION_DEFAULT;
    }
}
kmBranch(0x80530158, ResetTransmissions);

} // namespace Race
} // namespace VanzaKart
